#include <HDC1080.h>
#include <cassert>
#include <cmath>
#include <iostream>

TwoWire Wire;
unsigned long delayedMs = 0;

int main() {
  HDC1080 sensor;
  Wire.replies = {{0x54, 0x49}, {0x10, 0x50}};
  assert(sensor.begin());
  assert(delayedMs >= 15);
  assert((Wire.writes.back() == std::vector<uint8_t>{0x02, 0x10, 0x00}));

  float temperature = 0, humidity = 0;
  Wire.replies = {{0x80, 0x00, 0x80, 0x00}};
  delayedMs = 0;
  assert(sensor.read(temperature, humidity));
  assert(std::fabs(temperature - 42.5f) < 0.001f);
  assert(std::fabs(humidity - 50.0f) < 0.001f);
  assert(delayedMs >= 13);
  assert((Wire.writes.back() == std::vector<uint8_t>{0x00}));

  // Missing sensor and incomplete reads must never become plausible readings.
  Wire.statuses = {2};
  assert(!sensor.read(temperature, humidity));
  assert(std::isnan(temperature) && std::isnan(humidity));
  Wire.replies = {{0x80, 0x00, 0x80}};
  assert(!sensor.read(temperature, humidity));
  assert(std::isnan(temperature) && std::isnan(humidity));

  Wire = TwoWire{};
  Wire.replies = {{0x00, 0x00}};
  assert(!sensor.begin());  // Another device at 0x40 is not an HDC1080.
  Wire.replies = {{0x54, 0x49}, {0x00, 0x00}};
  assert(!sensor.begin());
  Wire.replies = {{0x54, 0x49}, {0x10, 0x50}};
  Wire.statuses = {0, 0, 2};
  assert(!sensor.begin());  // Configuration write failed.
  Wire.replies = {{0x54, 0x49}, {0x10, 0x50}};
  assert(sensor.begin());  // Reinitialization succeeds after reconnecting.
  Wire.replies = {{0x00, 0x00, 0x00, 0x00}};
  assert(sensor.read(temperature, humidity));
  assert(temperature == -40.0f && humidity == 0.0f);
  std::cout << "HDC1080 tests passed\n";
}
