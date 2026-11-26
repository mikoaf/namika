#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace remote { namespace ota {
constexpr size_t chunkSize = 1024;
constexpr size_t headerSize = 36; // 32 lowercase hex session bytes + uint32 BE offset.
constexpr size_t packetSize = headerSize + chunkSize;
inline bool hex(const char *s, size_t length) {
  if (!s || std::strlen(s) != length) return false;
  for (size_t i = 0; i < length; ++i)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return true;
}
// Binary-safe assembly of fragmented ESP-MQTT events, with a bounded buffer.
class Packet {
 public:
  bool append(size_t offset, size_t total, const char *data, size_t length, bool first) {
    if (!offset) {
      reset();
      if (!first || !total || total > packetSize) return false;
      total_ = total;
    }
    if (!total_ || total != total_ || offset != used_ || !data || !length || length > total_ - used_) {
      reset();
      return false;
    }
    std::memcpy(bytes + used_, data, length);
    used_ += length;
    return used_ == total_;
  }
  void reset() { used_ = total_ = 0; }
  uint8_t bytes[packetSize]{};
 private:
  size_t used_ = 0, total_ = 0;
};
inline uint32_t offset(const uint8_t *p) {
  return (uint32_t(p[32]) << 24) | (uint32_t(p[33]) << 16) |
         (uint32_t(p[34]) << 8) | uint32_t(p[35]);
}
enum class Position { Next, Duplicate, Invalid };
inline Position position(uint32_t received, uint32_t size, uint32_t at, size_t length) {
  if (!length || length > chunkSize || at > size || length > size - at) return Position::Invalid;
  if (at == received) return Position::Next;
  if (at < received && length <= received - at) return Position::Duplicate;
  return Position::Invalid;
}
}} // namespace remote::ota
