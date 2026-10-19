#pragma once
#include <cstddef>
#include <cstring>

namespace remote {
// MQTT_EVENT_DATA may split one payload across several events.
class CommandPacket {
 public:
  static constexpr std::size_t capacity = 512;
  bool append(std::size_t offset, std::size_t total, const char *data,
              std::size_t length, bool acceptFirst) {
    if (!offset) {
      reset();
      if (!acceptFirst || !total || total > capacity) return false;
      total_ = total;
    }
    if (!total_ || total != total_ || offset != used_ || !data ||
        !length || length > total_ - used_ || std::memchr(data, '\0', length)) {
      reset();
      return false;
    }
    std::memcpy(payload_ + used_, data, length);
    used_ += length;
    payload_[used_] = '\0';
    return used_ == total_;
  }
  const char *data() const { return payload_; }
  void reset() { used_ = total_ = 0; payload_[0] = '\0'; }
 private:
  char payload_[capacity + 1]{};
  std::size_t used_ = 0, total_ = 0;
};
}  // namespace remote
