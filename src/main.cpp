#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <time.h>
#include "CactusMood.h"
#include "EyeRenderer.h"

// Replace credentials to enable network time; monitoring also works offline.
const char *ssid = "YourWiFiSSID";
const char *password = "YourWiFiPassword";
constexpr int SOIL_SENSOR_PIN = 34;  // ADC1; move sensor output from GPIO5.
constexpr int TOUCH_SENSOR_PIN = 27;  // Digital touch-module OUT; avoid UART RX.
constexpr int TOUCH_ACTIVE_LEVEL = HIGH;  // Change to LOW for active-low modules.
constexpr int PUMP_PIN = 4;
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
uint32_t lastWiFiAttempt = 0, lastSensorRetry = 0;

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
  updateSleep(now);
  if (sensorsValid && samples.count() == WINDOW_SAMPLES)
    mood = cactus::evaluate(samples.mean(), sleeping, wetTooLong, SOIL_CALIBRATED);
  else
    mood = {2, 2, 2, 2, 2, sensorsValid ? "Mengumpulkan data" : "Cek sensor"};
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
  if (!sensorsValid) display.print("Cek sensor");
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
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);  // Monitoring only; no automatic watering.
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
}

void loop() {
  const uint32_t now = millis();
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
  delay(5);
}
