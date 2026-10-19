#include "MqttPacket.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
  remote::CommandPacket packet;
  const std::string payload = "{\"id\":1,\"action\":\"water\",\"expires_at\":1800000060}";
  assert(packet.append(0, payload.size(), payload.data(), payload.size(), true));
  assert(std::string(packet.data()) == payload);
  packet.reset();
  assert(!packet.append(0, payload.size(), payload.data(), 10, true));
  assert(packet.append(10, payload.size(), payload.data() + 10, payload.size() - 10, false));
  assert(std::string(packet.data()) == payload);
  // Retained delivery or the wrong topic must not be accepted as a new command.
  assert(!packet.append(0, payload.size(), payload.data(), 10, false));
  assert(!packet.append(10, payload.size(), payload.data() + 10, payload.size() - 10, false));
  assert(!packet.append(0, 513, "x", 1, true));
  assert(!packet.append(0, 0, "", 0, true));
  assert(!packet.append(0, payload.size(), payload.data(), 10, true));
  assert(!packet.append(11, payload.size(), "x", 1, false)); // Missing fragment.
  assert(!packet.append(0, 10, "abc", 3, true));
  assert(!packet.append(3, 11, "def", 3, false)); // Total size changes.
  assert(!packet.append(0, 2, "abc", 3, true)); // Bounds check.
  assert(!packet.append(0, 3, "a\0b", 3, true)); // No embedded terminator.
  const std::string full(512, 'x');
  assert(packet.append(0, full.size(), full.data(), full.size(), true));
  assert(std::string(packet.data()) == full);
  assert(packet.append(0, payload.size(), payload.data(), payload.size(), true));
  assert(std::string(packet.data()) == payload); // Recovery after malformed frames.
  std::puts("MQTT fragmentation, retained/topic filter and payload limits: PASS");
}
