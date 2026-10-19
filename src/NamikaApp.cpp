#include "NamikaApp.h"
#include "AppConfig.h"
#include "MqttMonitoring.h"
#include <Arduino.h>
#include <Wire.h>

void NamikaApp::begin() {
  Serial.begin(115200);
  irrigation_.begin();
  Wire.begin(config::I2C_SDA, config::I2C_SCL);
  sensors_.begin();
  connectivity_.begin();
  const uint32_t now = millis();
  monitor_.begin(now);  // Allow the first BH1750 measurement to complete.
  face_.begin(now);
  face_.update(millis(), monitor_.status(), irrigation_.controller());
  Serial.println("NaMiKa: soil input GPIO34; calibrate dry/wet ADC endpoints.");
  const remote::Config mqttConfig{config::MQTT_HOST, config::MQTT_PORT, config::MQTT_USERNAME,
      config::MQTT_PASSWORD, config::MQTT_ROOT_CA, config::MQTT_DEVICE_ID};
  if (!remote::begin(mqttConfig))
    Serial.println("Cloud initialization failed; offline monitoring continues");
  publishMonitoring(millis());
}

void NamikaApp::handleCommands(uint32_t now) {
  remote::Command command;
  if (!remote::takeCommand(command)) return;
  const auto &plant = monitor_.status();
  irrigation_.command(command, now, connectivity_.epoch(), plant.latest.soil,
                      plant.wateringInputUsable(now));
}

void NamikaApp::publishMonitoring(uint32_t now) {
  const auto &plant = monitor_.status();
  const auto &watering = irrigation_.controller();
  const remote::Snapshot snapshot{
      now, uint32_t(now - plant.lastSample), irrigation_.lastCommandId(),
      plant.latest.temperature, plant.latest.humidity, plant.latest.lux, plant.latest.soil,
      plant.soilRaw, unsigned(plant.sampleCount), unsigned(plant.mood.overall), unsigned(watering.pulses()),
      plant.sensorsValid && uint32_t(now - plant.lastSample) < 2 * config::SAMPLE_MS,
      plant.soilCalibrated, config::AUTO_WATERING_ENABLED, watering.pumping(),
      irrigation_.stateName(), irrigation_.lastCommandResult()};
  remote::publish(snapshot);
}

void NamikaApp::update() {
  uint32_t now = millis();
  // Pump deadlines and incoming commands take priority over maintenance.
  irrigation_.update(now, monitor_.status().wateringInputUsable(now));
  handleCommands(now);
  connectivity_.update(now);
  sensors_.update(now);
  now = millis();
  if (uint32_t(now - monitor_.status().lastSample) >= config::SAMPLE_MS) {
    const auto sample = sensors_.read();
    const auto clock = connectivity_.clock();
    monitor_.sample(sample, now, clock.valid, clock.hour);
    const auto &plant = monitor_.status();
    irrigation_.sample(now, plant.latest.soil, plant.wateringInputUsable(now));
    logSample();
  }
  now = millis();
  irrigation_.update(now, monitor_.status().wateringInputUsable(now));
  face_.update(now, monitor_.status(), irrigation_.controller());
  now = millis();
  if (uint32_t(now - lastPublish_) >= config::SNAPSHOT_MS) {
    lastPublish_ = now;
    publishMonitoring(now);
  }
}

void NamikaApp::logSample() const {
  const auto &plant = monitor_.status();
  const auto &watering = irrigation_.controller();
  const auto &mood = plant.mood;
  Serial.printf("Watering: %s pulses=%u enabled=%d\n", watering.status(),
                unsigned(watering.pulses()), config::AUTO_WATERING_ENABLED && plant.soilCalibrated);
  Serial.printf("raw=%d soil=%.1f%% T=%.1fC RH=%.1f%% lux=%.0f "
                "levels[T,H,L,S]=%u,%u,%u,%u mood=%u sleep=%d "
                "valid=%d samples=%u/%u calibrated=%d reason=%s\n",
                plant.soilRaw, plant.latest.soil, plant.latest.temperature, plant.latest.humidity,
                plant.latest.lux, mood.temperature, mood.humidity, mood.light, mood.soil,
                mood.overall, plant.sleeping, plant.sensorsValid, unsigned(plant.sampleCount),
                unsigned(config::WINDOW_SAMPLES), plant.soilCalibrated, mood.reason);
}
