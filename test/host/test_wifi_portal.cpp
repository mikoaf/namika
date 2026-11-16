// Exercise the real Wi-Fi service with fake radio, HTTP and NVS adapters.
#include "../../src/Connectivity.cpp"
#include <cassert>
#include <iostream>
std::vector<unsigned char> Preferences::record;
bool Preferences::writeWorks = true;
WebServer *WebServer::active = nullptr;

static void reset(uint32_t now = 0) {
  fakeMillis = now;
  WiFi = WiFiMock{};
  Preferences::record.clear();
  Preferences::writeWorks = true;
}
static std::string status() {
  WebServer::active->handlers.at("/status")();
  return WebServer::active->body;
}
static int submit(const char *ssid, const char *password) {
  auto &server = *WebServer::active;
  server.args["ssid"] = ssid;
  server.args["password"] = password;
  server.handlers.at("/save")();
  return server.code;
}
static void advance(WifiService &service, uint32_t elapsed) {
  fakeMillis += elapsed;
  service.update();
}
static void seed() {
  wifi::Credentials credentials;
  std::strcpy(credentials.ssid, "OldHome");
  std::strcpy(credentials.password, "oldpassword");
  Preferences prefs;
  prefs.putBytes("credentials", &credentials, sizeof(credentials));
}
int main() {
  reset();
  WifiService fresh;
  fresh.begin();
  assert(WiFi.ap && status() == "idle");
  assert(submit("NewHome", "short") == 400);
  assert(submit("NewHome", "newpassword") == 202);
  assert(status() == "trying");
  advance(fresh, 500);
  assert(WiFi.ssid == "NewHome");
  assert(Preferences::record.empty());
  assert(submit("Other", "password") == 409);
  advance(fresh, 30000);
  assert(status() == "failed" && WiFi.ap);
  assert(Preferences::record.empty());
  // Even a later automatic retry must save the candidate when it succeeds.
  WiFi.connection = WL_CONNECTED;
  advance(fresh, 1);
  assert(status() == "connected" && !Preferences::record.empty());
  advance(fresh, 14999);
  assert(WiFi.ap);
  advance(fresh, 1);
  assert(!WiFi.ap);

  // Reboot loads the successful credentials instead of starting a portal.
  WiFi = WiFiMock{};
  WifiService reboot;
  reboot.begin();
  assert(WiFi.ssid == "NewHome" && !WiFi.ap);
  WiFi.connection = WL_CONNECTED;
  advance(reboot, 1);
  WiFi.connection = 0;
  advance(reboot, 1);
  advance(reboot, 29999);
  assert(!WiFi.ap);
  advance(reboot, 1);
  assert(WiFi.ap);
  WebServer::active->viaAp = false;
  assert(submit("Attack", "password") == 403);
  WebServer::active->viaAp = true;
  assert(submit("NextHome", "wrongpass") == 202);
  const auto previous = Preferences::record;
  advance(reboot, 500);
  advance(reboot, 30000);
  assert(status() == "failed" && Preferences::record == previous);
  assert(submit("NextHome", "correctpass") == 202);
  advance(reboot, 500);
  WiFi.connection = WL_CONNECTED;
  advance(reboot, 1);
  assert(status() == "connected" && Preferences::record != previous);

  reset();
  seed();
  WifiService offlineBoot;
  offlineBoot.begin();
  advance(offlineBoot, 30000);
  assert(WiFi.ap && WiFi.attempts >= 2);
  assert(submit("NextHome", "password") == 202);
  advance(offlineBoot, 500);
  Preferences::writeWorks = false;
  WiFi.connection = WL_CONNECTED;
  advance(offlineBoot, 1);
  assert(status() == "save_failed");
  advance(offlineBoot, 20000);
  assert(WiFi.ap);

  // A delayed disconnect event must not mark a new password as tested.
  reset();
  seed();
  WifiService delayed;
  delayed.begin();
  advance(delayed, 30000);
  WiFi.connection = WL_CONNECTED;
  advance(delayed, 1);
  const auto oldRecord = Preferences::record;
  WiFi.deferredDisconnect = true;
  assert(submit("AnotherHome", "password") == 202);
  advance(delayed, 500);
  advance(delayed, 1);
  assert(status() == "trying" && Preferences::record == oldRecord);
  WiFi.connection = 0; // The old connection finally ends.
  advance(delayed, 1);
  assert(WiFi.ssid == "AnotherHome" && Preferences::record == oldRecord);
  WiFi.connection = WL_CONNECTED;
  advance(delayed, 1);
  assert(status() == "connected" && Preferences::record != oldRecord);

  reset(UINT32_MAX - 10000);
  seed();
  WifiService rollover;
  rollover.begin();
  advance(rollover, 29999);
  assert(!WiFi.ap);
  advance(rollover, 1);
  assert(WiFi.ap);

  reset();
  WiFi.apWorks = false;
  WifiService apFailure;
  apFailure.begin();
  assert(!WiFi.ap);
  WiFi.apWorks = true;
  advance(apFailure, 30000);
  assert(WiFi.ap);
  std::cout << "Wi-Fi portal lifecycle tests passed\n";
}
