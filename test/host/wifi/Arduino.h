#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#define PROGMEM
constexpr int pdPASS = 1;
static uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline uint32_t pdMS_TO_TICKS(uint32_t value) { return value; }
inline void vTaskDelay(uint32_t) {}
inline int xTaskCreate(void (*)(void *), const char *, int, void *, int, void *) { return pdPASS; }
inline void configTime(long, int, const char *, const char *) {}
inline size_t strlcpy(char *dest, const char *source, size_t size) {
  const size_t length = std::strlen(source);
  if (size) { const size_t copied = std::min(length, size - 1); std::memcpy(dest, source, copied); dest[copied] = 0; }
  return length;
}
class String {
 public:
  String(const char *s = "") : value(s) {}
  const char *c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  void toCharArray(char *dest, size_t size) const { strlcpy(dest, c_str(), size); }
 private:
  std::string value;
};
struct SerialMock {
  template<class T> void print(const T &) {}
  template<class T> void println(const T &) {}
  template<class... T> void printf(const char *, T...) {}
};
static SerialMock Serial;
struct EspMock { uint64_t getEfuseMac() { return 0x123456; } };
static EspMock ESP;
