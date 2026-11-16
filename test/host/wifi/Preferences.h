#pragma once
#include <cstring>
#include <vector>
struct Preferences {
  static std::vector<unsigned char> record;
  static bool writeWorks;
  bool begin(const char *, bool) { return true; }
  void end() {}
  size_t getBytesLength(const char *) { return record.size(); }
  size_t getBytes(const char *, void *value, size_t size) {
    if (record.size() != size) return 0;
    std::memcpy(value, record.data(), size); return size;
  }
  size_t putBytes(const char *, const void *value, size_t size) {
    if (!writeWorks) return 0;
    const auto *bytes = static_cast<const unsigned char *>(value);
    record.assign(bytes, bytes + size); return size;
  }
};
