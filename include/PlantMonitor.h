#pragma once
#include "AppConfig.h"
#include "SensorSample.h"
#include "EyeAnimation.h"

namespace plant {
struct Status {
  cactus::Reading latest{}, average{};
  cactus::Mood mood{2, 2, 2, 2, 2, "Mengumpulkan data"};
  int soilRaw = 0;
  std::size_t sampleCount = 0;
  uint32_t lastSample = 0;
  bool sensorsValid = false, sleeping = false, wetTooLong = false;
  bool soilCalibrated = false;
  bool environmentKnown() const {
    return sensorsValid && sampleCount == config::WINDOW_SAMPLES &&
           (soilCalibrated || mood.overall == 1);
  }
  bool wateringInputUsable(uint32_t now) const {
    return soilCalibrated && sensorsValid && soilRaw > 0 && soilRaw < 4095 &&
           uint32_t(now - lastSample) < 2 * config::SAMPLE_MS;
  }
};

// Pure monitoring logic; no Arduino, GPIO, display or network dependencies.
class Monitor {
 public:
  explicit Monitor(bool calibrated = config::SOIL_CALIBRATED) {
    status_.soilCalibrated = calibrated;
  }
  void begin(uint32_t now) { status_.lastSample = now; }
  void sample(const SensorSample &sample, uint32_t now, bool clockValid, int hour);
  const Status &status() const { return status_; }
 private:
  Status status_;
  cactus::AverageWindow<config::WINDOW_SAMPLES> samples_;
  cactus::HeldCondition wetCondition_;
  eyes::SleepDetector sleepDetector_;
};
}  // namespace plant
