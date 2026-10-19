#pragma once
#include <cstddef>
#include <cstdint>
#include "WateringController.h"

namespace config {
// Replace credentials to enable network time; monitoring also works offline.
constexpr const char *WIFI_SSID = "YourWiFiSSID";
constexpr const char *WIFI_PASSWORD = "YourWiFiPassword";
// MQTT cloud is optional until a broker is configured. Host has no URL scheme.
constexpr const char *MQTT_HOST = "";  // e.g. your-cluster.example.com
constexpr uint16_t MQTT_PORT = 8883;
constexpr const char *MQTT_USERNAME = "";
constexpr const char *MQTT_PASSWORD = "";
constexpr const char *MQTT_ROOT_CA = "";  // PEM root CA matching the broker certificate.
constexpr const char *MQTT_DEVICE_ID = "namika-01";  // Unique per device; letters/digits/-/_.
constexpr int SOIL_SENSOR_PIN = 34;  // ADC1; move sensor output from GPIO5.
constexpr int TOUCH_SENSOR_PIN = 27;  // Digital touch-module OUT; avoid UART RX.
constexpr int TOUCH_ACTIVE_LEVEL = 1;  // 1 = HIGH, 0 = LOW for active-low modules.
constexpr int PUMP_PIN = 4;
constexpr int PUMP_ACTIVE_LEVEL = 1;  // 1 = HIGH, 0 = LOW for active-low relay modules.
constexpr bool AUTO_WATERING_ENABLED = true;  // Also requires soil calibration.
// Experimental settings; adjust pulse duration to measured pump flow/pot size.
const watering::Config WATERING_CONFIG{};  // 15/25%, 1s pulse, 5min soak, max 3.
constexpr int I2C_SDA = 21, I2C_SCL = 22;
constexpr int SOIL_RAW_DRY = 3000;  // EXAMPLE: measure in your dry medium.
constexpr int SOIL_RAW_WET = 1300;  // EXAMPLE: after watering and draining.
constexpr bool SOIL_CALIBRATED = false;  // Set true after measuring endpoints.
constexpr uint32_t SAMPLE_MS = 5000;
constexpr size_t WINDOW_SAMPLES = 60;  // Five minutes of samples.
constexpr uint32_t WET_WARNING_MS = 48UL * 60 * 60 * 1000;
constexpr int SLEEP_HOUR = 20, WAKE_HOUR = 6;

constexpr uint32_t DISPLAY_MS = 40;
constexpr uint32_t SNAPSHOT_MS = 250;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t SENSOR_RETRY_MS = 30000;
constexpr int OLED_WIDTH = 128, OLED_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr long GMT_OFFSET_SECONDS = 7 * 3600;  // WIB.
}  // namespace config
