#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <time.h>
#include "CactusMood.h"
#include "WateringController.h"
#include "MqttMonitoring.h"
#include "EyeRenderer.h"

// Replace credentials to enable network time; monitoring also works offline.
const char *ssid = "YourWiFiSSID";
const char *password = "YourWiFiPassword";
// MQTT cloud is optional until a broker is configured. Host has no URL scheme.
const char *MQTT_HOST = "";  // e.g. your-cluster.example.com
constexpr uint16_t MQTT_PORT = 8883;
const char *MQTT_USERNAME = "";
const char *MQTT_PASSWORD = "";
const char *MQTT_ROOT_CA = "";  // PEM root CA matching the broker certificate.
const char *MQTT_DEVICE_ID = "namika-01";  // Unique per device; letters/digits/-/_.
constexpr int SOIL_SENSOR_PIN = 34;  // ADC1; move sensor output from GPIO5.
constexpr int TOUCH_SENSOR_PIN = 27;  // Digital touch-module OUT; avoid UART RX.
constexpr int TOUCH_ACTIVE_LEVEL = HIGH;  // Change to LOW for active-low modules.
constexpr int PUMP_PIN = 4;
constexpr int PUMP_ACTIVE_LEVEL = HIGH;  // LOW for active-low relay modules.
constexpr bool AUTO_WATERING_ENABLED = true;  // Also requires soil calibration.
// Experimental settings; adjust pulse duration to measured pump flow/pot size.
const watering::Config WATERING_CONFIG{};  // 15/25%, 1s pulse, 5min soak, max 3.
watering::Controller irrigation(WATERING_CONFIG);
constexpr int I2C_SDA = 21, I2C_SCL = 22;
constexpr int SOIL_RAW_DRY = 3000;  // EXAMPLE: measure in your dry medium.
constexpr int SOIL_RAW_WET = 1300;  // EXAMPLE: after watering and draining.
constexpr bool SOIL_CALIBRATED = false;  // Set true after measuring endpoints.
constexpr uint32_t SAMPLE_MS = 5000;
constexpr size_t WINDOW_SAMPLES = 60;  // Five minutes of samples.
constexpr uint32_t WET_WARNING_MS = 48UL * 60 * 60 * 1000;
constexpr int SLEEP_HOUR = 20, WAKE_HOUR = 6;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_BME280 bme;
BH1750 lightMeter;
cactus::AverageWindow<WINDOW_SAMPLES> samples;
cactus::HeldCondition wetCondition;
eyes::SleepDetector sleepDetector;
eyes::TouchDebouncer touchDebouncer;
eyes::Controller animation;
EyeRenderer eyeRenderer;
cactus::Reading latest{};
cactus::Mood mood{2, 2, 2, 2, 2, "Mengumpulkan data"};
bool oledReady = false, bmeReady = false, lightReady = false;
bool sensorsValid = false, sleeping = false, wetTooLong = false;
bool clockReady = false, wifiConfigured = false;
int soilRaw = 0;
uint32_t lastSample = 0, lastDisplay = 0;
uint32_t lastWiFiAttempt = 0, lastSensorRetry = 0, lastPublish = 0;
uint32_t lastCommandId = 0;
const char *lastCommandResult = "none";

bool wateringInputUsable(uint32_t now) {
  return SOIL_CALIBRATED && sensorsValid &&
         soilRaw > 0 && soilRaw < 4095 && uint32_t(now - lastSample) < 2 * SAMPLE_MS;
}

void writePump() {
  digitalWrite(PUMP_PIN, irrigation.pumping() ? PUMP_ACTIVE_LEVEL :
               (PUMP_ACTIVE_LEVEL == HIGH ? LOW : HIGH));
}

const char *wateringStateName() {
  switch (irrigation.state()) {
    case watering::State::Watering: return "watering";
    case watering::State::Soaking: return "soaking";
    case watering::State::Cooldown: return "cooldown";
    case watering::State::Locked: return "locked";
    default: return "monitoring";
  }
}

