#pragma once
#include <cstdint>
namespace config {
constexpr const char *WIFI_SSID = "";
constexpr const char *WIFI_PASSWORD = "";
constexpr const char *WIFI_SETUP_PASSWORD = "namika123";
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t WIFI_PORTAL_CLOSE_MS = 15000;
constexpr long GMT_OFFSET_SECONDS = 7 * 3600;
}
