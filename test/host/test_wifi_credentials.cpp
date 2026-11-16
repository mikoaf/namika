#include "WifiCredentials.h"
#include <cassert>
#include <iostream>
#include <string>

static bool valid(const std::string &ssid, const std::string &password) {
  return wifi::validCredentials(ssid.data(), ssid.size(), password.data(), password.size());
}

int main() {
  assert(valid("Rumah", "password"));
  assert(valid("Jaringan terbuka", ""));
  assert(!valid("", "password"));
  assert(valid(std::string(32, 's'), std::string(63, 'p')));
  assert(!valid(std::string(33, 's'), "password"));
  assert(!valid("Rumah", "1234567"));
  assert(!valid("Rumah", std::string(65, 'a')));
  assert(valid("Rumah", std::string(64, 'a')));
  assert(!valid("Rumah", std::string(64, 'g')));
  assert(!valid(std::string("abc\0def", 7), "password"));
  assert(!valid("Rumah", std::string("abcd\0efgh", 9)));
  // Limits apply to UTF-8 bytes, not the number of displayed characters.
  assert(!valid(std::string(17, '\xc3') + std::string(17, '\xa9'), "password"));
  wifi::Credentials stored;
  assert(!wifi::validStoredCredentials(stored));
  std::strcpy(stored.ssid, "Rumah");
  assert(wifi::validStoredCredentials(stored));
  stored.version = 2;
  assert(!wifi::validStoredCredentials(stored));
  stored.version = 1;
  std::memset(stored.ssid, 's', sizeof(stored.ssid));
  assert(!wifi::validStoredCredentials(stored));
  stored.ssid[32] = '\0';
  assert(wifi::validStoredCredentials(stored));
  std::memset(stored.password, 'a', sizeof(stored.password));
  assert(!wifi::validStoredCredentials(stored));
  std::cout << "Wi-Fi credential tests passed\n";
}