void handleRemoteCommands(uint32_t now) {
  remote::Command command;
  if (!remote::takeCommand(command)) return;
  lastCommandId = command.id;
  if (time(nullptr) < 1704067200 || uint32_t(time(nullptr)) >= command.expiresAt) {
    lastCommandResult = "expired";
  } else if (command.action == remote::Action::Stop) {
    irrigation.stop(now);
    lastCommandResult = "stopped";
  } else {
    lastCommandResult = irrigation.requestManual(now, latest.soil, wateringInputUsable(now));
  }
  writePump();
  Serial.printf("Cloud command %lu: %s\n", (unsigned long)lastCommandId, lastCommandResult);
}

void publishMonitoring(uint32_t now) {
  const remote::Snapshot snapshot{
      now, uint32_t(now - lastSample), lastCommandId,
      latest.temperature, latest.humidity, latest.lux, latest.soil, soilRaw,
      unsigned(samples.count()), unsigned(mood.overall), unsigned(irrigation.pulses()),
      sensorsValid && uint32_t(now - lastSample) < 2 * SAMPLE_MS,
      SOIL_CALIBRATED, AUTO_WATERING_ENABLED, irrigation.pumping(),
      wateringStateName(), lastCommandResult};
  remote::publish(snapshot);
}

void maintainWiFi(uint32_t now) {
  if (!wifiConfigured || WiFi.status() == WL_CONNECTED) return;
  
  if (uint32_t(now - lastWiFiAttempt) >= 30000) {
    lastWiFiAttempt = now;
    WiFi.begin(ssid, password);
  }
}

void updateSleep(uint32_t now) {
  const time_t current = time(nullptr);
  tm local{};
  localtime_r(&current, &local);
  clockReady = local.tm_year >= 124;  // Synchronized date, 2024 or later.
  const bool lightValid = lightReady && isfinite(latest.lux) && latest.lux >= 0;
  sleeping = sleepDetector.update(clockReady, local.tm_hour, lightValid,
                                  latest.lux, now, SLEEP_HOUR, WAKE_HOUR);
}

void sampleSensors(uint32_t now) {
  soilRaw = analogRead(SOIL_SENSOR_PIN);
  latest.temperature = bmeReady ? bme.readTemperature() : NAN;
  latest.humidity = bmeReady ? bme.readHumidity() : NAN;
  latest.lux = lightReady ? lightMeter.readLightLevel() : NAN;
  latest.soil = cactus::soilPercent(soilRaw, SOIL_RAW_DRY, SOIL_RAW_WET);
  sensorsValid = cactus::valid(latest);
  if (!sensorsValid) {
    // Discard stale averages and wet-duration evidence across an outage.
    samples.clear();
    wetTooLong = wetCondition.update(false, now, WET_WARNING_MS);
  } else {
    samples.add(latest);
    wetTooLong = wetCondition.update(SOIL_CALIBRATED && latest.soil > 60,
                                     now, WET_WARNING_MS);
  }
  irrigation.sample(now, latest.soil, wateringInputUsable(now), AUTO_WATERING_ENABLED);
  writePump();
  updateSleep(now);
  if (sensorsValid && samples.count() == WINDOW_SAMPLES)
    mood = cactus::evaluate(samples.mean(), sleeping, wetTooLong, SOIL_CALIBRATED);
  else
    mood = {2, 2, 2, 2, 2, sensorsValid ? "Mengumpulkan data" : "Cek sensor"};
  Serial.printf("Watering: %s pulses=%u enabled=%d\n", irrigation.status(),
                unsigned(irrigation.pulses()), AUTO_WATERING_ENABLED && SOIL_CALIBRATED);
  Serial.printf("raw=%d soil=%.1f%% T=%.1fC RH=%.1f%% lux=%.0f "
                "levels[T,H,L,S]=%u,%u,%u,%u mood=%u sleep=%d "
                "valid=%d samples=%u/%u calibrated=%d reason=%s\n",
                soilRaw, latest.soil, latest.temperature, latest.humidity,
                latest.lux, mood.temperature, mood.humidity, mood.light,
                mood.soil, mood.overall, sleeping, sensorsValid,
                unsigned(samples.count()), unsigned(WINDOW_SAMPLES),
                SOIL_CALIBRATED, mood.reason);
}

