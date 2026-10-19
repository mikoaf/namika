#include "PlantMonitor.h"

namespace plant {
void Monitor::sample(const SensorSample &sample, uint32_t now, bool clockValid, int hour) {
  status_.lastSample = now;
  status_.latest = sample.values;
  status_.soilRaw = sample.soilRaw;
  status_.sensorsValid = cactus::valid(sample.values);
  if (!status_.sensorsValid) {
    samples_.clear();
    status_.wetTooLong = wetCondition_.update(false, now, config::WET_WARNING_MS);
  } else {
    samples_.add(sample.values);
    status_.wetTooLong = wetCondition_.update(status_.soilCalibrated && sample.values.soil > 60,
                                             now, config::WET_WARNING_MS);
  }
  status_.sleeping = sleepDetector_.update(clockValid, hour, sample.lightValid,
      sample.values.lux, now, config::SLEEP_HOUR, config::WAKE_HOUR);
  status_.sampleCount = samples_.count();
  status_.average = status_.sampleCount ? samples_.mean() : status_.latest;
  if (status_.sensorsValid && status_.sampleCount == config::WINDOW_SAMPLES)
    status_.mood = cactus::evaluate(status_.average, status_.sleeping,
                                  status_.wetTooLong, status_.soilCalibrated);
  else
    status_.mood = {2, 2, 2, 2, 2, status_.sensorsValid ? "Mengumpulkan data" : "Cek sensor"};
}
}  // namespace plant
