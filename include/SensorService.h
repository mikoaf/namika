#pragma once
#include <HDC1080.h>
#include <BH1750.h>
#include "SensorSample.h"

// Owns sensor hardware; callers schedule reads and supply timestamps.
class SensorService {
 public:
  void begin();
  void update(uint32_t now);
  SensorSample read();
 private:
  HDC1080 hdc_;
  BH1750 lightMeter_;
  bool hdcReady_ = false, lightReady_ = false;
  uint32_t lastRetry_ = 0;
  void initializeMissing();
};
