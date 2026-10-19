#include "SensorService.h"
#include "AppConfig.h"
#include <Arduino.h>

void SensorService::initializeMissing() {
  if (!bmeReady_) {
    bmeReady_ = bme_.begin(0x76);
    if (!bmeReady_) bmeReady_ = bme_.begin(0x77);
  }
  if (!lightReady_) lightReady_ = lightMeter_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
}

void SensorService::begin() {
  pinMode(config::SOIL_SENSOR_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(config::SOIL_SENSOR_PIN, ADC_11db);
  initializeMissing();
}

void SensorService::update(uint32_t now) {
  if (uint32_t(now - lastRetry_) < config::SENSOR_RETRY_MS) return;
  lastRetry_ = now;
  initializeMissing();
}

SensorSample SensorService::read() {
  SensorSample sample;
  sample.soilRaw = analogRead(config::SOIL_SENSOR_PIN);
  sample.values.temperature = bmeReady_ ? bme_.readTemperature() : NAN;
  sample.values.humidity = bmeReady_ ? bme_.readHumidity() : NAN;
  sample.values.lux = lightReady_ ? lightMeter_.readLightLevel() : NAN;
  sample.values.soil = cactus::soilPercent(sample.soilRaw, config::SOIL_RAW_DRY,
                                          config::SOIL_RAW_WET);
  sample.lightValid = lightReady_ && std::isfinite(sample.values.lux) && sample.values.lux >= 0;
  return sample;
}
