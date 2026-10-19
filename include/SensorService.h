#pragma once
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "SensorSample.h"

// Owns sensor hardware; callers schedule reads and supply timestamps.
class SensorService {
 public:
  void begin();
  void update(uint32_t now);
  SensorSample read();
 private:
  Adafruit_BME280 bme_;
  BH1750 lightMeter_;
  bool bmeReady_ = false, lightReady_ = false;
  uint32_t lastRetry_ = 0;
  void initializeMissing();
};
