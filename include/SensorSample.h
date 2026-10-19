#pragma once
#include "CactusMood.h"

struct SensorSample {
  cactus::Reading values{};
  int soilRaw = 0;
  bool lightValid = false;
};
