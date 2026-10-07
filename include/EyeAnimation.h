#pragma once
#include <cstdint>
#include "CactusMood.h"

namespace eyes {
enum class Animation { WAKEUP, SLEEP, HAPPY, SACCADE_RANDOM, SAD, RESET };
inline const char *name(Animation mode) {
  switch (mode) {
    case Animation::WAKEUP: return "Wakeup";
    case Animation::SLEEP: return "Sleep";
    case Animation::HAPPY: return "Happy";
    case Animation::SACCADE_RANDOM: return "Saccade";
    case Animation::SAD: return "Sad";
    default: return "Netral";
  }
}

class TouchDebouncer {
 public:
  bool update(bool raw, uint32_t now) {
    if (raw != candidate_) { candidate_ = raw; changed_ = now; }
    if (uint32_t(now - changed_) >= 30) pressed_ = candidate_;
    return pressed_;
  }
 private:
  bool candidate_ = false, pressed_ = false;
  uint32_t changed_ = 0;
};

// Sleep when it is nighttime OR sustained darkness, even with NTP available.
class SleepDetector {
 public:
  bool update(bool clockValid, int hour, bool lightValid, float lux,
              uint32_t now, int sleepHour = 20, int wakeHour = 6) {
    if (darkHold_.update(lightValid && lux < 20, now, 5UL * 60 * 1000)) dark_ = true;
    if (brightHold_.update(lightValid && lux > 100, now, 30000)) dark_ = false;
    const bool night = clockValid && (sleepHour > wakeHour ?
        (hour >= sleepHour || hour < wakeHour) :
        (hour >= sleepHour && hour < wakeHour));
    return night || dark_;
  }
 private:
  bool dark_ = false;
  cactus::HeldCondition darkHold_, brightHold_;
};

class Controller {
 public:
  static constexpr uint32_t WAKEUP_MS = 1200;
  static constexpr uint32_t HAPPY_MS = 3000;
  void begin(uint32_t now) {
    waking_ = true;
    wakeStarted_ = now;
    wasSleeping_ = happy_ = false;
    mode_ = Animation::WAKEUP;
    modeSince_ = now;
  }
  Animation update(uint32_t now, bool sleeping, bool touched,
                   bool environmentKnown, uint8_t mood) {
    if (wasSleeping_ && !sleeping) { waking_ = true; wakeStarted_ = now; }
    wasSleeping_ = sleeping;
    if (waking_ && uint32_t(now - wakeStarted_) >= WAKEUP_MS) waking_ = false;
    // Held touches keep happiness active; retain it for 3 s after release.
    if (touched) { happy_ = true; happyStarted_ = now; }
    if (happy_ && uint32_t(now - happyStarted_) >= HAPPY_MS) happy_ = false;
    const Animation next = happy_ ? Animation::HAPPY :
        waking_ ? Animation::WAKEUP : sleeping ? Animation::SLEEP :
        !environmentKnown ? Animation::RESET :
        mood == 3 ? Animation::SACCADE_RANDOM : Animation::SAD;
    if (next != mode_) { mode_ = next; modeSince_ = now; }
    return mode_;
  }
  Animation mode() const { return mode_; }
  uint32_t elapsed(uint32_t now) const { return uint32_t(now - modeSince_); }
 private:
  Animation mode_ = Animation::RESET;
  bool waking_ = false, wasSleeping_ = false, happy_ = false;
  uint32_t wakeStarted_ = 0, happyStarted_ = 0, modeSince_ = 0;
};
}  // namespace eyes
