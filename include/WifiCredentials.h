#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wifi {
struct Credentials {
  uint32_t version = 1;
  char ssid[33]{};
  char password[65]{};
};

inline bool validCredentials(const char *ssid, size_t ssidLength,
                             const char *password, size_t passwordLength) {
  if (!ssidLength || ssidLength > 32 || passwordLength > 64 ||
      std::memchr(ssid, '\0', ssidLength) || std::memchr(password, '\0', passwordLength)) return false;
  if (!passwordLength) return true;
  if (passwordLength == 64) {
    for (size_t i = 0; i < passwordLength; ++i) {
      const char c = password[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
  }
  return passwordLength >= 8;
}

inline bool validStoredCredentials(const Credentials &value) {
  return value.version == 1 && value.ssid[32] == '\0' && value.password[64] == '\0' &&
      validCredentials(value.ssid, std::strlen(value.ssid), value.password, std::strlen(value.password));
}
} // namespace wifi