void updateDisplay(uint32_t now) {
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("NaMiKa");
  display.setCursor(80, 0);
  display.print(eyes::name(animation.mode()));
  eyeRenderer.draw(display, animation.mode(), animation.elapsed(now), now);
  display.setCursor(0, 56);
  const cactus::Reading values = samples.count() ? samples.mean() : latest;
  // Keep diagnostics visible even while touch or sleep overrides the face.
  if (irrigation.state() == watering::State::Locked || irrigation.pumping() ||
      irrigation.state() == watering::State::Soaking) display.print(irrigation.status());
  else if (!sensorsValid) display.print("Cek sensor");
  else if (!SOIL_CALIBRATED && mood.overall != 1) display.print("Kalibrasi tanah");
  else if (samples.count() < WINDOW_SAMPLES)
    display.printf("Data %u/%u", unsigned(samples.count()), unsigned(WINDOW_SAMPLES));
  else if ((now / 4000) % 2 == 0) display.print(mood.reason);
  else display.printf("T:%.0fC H:%.0f%% S:%.0f%%", values.temperature,
                      values.humidity, values.soil);
  display.display();
}

void setup() {
  Serial.begin(115200);
  digitalWrite(PUMP_PIN, PUMP_ACTIVE_LEVEL == HIGH ? LOW : HIGH);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(TOUCH_SENSOR_PIN, TOUCH_ACTIVE_LEVEL == HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);
  Wire.begin(I2C_SDA, I2C_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledReady) Serial.println("OLED initialization failed");
  bmeReady = bme.begin(0x76);
  if (!bmeReady) bmeReady = bme.begin(0x77);
  lightReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  wifiConfigured = strlen(ssid) > 0 && strcmp(ssid, "YourWiFiSSID") != 0;
  if (wifiConfigured) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  }
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");  // WIB (UTC+7).
  Serial.println("NaMiKa: soil input GPIO34; calibrate dry/wet ADC endpoints.");
  // Allow the BH1750 first high-resolution measurement to complete.
  lastSample = millis();
  animation.begin(lastSample);
  updateDisplay(lastSample);
  const remote::Config mqttConfig{MQTT_HOST, MQTT_PORT, MQTT_USERNAME,
                                  MQTT_PASSWORD, MQTT_ROOT_CA, MQTT_DEVICE_ID};
  if (!remote::begin(mqttConfig))
    Serial.println("Cloud initialization failed; offline monitoring continues");
  publishMonitoring(millis());
}

void loop() {
  const uint32_t now = millis();
  // Service pump deadlines and commands before slower sensor/network maintenance.
  irrigation.tick(now, wateringInputUsable(now));
  writePump();
  handleRemoteCommands(now);
  maintainWiFi(now);
  if (uint32_t(now - lastSensorRetry) >= 30000) {
    lastSensorRetry = now;
    if (!bmeReady) {
      bmeReady = bme.begin(0x76);
      if (!bmeReady) bmeReady = bme.begin(0x77);
    }
    if (!lightReady) lightReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  }
  if (uint32_t(now - lastSample) >= SAMPLE_MS) {
    lastSample = now;
    sampleSensors(now);
  }
  const bool wasPumping = irrigation.pumping();
  irrigation.tick(now, wateringInputUsable(now));
  writePump();
  if (wasPumping != irrigation.pumping())
    Serial.printf("Watering: %s\n", irrigation.status());
  const bool touched = touchDebouncer.update(
      digitalRead(TOUCH_SENSOR_PIN) == TOUCH_ACTIVE_LEVEL, now);
  const bool environmentKnown = sensorsValid && samples.count() == WINDOW_SAMPLES &&
                                 (SOIL_CALIBRATED || mood.overall == 1);
  const eyes::Animation previous = animation.mode();
  animation.update(now, sleeping, touched, environmentKnown, mood.overall);
  if (animation.mode() != previous)
    Serial.printf("Animation: %s\n", eyes::name(animation.mode()));
  if (uint32_t(now - lastDisplay) >= 40) {
    lastDisplay = now;
    updateDisplay(now);
  }
  if (uint32_t(millis() - lastPublish) >= 250) {
    lastPublish = millis();
    publishMonitoring(lastPublish);
  }
  delay(5);
}
