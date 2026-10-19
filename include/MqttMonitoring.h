#pragma once
#include <cstdint>
#include "CloudCommand.h"

namespace remote {
// All text pointers refer to immutable strings with program lifetime.
struct Snapshot {
  uint32_t uptimeMs, sampleAgeMs, commandId;
  float temperature, humidity, lux, soil;
  int soilRaw;
  unsigned samples, mood, pulses;
  bool valid, calibrated, automatic, pumping;
  const char *wateringState;
  const char *commandResult;
};
struct Config {
  const char *host;
  uint16_t port;
  const char *username;
  const char *password;
  const char *rootCa;
  const char *deviceId;
};
bool begin(const Config &config);
bool takeCommand(Command &command);
void publish(const Snapshot &snapshot);
}  // namespace remote
