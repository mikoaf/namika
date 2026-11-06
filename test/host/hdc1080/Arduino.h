#pragma once
#include <cstdint>
extern unsigned long delayedMs;
inline void delay(unsigned long ms) { delayedMs += ms; }
