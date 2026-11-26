#pragma once
#include <cstdint>
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1;
static int pumpLevel = LOW;
inline void digitalWrite(int, int level) { pumpLevel = level; }
inline void pinMode(int, int) {}
struct SerialMock {
  template<class... Args> void printf(const char *, Args...) {}
};
static SerialMock Serial;
