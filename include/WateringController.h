#pragma once
#include <cmath>
#include <cstdint>

namespace watering {
struct Config {
  float dry = 15, wet = 25;
  uint32_t dryMs = 5UL * 60 * 1000;
  uint32_t pulseMs = 1000;
  uint32_t soakMs = 5UL * 60 * 1000;
  uint32_t cooldownMs = 24UL * 60 * 60 * 1000;
  uint8_t maxPulses = 3;
};
enum class State { Monitoring, Watering, Soaking, Cooldown, Locked };
class Controller {
 public:
  explicit Controller(const Config &config = Config()) : config_(config) {}
  bool pumping() const { return state_ == State::Watering; }
  State state() const { return state_; }
  uint8_t pulses() const { return pulses_; }
  const char *status() const {
    switch (state_) {
      case State::Watering: return "Pompa menyala";
      case State::Soaking: return "Tunggu resapan";
      case State::Cooldown: return "Jeda penyiraman";
      case State::Locked: return "Cek air / sensor";
      default: return "Pantau tanah";
    }
  }
  const char *requestManual(uint32_t now, float soil, bool usable) {
    usable = usable && std::isfinite(soil) && soil >= 0 && soil <= 100;
    tick(now, usable);
    if (!usable) return "sensor_unavailable";
    if (state_ == State::Locked) return "locked";
    if (state_ == State::Cooldown) return "cooldown";
    if (state_ != State::Monitoring) return "busy";
    if (soil >= config_.wet) return "soil_wet";
    start(now);
    manual_ = true;
    return pumping() ? "accepted" : "locked";
  }
  void stop(uint32_t now) {
    if (state_ == State::Watering || state_ == State::Soaking) finish(now);
    dryTracking_ = false;
  }
  // Called every loop: pump duration does not depend on the sensor interval.
  void tick(uint32_t now, bool inputUsable) {
    if (!inputUsable) {
      dryTracking_ = false;
      // An interrupted watering cycle requires inspection, never an auto retry.
      if (state_ == State::Watering || state_ == State::Soaking)
        state_ = State::Locked;
      return;
    }
    if (state_ == State::Watering && uint32_t(now - since_) >= config_.pulseMs) {
      state_ = State::Soaking;
      since_ = now;
    } else if (state_ == State::Cooldown &&
               uint32_t(now - since_) >= config_.cooldownMs) {
      state_ = State::Monitoring;
      dryTracking_ = false;
      pulses_ = 0;
    }
  }
  // Decisions use individual fresh readings, never the five-minute mood average.
  void sample(uint32_t now, float soil, bool usable, bool autoEnabled = true) {
    usable = usable && std::isfinite(soil) && soil >= 0 && soil <= 100;
    tick(now, usable);
    if (!usable || state_ == State::Locked || state_ == State::Cooldown) return;
    if (state_ == State::Watering) {
      if (soil >= config_.wet) finish(now);
      return;
    }
    if (state_ == State::Soaking) {
      if (uint32_t(now - since_) < config_.soakMs) return;
      if (manual_ || soil >= config_.wet) finish(now);
      else if (pulses_ >= config_.maxPulses) state_ = State::Locked;
      else start(now);
      return;
    }
    if (!autoEnabled) { dryTracking_ = false; return; }
    if (soil >= config_.dry) { dryTracking_ = false; return; }
    if (!dryTracking_) { dryTracking_ = true; drySince_ = now; }
    if (uint32_t(now - drySince_) >= config_.dryMs) start(now);
  }
 private:
  void start(uint32_t now) {
    if (!config_.maxPulses || !config_.pulseMs) { state_ = State::Locked; return; }
    manual_ = false;
    ++pulses_;
    state_ = State::Watering;
    since_ = now;
    dryTracking_ = false;
  }
  void finish(uint32_t now) { state_ = State::Cooldown; since_ = now; }
  Config config_;
  State state_ = State::Monitoring;
  bool dryTracking_ = false, manual_ = false;
  uint8_t pulses_ = 0;
  uint32_t since_ = 0, drySince_ = 0;
};
}  // namespace watering
