#include "MqttOtaProtocol.h"
#include <cassert>
#include <climits>
#include <iostream>
using namespace remote::ota;
int main() {
  Packet p;
  const char binary[] = {'\0', char(0xff), 'x', '\0'};
  assert(!p.append(0, 4, binary, 2, true));
  assert(p.append(2, 4, binary + 2, 2, false));
  assert(!memcmp(p.bytes, binary, 4));
  assert(!p.append(0, packetSize + 1, binary, 1, true));
  assert(!p.append(0, 4, binary, 2, false)); // Retained/wrong topic.
  assert(!p.append(2, 4, binary, 2, true));
  assert(!p.append(0, 4, binary, 1, true));
  assert(!p.append(2, 4, binary, 2, false)); // Gap resets assembly.
  assert(!p.append(1, 4, binary, 3, false));
  assert(!p.append(0, 4, binary, 1, true));
  assert(!p.append(1, 3, binary, 2, false)); // Inconsistent total.
  assert(!p.append(0, 4, nullptr, 4, true));
  assert(position(0, 2050, 0, 1024) == Position::Next);
  assert(position(1024, 2050, 0, 1024) == Position::Duplicate);
  assert(position(1024, 2050, 512, 1024) == Position::Invalid);
  assert(position(1024, 2050, 2048, 2) == Position::Invalid);
  assert(position(2048, 2050, 2048, 2) == Position::Next);
  assert(position(2050, 2050, 2048, 2) == Position::Duplicate);
  assert(position(2050, 2050, 2050, 1) == Position::Invalid);
  assert(position(0, UINT32_MAX, UINT32_MAX - 1, 10) == Position::Invalid);
  assert(position(0, 2050, 0, 0) == Position::Invalid);
  assert(position(0, 2050, 0, 1025) == Position::Invalid);
  uint8_t bytes[headerSize]{};
  bytes[32] = 0x12; bytes[33] = 0x34; bytes[34] = 0x56; bytes[35] = 0x78;
  assert(offset(bytes) == 0x12345678);
  assert(hex("0123456789abcdef0123456789abcdef", 32));
  assert(!hex("0123456789abcdef0123456789abcdeF", 32));
  assert(!hex("bad", 32));
  std::cout << "MQTT OTA protocol tests passed\n";
}
