#pragma once
#include <cstdint>

struct LocalClock {
  bool valid = false;
  int hour = 0;
};

class Connectivity {
 public:
  void begin();
  void update(uint32_t now);
  LocalClock clock() const;
  uint32_t epoch() const;
 private:
  bool configured_ = false;
  uint32_t lastAttempt_ = 0;
};
