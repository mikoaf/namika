#pragma once
#include <Wire.h>

// Wire must already be initialized by the application.
class HDC1080 {
 public:
  bool begin();
  bool read(float &temperature, float &humidity);

 private:
  static constexpr uint8_t ADDRESS = 0x40;
  bool readRegister(uint8_t reg, uint16_t &value);
};
