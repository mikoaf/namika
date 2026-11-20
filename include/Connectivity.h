#pragma once
#include <cstdint>

struct LocalClock {
  bool valid = false;
  int hour = 0;
  int minute = 0;
};

class Connectivity {
 public:
  void begin();
  LocalClock clock() const;
  uint32_t epoch() const;
};
