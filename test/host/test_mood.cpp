#include "CactusMood.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
  using namespace cactus;
  Reading r{25, 50, 12000, 10};
  assert(evaluate(r, false, false, true).overall == 3);
  assert(evaluate(r, false, false, false).overall == 2);
  for (float t : {9.9f, 40.1f}) {
    r.temperature = t;
    assert(evaluate(r, true, false, true).overall == 1);
  }
  for (float t : {10.f, 17.9f, 32.1f, 40.f}) {
    r.temperature = t;
    assert(evaluate(r, false, false, true).temperature == 2);
  }
  for (float t : {18.f, 32.f}) {
    r.temperature = t;
    assert(evaluate(r, false, false, true).temperature == 3);
  }
  r.temperature = 25;
  r.lux = 0;
  assert(evaluate(r, false, false, true).overall == 2);
  assert(evaluate(r, true, false, true).overall == 3);
  r.lux = 2000;
  assert(evaluate(r, false, false, true).light == 2);
  r.lux = 10000;
  assert(evaluate(r, false, false, true).light == 3);
  r.humidity = 85;
  assert(evaluate(r, false, false, true).humidity == 1);
  assert(evaluate(r, false, false, true).overall == 2);
  r.humidity = 50;
  r.soil = 80;
  assert(evaluate(r, false, false, true).overall == 2); // Freshly watered.
  assert(evaluate(r, true, true, true).overall == 1); // Warning overrides sleep.
  assert(evaluate(r, false, true, false).overall == 2); // No false wet alarm.
  r.soil = 60;
  assert(evaluate(r, false, true, true).soil == 2);
  r.soil = 25;
  assert(evaluate(r, false, false, true).overall == 3);
  r.soil = 0;
  assert(evaluate(r, false, false, true).overall == 3); // Dry is not sad.

  assert(soilPercent(3000, 3000, 1300) == 0);
  assert(soilPercent(1300, 3000, 1300) == 100);
  assert(soilPercent(2150, 3000, 1300) == 50);
  assert(soilPercent(4095, 3000, 1300) == 0);
  assert(soilPercent(0, 3000, 1300) == 100);
  assert(soilPercent(1500, 1000, 2000) == 50); // Opposite sensor polarity.
  assert(std::isnan(soilPercent(1000, 1000, 1000)));

  AverageWindow<3> window;
  assert(window.count() == 0);
  assert(window.add({20, 40, 10000, 0}));
  assert(window.add({25, 50, 20000, 15}));
  assert(window.add({30, 60, 30000, 30}));
  assert(window.mean().temperature == 25);
  assert(!window.add({NAN, 50, 0, 10}));
  assert(!window.add({25, 50, -1, 10}));
  assert(window.count() == 3);
  assert(window.add({35, 70, 40000, 45}));
  assert(window.mean().temperature == 30); // Oldest sample leaves window.
  window.clear();
  assert(window.count() == 0);

  HeldCondition wet;
  constexpr uint32_t duration = 48UL * 60 * 60 * 1000;
  assert(!wet.update(true, 100, duration));
  assert(!wet.update(true, duration + 99, duration));
  assert(wet.update(true, duration + 100, duration));
  assert(!wet.update(false, duration + 101, duration));
  assert(!wet.update(true, duration + 102, duration));
  HeldCondition rollover;
  assert(!rollover.update(true, UINT32_MAX - 100, 200));
  assert(!rollover.update(true, 98, 200));
  assert(rollover.update(true, 99, 200));
  assert(rollover.update(true, UINT32_MAX - 100, 200)); // Alarm stays latched.
  assert(!rollover.update(false, 100, 200));
  std::puts("Mood, calibration, moving average, wet timer and rollover: PASS");
}
