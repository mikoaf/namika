#include "IrrigationService.h"
#include <Arduino.h>

void IrrigationService::begin() {
  digitalWrite(config::PUMP_PIN, config::PUMP_ACTIVE_LEVEL == HIGH ? LOW : HIGH);
  pinMode(config::PUMP_PIN, OUTPUT);
}

void IrrigationService::writePump() {
  digitalWrite(config::PUMP_PIN, controller_.pumping() ? config::PUMP_ACTIVE_LEVEL :
               (config::PUMP_ACTIVE_LEVEL == HIGH ? LOW : HIGH));
}

void IrrigationService::update(uint32_t now, bool inputUsable) {
  const bool wasPumping = controller_.pumping();
  controller_.tick(now, inputUsable);
  writePump();
  if (wasPumping != controller_.pumping())
    Serial.printf("Watering: %s\n", controller_.status());
}

void IrrigationService::sample(uint32_t now, float soil, bool inputUsable) {
  controller_.sample(now, soil, inputUsable, config::AUTO_WATERING_ENABLED);
  writePump();
}

void IrrigationService::command(const remote::Command &command, uint32_t now,
                                uint32_t epoch, float soil, bool inputUsable) {
  lastCommandId_ = command.id;
  if (epoch < 1704067200 || epoch >= command.expiresAt) {
    lastCommandResult_ = "expired";
  } else if (command.action == remote::Action::Stop) {
    controller_.stop(now);
    lastCommandResult_ = "stopped";
  } else {
    lastCommandResult_ = controller_.requestManual(now, soil, inputUsable);
  }
  writePump();
  Serial.printf("Cloud command %lu: %s\n", (unsigned long)lastCommandId_, lastCommandResult_);
}

const char *IrrigationService::stateName() const {
  switch (controller_.state()) {
    case watering::State::Watering: return "watering";
    case watering::State::Soaking: return "soaking";
    case watering::State::Cooldown: return "cooldown";
    case watering::State::Locked: return "locked";
    default: return "monitoring";
  }
}
