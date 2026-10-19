#include "Connectivity.h"
#include "AppConfig.h"
#include <WiFi.h>
#include <time.h>
#include <cstring>

void Connectivity::begin() {
  configured_ = strlen(config::WIFI_SSID) > 0 &&
                strcmp(config::WIFI_SSID, "YourWiFiSSID") != 0;
  if (configured_) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);
  }
  configTime(config::GMT_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com");
}

void Connectivity::update(uint32_t now) {
  if (!configured_ || WiFi.status() == WL_CONNECTED) return;
  if (uint32_t(now - lastAttempt_) >= config::WIFI_RETRY_MS) {
    lastAttempt_ = now;
    WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);
  }
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
