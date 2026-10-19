#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace remote {
enum class Action : uint8_t { Water, Stop };
struct Command { uint32_t id; Action action; uint32_t expiresAt; };
inline bool positiveUint32(double value) {
  return std::isfinite(value) && value >= 1 && value <= UINT32_MAX &&
         std::floor(value) == value;
}
inline const char *validateCommand(double id, const char *action, double expires,
                                   uint32_t now, Command &out) {
  if (!positiveUint32(id) || !positiveUint32(expires) || !action)
    return "invalid_command";
  if (std::strcmp(action, "water") && std::strcmp(action, "stop"))
    return "invalid_command";
  out = {static_cast<uint32_t>(id), !std::strcmp(action, "water") ?
         Action::Water : Action::Stop, static_cast<uint32_t>(expires)};
  if (now < 1704067200UL) return "clock_unavailable";
  if (out.expiresAt <= now) return "expired";
  if (out.expiresAt - now > 120) return "invalid_expiry";
  return "valid";
}
}  // namespace remote
