#pragma once
#include "AppConfig.h"
#include "CloudCommand.h"

// Owns the pump GPIO. All methods are called only from the Arduino loop.
class IrrigationService {
 public:
  void begin();
  void update(uint32_t now, bool inputUsable);
  void sample(uint32_t now, float soil, bool inputUsable);
  void command(const remote::Command &command, uint32_t now, uint32_t epoch,
               float soil, bool inputUsable);
  const watering::Controller &controller() const { return controller_; }
  const char *stateName() const;
  uint32_t lastCommandId() const { return lastCommandId_; }
  const char *lastCommandResult() const { return lastCommandResult_; }
 private:
  watering::Controller controller_{config::WATERING_CONFIG};
  uint32_t lastCommandId_ = 0;
  const char *lastCommandResult_ = "none";
  void writePump();
};
