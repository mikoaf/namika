#include "EyeAnimation.h"
#include <cassert>
#include <cstdio>

int main() {
  using namespace eyes;
  Controller controller;
  controller.begin(100);
  assert(controller.update(100, true, false, true, 1) == Animation::WAKEUP);
  assert(controller.update(1299, true, false, true, 1) == Animation::WAKEUP);
  assert(controller.update(1300, true, false, true, 1) == Animation::SLEEP);
  assert(controller.update(2000, false, false, true, 3) == Animation::WAKEUP);
  assert(controller.update(3200, false, false, true, 3) == Animation::SACCADE_RANDOM);
  assert(controller.update(4000, false, false, true, 3) == Animation::SACCADE_RANDOM);
  assert(controller.update(4100, false, false, true, 2) == Animation::SAD);
  assert(controller.update(4200, false, false, true, 1) == Animation::SAD);
  assert(controller.update(4300, false, false, false, 2) == Animation::RESET);
  assert(controller.update(4400, true, false, true, 1) == Animation::SLEEP);
  assert(controller.update(4500, true, true, true, 1) == Animation::HAPPY);
  assert(controller.update(8500, true, true, true, 1) == Animation::HAPPY); // Held.
  assert(controller.update(11499, true, false, true, 1) == Animation::HAPPY);
  assert(controller.update(11500, true, false, true, 1) == Animation::SLEEP);
  assert(controller.update(12000, false, false, true, 1) == Animation::WAKEUP);
  assert(controller.update(13200, false, false, true, 1) == Animation::SAD);
  assert(controller.update(14000, false, true, true, 1) == Animation::HAPPY);
  assert(controller.update(17000, false, false, true, 1) == Animation::SAD);
  Controller rollover;
  rollover.begin(UINT32_MAX - 500);
  assert(rollover.update(698, false, false, true, 3) == Animation::WAKEUP);
  assert(rollover.update(699, false, false, true, 3) == Animation::SACCADE_RANDOM);
  assert(rollover.update(UINT32_MAX - 50, true, true, true, 3) == Animation::HAPPY);
  assert(rollover.update(2948, true, false, true, 3) == Animation::HAPPY);
  assert(rollover.update(2949, true, false, true, 3) == Animation::SLEEP);

  SleepDetector night;
  assert(!night.update(true, 19, true, 12000, 0));
  assert(night.update(true, 20, true, 12000, 5000));
  assert(night.update(true, 5, true, 12000, 10000));
  assert(!night.update(true, 6, true, 12000, 15000));
  SleepDetector dark;
  assert(!dark.update(true, 12, true, 10, 100)); // Darkness also works with NTP.
  assert(!dark.update(true, 12, true, 10, 300099));
  assert(dark.update(true, 12, true, 10, 300100));
  assert(dark.update(true, 12, true, 50, 301000)); // Hysteresis.
  assert(dark.update(true, 12, false, -1, 302000)); // Failure is not bright.
  assert(dark.update(true, 12, true, 1000, 303000));
  assert(dark.update(true, 12, true, 1000, 332999));
  assert(!dark.update(true, 12, true, 1000, 333000));
  SleepDetector offline;
  assert(!offline.update(false, 22, false, -1, 0)); // No fake night from invalid clock.
  assert(!offline.update(false, 0, true, 0, 100));
  assert(offline.update(false, 0, true, 0, 300100));
  SleepDetector interrupted;
  assert(!interrupted.update(false, 0, true, 0, 0));
  assert(!interrupted.update(false, 0, false, -1, 299999));
  assert(!interrupted.update(false, 0, true, 0, 300000));
  assert(!interrupted.update(false, 0, true, 0, 599999));
  assert(interrupted.update(false, 0, true, 0, 600000));

  TouchDebouncer touch;
  assert(!touch.update(true, 100));
  assert(!touch.update(false, 110));
  assert(!touch.update(true, 120));
  assert(!touch.update(true, 149));
  assert(touch.update(true, 150));
  assert(touch.update(false, 160));
  assert(!touch.update(false, 190));
  std::puts("Boot, morning wakeup, sleep, touch, priorities, debounce and rollover: PASS");
}
