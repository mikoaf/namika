#pragma once
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

struct TwoWire {
  std::vector<std::vector<uint8_t>> writes;
  std::vector<uint8_t> pending;
  std::deque<std::vector<uint8_t>> replies;
  std::deque<uint8_t> received;
  std::deque<uint8_t> statuses;
  void beginTransmission(uint8_t address) {
    assert(address == 0x40);
    pending.clear();
  }
  void write(uint8_t value) { pending.push_back(value); }
  uint8_t endTransmission(bool stop = true) {
    assert(stop);  // Never hold the shared I2C bus during conversion.
    writes.push_back(pending);
    if (statuses.empty()) return 0;
    auto status = statuses.front();
    statuses.pop_front();
    return status;
  }
  uint8_t requestFrom(uint8_t address, uint8_t count) {
    assert(address == 0x40);
    assert(!replies.empty());
    const auto reply = replies.front();
    replies.pop_front();
    assert(reply.size() <= count);
    received.assign(reply.begin(), reply.end());
    return uint8_t(received.size());
  }
  int read() {
    assert(!received.empty());
    auto value = received.front();
    received.pop_front();
    return value;
  }
};
extern TwoWire Wire;
