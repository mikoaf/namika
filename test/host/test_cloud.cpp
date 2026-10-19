#include "CloudCommand.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
  using namespace remote;
  const uint32_t now = 1800000000;
  Command command{};
  auto check = [&](double id, const char *action, double expiry, uint32_t clock,
                   const char *expected) {
    command = {};
    assert(std::string(validateCommand(id, action, expiry, clock, command)) == expected);
  };
  check(1, "water", now + 60, now, "valid");
  assert(command.id == 1 && command.action == Action::Water);
  check(2, "stop", now + 120, now, "valid");
  assert(command.action == Action::Stop);
  check(3, "water", now, now, "expired");
  check(3, "water", now - 1, now, "expired");
  check(3, "water", now + 121, now, "invalid_expiry");
  check(3, "water", now + 60, 0, "clock_unavailable");
  check(0, "water", now + 60, now, "invalid_command");
  check(-1, "water", now + 60, now, "invalid_command");
  check(1.5, "water", now + 60, now, "invalid_command");
  check(4294967296.0, "water", now + 60, now, "invalid_command");
  check(NAN, "water", now + 60, now, "invalid_command");
  check(1, nullptr, now + 60, now, "invalid_command");
  check(1, "on", now + 60, now, "invalid_command");
  check(1, "water", INFINITY, now, "invalid_command");
  check(1, "water", now + 0.5, now, "invalid_command");
  std::puts("Cloud command types, IDs, expiry and clock validation: PASS");
}
