#pragma once
#include "Arduino.h"
struct IPAddress {
  uint32_t value;
  IPAddress(int a = 0, int b = 0, int c = 0, int d = 0)
      : value(uint32_t(a) << 24 | uint32_t(b) << 16 | uint32_t(c) << 8 | uint32_t(d)) {}
  bool operator==(const IPAddress &other) const { return value == other.value; }
};
constexpr int WIFI_AUTH_OPEN = 0, WIFI_STA = 1, WIFI_AP_STA = 3, WL_CONNECTED = 3;
struct WiFiMock {
  bool ap = false, apWorks = true, deferredDisconnect = false;
  int connection = 0, attempts = 0;
  std::string ssid;
  void persistent(bool) {}
  void mode(int) {}
  void setMinSecurity(int) {}
  void setAutoReconnect(bool) {}
  bool disconnect(bool, bool) { if (!deferredDisconnect) connection = 0; return true; }
  void begin(const char *name, const char *) { ssid = name; ++attempts; }
  int status() const { return connection; }
  IPAddress localIP() { return IPAddress(10, 0, 0, 2); }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  bool softAPConfig(IPAddress, IPAddress, IPAddress) { return true; }
  bool softAP(const char *, const char *) { ap = apWorks; return ap; }
  void softAPdisconnect(bool) { ap = false; }
};
static WiFiMock WiFi;
