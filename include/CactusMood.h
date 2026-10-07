#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cactus {
struct Reading { float temperature, humidity, lux, soil; };
inline bool valid(const Reading &r) {
  return std::isfinite(r.temperature) && std::isfinite(r.humidity) &&
         std::isfinite(r.lux) && std::isfinite(r.soil) &&
         r.humidity >= 0 && r.humidity <= 100 && r.lux >= 0 &&
         r.soil >= 0 && r.soil <= 100;
}
template <std::size_t N> class AverageWindow {
 public:
  bool add(const Reading &r) {
    if (!valid(r)) return false;
    readings_[next_] = r;
    next_ = (next_ + 1) % N;
    if (count_ < N) ++count_;
    return true;
  }
  std::size_t count() const { return count_; }
  void clear() { count_ = next_ = 0; }
  Reading mean() const {
    Reading result{};
    if (!count_) return result;
    for (std::size_t i = 0; i < count_; ++i) {
      result.temperature += readings_[i].temperature;
      result.humidity += readings_[i].humidity;
      result.lux += readings_[i].lux;
      result.soil += readings_[i].soil;
    }
    result.temperature /= count_; result.humidity /= count_;
    result.lux /= count_; result.soil /= count_;
    return result;
  }
 private:
  Reading readings_[N]{};
  std::size_t count_ = 0, next_ = 0;
};
// Unsigned subtraction handles millis() rollover.
class HeldCondition {
 public:
  bool update(bool condition, uint32_t now, uint32_t duration) {
    if (!condition) { active_ = satisfied_ = false; return false; }
    if (!active_) { active_ = true; started_ = now; }
    if (uint32_t(now - started_) >= duration) satisfied_ = true;
    return satisfied_;
  }
 private:
  bool active_ = false;
  bool satisfied_ = false;
  uint32_t started_ = 0;
};
inline float soilPercent(int raw, int dry, int wet) {
  if (dry == wet) return NAN;
  const float value = 100.0f * (raw - dry) / (wet - dry);
  return value < 0 ? 0 : (value > 100 ? 100 : value);
}
struct Mood {
  uint8_t temperature, humidity, light, soil, overall;
  const char *reason;
};
inline Mood evaluate(const Reading &r, bool sleeping, bool wetTooLong,
                     bool soilCalibrated) {
  Mood m{};
  m.temperature = (r.temperature < 10 || r.temperature > 40) ? 1 :
                  ((r.temperature < 18 || r.temperature > 32) ? 2 : 3);
  m.humidity = r.humidity > 80 ? 1 : (r.humidity > 60 ? 2 : 3);
  m.light = r.lux < 2000 ? 1 : (r.lux < 10000 ? 2 : 3);
  m.soil = !soilCalibrated ? 2 :
           (r.soil > 60 && wetTooLong ? 1 : (r.soil > 25 ? 2 : 3));
  m.overall = 3; m.reason = "Kondisi baik";
  if (m.temperature == 1 || m.soil == 1) {
    m.overall = 1;
    m.reason = m.temperature == 1 ? "Suhu ekstrem" : "Tanah lama basah";
  } else if (!soilCalibrated) {
    m.overall = 2; m.reason = "Kalibrasi tanah";
  } else if (m.temperature == 2) {
    m.overall = 2; m.reason = "Cek suhu";
  } else if (m.soil == 2) {
    m.overall = 2; m.reason = "Tanah mengering";
  } else if (!sleeping && m.light < 3) {
    m.overall = 2; m.reason = "Kurang cahaya";
  } else if (m.humidity < 3) {
    // Humid air alone is advisory; it never forces overall mood 1.
    m.overall = 2; m.reason = "Udara lembap";
  }
  return m;
}
}  // namespace cactus
