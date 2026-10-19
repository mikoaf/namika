#pragma once
#include "Connectivity.h"
#include "FaceDisplay.h"
#include "IrrigationService.h"
#include "PlantMonitor.h"
#include "SensorService.h"

// Composition and scheduling only. Hardware and business logic live in modules.
class NamikaApp {
 public:
  void begin();
  void update();
 private:
  Connectivity connectivity_;
  SensorService sensors_;
  plant::Monitor monitor_;
  IrrigationService irrigation_;
  FaceDisplay face_;
  uint32_t lastPublish_ = 0;
  void handleCommands(uint32_t now);
  void publishMonitoring(uint32_t now);
  void logSample() const;
};
