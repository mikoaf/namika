#include "Connectivity.h"
#include "AppConfig.h"
#include "WifiCredentials.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cstring>
#include <time.h>

namespace {
using wifi::Credentials;

bool validCredentials(const String &ssid, const String &password) {
  return wifi::validCredentials(ssid.c_str(), ssid.length(), password.c_str(), password.length());
}

const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="id"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi NaMiKa</title><style>
body{font:16px system-ui;max-width:420px;margin:40px auto;padding:0 20px;background:#f3f7f3;color:#163b27}
input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0 18px;font:inherit}
button{background:#216944;color:white;border:0;border-radius:8px}p{line-height:1.5}
</style><h1>Wi-Fi NaMiKa</h1><p>Masukkan Wi-Fi 2,4 GHz di lokasi ini. Kosongkan password untuk jaringan terbuka.</p>
<form id="wifi" method="post" action="/save"><label>Nama Wi-Fi (SSID)<input name="ssid" required maxlength="32" autocomplete="off"></label>
<label>Password Wi-Fi<input name="password" type="password" maxlength="64" autocomplete="new-password"></label>
<button id="connect">Hubungkan dan simpan</button></form>
<p id="status" role="status">Siap mengatur Wi-Fi.</p>
<p>Jika hotspot sempat terputus saat mencoba jaringan baru, sambungkan kembali ke hotspot NaMiKa dan buka http://192.168.4.1.</p>
<script>
const form=document.getElementById('wifi'),statusEl=document.getElementById('status'),button=document.getElementById('connect');
const messages={idle:'Siap mengatur Wi-Fi.',trying:'Mencoba koneksi, tunggu hingga 30 detik...',
failed:'Koneksi gagal. Periksa SSID/password dan coba lagi. Pengaturan lama tetap tersimpan.',
connected:'Terhubung! Wi-Fi sudah tersimpan. Hotspot akan ditutup dalam 15 detik.',
save_failed:'Wi-Fi terhubung, tetapi gagal disimpan. Masukkan ulang SSID/password lalu coba lagi.'};
async function poll(){try{const r=await fetch('/status',{cache:'no-store'});if(!r.ok)return;
const s=await r.text();statusEl.textContent=messages[s]||s;button.disabled=s==='trying'||s==='connected';}catch(e){}}
form.addEventListener('submit',async e=>{e.preventDefault();button.disabled=true;
try{const r=await fetch('/save',{method:'POST',body:new URLSearchParams(new FormData(form))});
statusEl.textContent=await r.text();form.elements.password.value='';if(!r.ok)button.disabled=false;
}catch(e){statusEl.textContent='Sambungkan kembali ke hotspot NaMiKa lalu periksa status.';button.disabled=false;}});
setInterval(poll,2000);poll();
</script></html>)HTML";

// All mutable network/portal state belongs to this task, never the pump loop.
class WifiService {
 public:
  void begin() {
    WiFi.persistent(false);  // Only commit tested credentials through Preferences.
    WiFi.mode(WIFI_STA);
    WiFi.setMinSecurity(WIFI_AUTH_OPEN);  // Also allow explicitly chosen open networks.
    WiFi.setAutoReconnect(false);  // Retry timing is managed below.
    Preferences prefs;
    if (prefs.begin("namika-wifi", true)) {
      Credentials stored;
      if (prefs.getBytesLength("credentials") == sizeof(stored) &&
          prefs.getBytes("credentials", &stored, sizeof(stored)) == sizeof(stored) &&
          wifi::validStoredCredentials(stored)) credentials_ = stored;
      prefs.end();
    }
    if (!credentials_.ssid[0] && std::strcmp(config::WIFI_SSID, "YourWiFiSSID") != 0 &&
        validCredentials(config::WIFI_SSID, config::WIFI_PASSWORD)) {
      strlcpy(credentials_.ssid, config::WIFI_SSID, sizeof(credentials_.ssid));
      strlcpy(credentials_.password, config::WIFI_PASSWORD, sizeof(credentials_.password));
    }
    server_.on("/", HTTP_GET, [this]() {
      if (!allowRequest()) return;
      server_.sendHeader("Cache-Control", "no-store");
      server_.send_P(200, "text/html; charset=utf-8", SETUP_PAGE);
    });
    server_.on("/status", HTTP_GET, [this]() {
      if (!allowRequest()) return;
      server_.sendHeader("Cache-Control", "no-store");
      server_.send(200, "text/plain", status_);
    });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    server_.onNotFound([this]() {
      if (!allowRequest()) return;
      server_.sendHeader("Location", "http://192.168.4.1/", true);
      server_.send(302, "text/plain", "Buka http://192.168.4.1/");
    });
    offlineSince_ = millis();
    portalAttempt_ = offlineSince_;
    if (credentials_.ssid[0]) connect();
    else startPortal();
    configTime(config::GMT_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com");
  }

  void update() {
    // A slow HTTP client can block this task, but cannot hold up pump deadlines.
    if (portal_) {
      dns_.processNextRequest();
      server_.handleClient();
    }
    const uint32_t now = millis();
    if (pending_) {
      if (uint32_t(now - submittedAt_) < 500) return; // Let the HTTP response leave.
      pending_ = false;
      testing_ = true;
      uncommitted_ = true;
      credentials_ = candidate_;
      connect();
      return;
    }
    if (disconnecting_) {
      // The ESP32 disconnect event is asynchronous. Do not mistake the old
      // router connection for a successful test of newly submitted credentials.
      if (WiFi.status() == WL_CONNECTED) {
        if (uint32_t(now - lastAttempt_) >= config::WIFI_RETRY_MS) connect();
        return;
      }
      disconnecting_ = false;
      WiFi.begin(credentials_.ssid, credentials_.password);
      lastAttempt_ = millis();
      return;
    }
    if (WiFi.status() == WL_CONNECTED) {
      if (!connected_) {
        connected_ = true;
        Serial.print("Wi-Fi connected; IP: ");
        Serial.println(WiFi.localIP());
        bool saved = true;
        if (uncommitted_) {
          Preferences prefs;
          saved = prefs.begin("namika-wifi", false);
          if (saved) {
            saved = prefs.putBytes("credentials", &credentials_, sizeof(credentials_)) == sizeof(credentials_);
            prefs.end();
          }
          uncommitted_ = !saved;
        }
        testing_ = false;
        status_ = saved ? "connected" : "save_failed";
        connectedAt_ = now;
      }
      offlineSince_ = now;
      if (portal_ && std::strcmp(status_, "connected") == 0 &&
          uint32_t(now - connectedAt_) >= config::WIFI_PORTAL_CLOSE_MS) stopPortal();
      return;
    }
    if (connected_) {
      connected_ = false;
      offlineSince_ = now;
      lastAttempt_ = now;
      status_ = "idle";
    }
    if (!portal_ && uint32_t(now - offlineSince_) >= config::WIFI_RETRY_MS &&
        uint32_t(now - portalAttempt_) >= config::WIFI_RETRY_MS) startPortal();
    if (uint32_t(now - lastAttempt_) >= config::WIFI_RETRY_MS) {
      if (testing_) {
        testing_ = false;
        status_ = "failed";
      }
      if (credentials_.ssid[0]) connect();
    }
  }

 private:
  WebServer server_{80};
  DNSServer dns_;
  Credentials credentials_, candidate_;
  bool portal_ = false, connected_ = false, pending_ = false, testing_ = false;
  bool uncommitted_ = false, disconnecting_ = false;
  uint32_t lastAttempt_ = 0, offlineSince_ = 0, connectedAt_ = 0;
  uint32_t submittedAt_ = 0, portalAttempt_ = 0;
  const char *status_ = "idle";

  bool allowRequest() {
    // Setup endpoints are accessible only through the password-protected hotspot.
    if (portal_ && server_.client().localIP() == WiFi.softAPIP()) return true;
    server_.send(403, "text/plain", "Gunakan hotspot pengaturan NaMiKa.");
    return false;
  }

  void handleSave() {
    if (!allowRequest()) return;
    server_.sendHeader("Cache-Control", "no-store");
    if (pending_ || testing_) {
      server_.send(409, "text/plain", "Masih mencoba koneksi. Tunggu hasilnya.");
      return;
    }
    const String ssid = server_.arg("ssid"), password = server_.arg("password");
    if (!validCredentials(ssid, password)) {
      server_.send(400, "text/plain; charset=utf-8",
          "SSID harus 1-32 byte. Password kosong, 8-63 karakter, atau 64 digit heksadesimal.");
      return;
    }
    candidate_ = Credentials{};
    ssid.toCharArray(candidate_.ssid, sizeof(candidate_.ssid));
    password.toCharArray(candidate_.password, sizeof(candidate_.password));
    status_ = "trying";
    pending_ = true;
    submittedAt_ = millis();
    server_.send(202, "text/plain", "Mencoba Wi-Fi baru. Tunggu hingga 30 detik...");
  }

  void connect() {
    connected_ = false;
    WiFi.disconnect(false, false);
    disconnecting_ = WiFi.status() == WL_CONNECTED;
    if (!disconnecting_) WiFi.begin(credentials_.ssid, credentials_.password);
    lastAttempt_ = millis();
  }

  void startPortal() {
    portalAttempt_ = millis();
    WiFi.mode(WIFI_AP_STA);
    const IPAddress ip(192, 168, 4, 1);
    char name[32];
    snprintf(name, sizeof(name), "NaMiKa-Setup-%06X", unsigned(ESP.getEfuseMac() & 0xffffff));
    if (!WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0)) ||
        !WiFi.softAP(name, config::WIFI_SETUP_PASSWORD)) {
      Serial.println("Wi-Fi setup hotspot failed; will retry.");
      WiFi.softAPdisconnect(true);
      return;
    }
    dns_.start(53, "*", ip);
    server_.begin();
    portal_ = true;
    Serial.printf("Wi-Fi setup: %s -> http://192.168.4.1/\n", name);
  }

  void stopPortal() {
    server_.stop();
    dns_.stop();
    WiFi.softAPdisconnect(true);
    portal_ = false;
    Serial.println("Wi-Fi setup hotspot closed.");
  }
};

void wifiTask(void *) {
  static WifiService service;
  service.begin();
  for (;;) {
    service.update();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
} // namespace

void Connectivity::begin() {
  if (xTaskCreate(wifiTask, "namika-wifi", 8192, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Wi-Fi task could not start; restart device. Offline monitoring continues.");
}

LocalClock Connectivity::clock() const {
  const time_t current = time(nullptr);
  tm local{};
  localtime_r(&current, &local);
  LocalClock result;
  result.valid = local.tm_year >= 124;
  result.hour = local.tm_hour;
  return result;
}

uint32_t Connectivity::epoch() const {
  return uint32_t(time(nullptr));
}
