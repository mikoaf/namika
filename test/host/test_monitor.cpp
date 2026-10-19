#include "PlantMonitor.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
  SensorSample good;
  good.values = {25, 50, 12000, 10};
  good.soilRaw = 2830;
  good.lightValid = true;
  plant::Monitor monitor(true);
  monitor.begin(100);
  assert(!monitor.status().wateringInputUsable(100));
  for (uint32_t i = 1; i <= config::WINDOW_SAMPLES; ++i)
    monitor.sample(good, i * config::SAMPLE_MS, true, 12);
  const uint32_t last = config::WINDOW_SAMPLES * config::SAMPLE_MS;
  assert(monitor.status().sampleCount == config::WINDOW_SAMPLES);
  assert(monitor.status().mood.overall == 3 && monitor.status().environmentKnown());
  assert(monitor.status().wateringInputUsable(last + 9999));
  assert(!monitor.status().wateringInputUsable(last + 10000));
  monitor.sample(good, last + 5000, true, 21);
  assert(monitor.status().sleeping);
  monitor.sample(good, last + 10000, true, 9);
  assert(!monitor.status().sleeping);

  SensorSample bad = good;
  bad.values.temperature = NAN;
  monitor.sample(bad, last + 15000, true, 12);
  assert(!monitor.status().sensorsValid && monitor.status().sampleCount == 0);
  assert(!monitor.status().environmentKnown());
  assert(!monitor.status().wateringInputUsable(last + 15000));
  monitor.sample(good, last + 20000, true, 12);
  assert(monitor.status().sampleCount == 1); // Outage discards previous evidence.
  assert(monitor.status().mood.overall == 2);

  for (int rail : {0, 4095}) {
    bad = good;
    bad.soilRaw = rail;
    monitor.sample(bad, last + 25000, true, 12);
    assert(!monitor.status().wateringInputUsable(last + 25000));
  }
  plant::Monitor uncalibrated(false);
  for (uint32_t i = 0; i < config::WINDOW_SAMPLES; ++i)
    uncalibrated.sample(good, i * config::SAMPLE_MS, true, 12);
  assert(uncalibrated.status().mood.overall == 2);
  assert(!uncalibrated.status().environmentKnown());
  assert(!uncalibrated.status().wateringInputUsable(last));

  plant::Monitor wet(true);
  bad = good;
  bad.values.soil = 80;
  wet.sample(bad, 0, true, 12);
  for (uint32_t i = 0; i < config::WINDOW_SAMPLES - 1; ++i)
    wet.sample(bad, config::WET_WARNING_MS + i * config::SAMPLE_MS, true, 12);
  assert(wet.status().wetTooLong && wet.status().mood.overall == 1);
  bad.values.lux = NAN;
  bad.lightValid = false;
  wet.sample(bad, config::WET_WARNING_MS + last, false, 0);
  assert(!wet.status().wetTooLong && wet.status().sampleCount == 0);

  plant::Monitor rollover(true);
  rollover.sample(good, UINT32_MAX - 100, false, 0);
  assert(rollover.status().wateringInputUsable(98));
  assert(!rollover.status().wateringInputUsable(9899));
  std::puts("Monitoring integration, stale input, calibration, outage, sleep and wet warning: PASS");
}
