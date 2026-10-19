#include "WateringController.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <string>

int main() {
  using namespace watering;
  Config c;
  c.dryMs = 100; c.pulseMs = 10; c.soakMs = 50; c.cooldownMs = 200;
  Controller w(c);
  w.sample(0, 0, false);
  w.sample(100, 10, true);
  w.sample(199, 10, true);
  assert(!w.pumping());
  w.sample(200, 15, true); // A non-dry sample resets confirmation.
  w.sample(201, 10, true);
  w.sample(301, 10, true);
  assert(w.pumping() && w.pulses() == 1);
  w.tick(310, true);
  assert(w.pumping());
  w.tick(311, true);
  assert(!w.pumping() && w.state() == State::Soaking);
  w.sample(360, 10, true);
  assert(!w.pumping());
  w.sample(361, 20, true); // Continue until wet threshold is reached.
  assert(w.pumping() && w.pulses() == 2);
  w.sample(362, 25, true);
  assert(!w.pumping() && w.state() == State::Cooldown);
  w.sample(561, 10, true);
  assert(!w.pumping());
  w.sample(562, 10, true);
  w.sample(662, 10, true);
  assert(w.pumping() && w.pulses() == 1);
  w.tick(663, false);
  assert(!w.pumping() && w.state() == State::Locked);
  w.sample(10000, 10, true);
  assert(!w.pumping());

  Controller limit(c);
  limit.sample(0, 0, true);
  limit.sample(100, 0, true);
  for (uint32_t t : {110u, 170u, 230u}) {
    limit.tick(t, true);
    limit.sample(t + 50, 0, true);
  }
  assert(limit.pulses() == 3 && limit.state() == State::Locked);
  Controller invalid(c);
  invalid.sample(0, 0, true);
  invalid.sample(100, 0, true);
  invalid.sample(101, NAN, true);
  assert(!invalid.pumping() && invalid.state() == State::Locked);

  Controller wrap(c);
  wrap.sample(UINT32_MAX - 50, 0, true);
  wrap.sample(49, 0, true);
  assert(wrap.pumping());
  wrap.tick(59, true);
  assert(!wrap.pumping());
  Controller manual(c);
  assert(std::string(manual.requestManual(0, 10, false)) == "sensor_unavailable");
  assert(std::string(manual.requestManual(0, 25, true)) == "soil_wet");
  assert(std::string(manual.requestManual(1, 10, true)) == "accepted");
  assert(manual.pumping());
  assert(std::string(manual.requestManual(2, 10, true)) == "busy");
  manual.tick(11, true);
  manual.sample(61, 10, true, false); // Manual is exactly one pulse, then cooldown.
  assert(manual.state() == State::Cooldown && manual.pulses() == 1);
  assert(std::string(manual.requestManual(62, 10, true)) == "cooldown");
  manual.tick(261, true);
  manual.sample(262, 0, true, false);
  manual.sample(1000, 0, true, false);
  assert(!manual.pumping()); // Automatic switch does not prevent manual commands.
  assert(std::string(manual.requestManual(1001, 10, true)) == "accepted");
  manual.stop(1002);
  assert(!manual.pumping() && manual.state() == State::Cooldown);
  Controller fault(c);
  fault.requestManual(0, 10, true);
  fault.tick(1, false);
  assert(std::string(fault.requestManual(2, 10, true)) == "locked");
  fault.stop(3);
  assert(fault.state() == State::Locked);
  std::puts("Watering thresholds, timers, limits, faults and rollover: PASS");
}
