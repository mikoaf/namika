#include "HDC1080.h"
#include <Arduino.h>
#include <cmath>

bool HDC1080::readRegister(uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(ADDRESS, uint8_t(2)) != 2) return false;
  const uint8_t high = Wire.read();
  const uint8_t low = Wire.read();
  value = (uint16_t(high) << 8) | low;
  return true;
}

bool HDC1080::begin() {
  // Allow the maximum power-up time before configuring the device.
  delay(15);
  uint16_t manufacturer = 0, device = 0;
  if (!readRegister(0xFE, manufacturer) || manufacturer != 0x5449 ||
      !readRegister(0xFF, device) || device != 0x1050) return false;

  // TI HDC1080 datasheet SNAS672A: paired acquisition, 14-bit resolution,
  // heater disabled. https://www.ti.com/lit/ds/symlink/hdc1080.pdf
  Wire.beginTransmission(ADDRESS);
  Wire.write(0x02);
  Wire.write(0x10);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

bool HDC1080::read(float &temperature, float &humidity) {
  temperature = humidity = NAN;
  Wire.beginTransmission(ADDRESS);
  Wire.write(0x00);  // Trigger temperature followed by humidity.
  if (Wire.endTransmission() != 0) return false;
  // Both 14-bit conversions take about 13 ms in total. Release the bus
  // before waiting so the OLED and BH1750 can share it.
  delay(20);
  if (Wire.requestFrom(ADDRESS, uint8_t(4)) != 4) return false;
  const uint8_t temperatureHigh = Wire.read();
  const uint8_t temperatureLow = Wire.read();
  const uint8_t humidityHigh = Wire.read();
  const uint8_t humidityLow = Wire.read();
  const uint16_t rawTemperature = (uint16_t(temperatureHigh) << 8) | temperatureLow;
  const uint16_t rawHumidity = (uint16_t(humidityHigh) << 8) | humidityLow;
  temperature = rawTemperature * (165.0f / 65536.0f) - 40.0f;
  humidity = rawHumidity * (100.0f / 65536.0f);
  return true;
}
