#include "../../src/IrrigationService.cpp"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  IrrigationService service;
  service.begin();
  const int off = config::PUMP_ACTIVE_LEVEL == HIGH ? LOW : HIGH;
  assert(pumpLevel == off);
  const uint32_t epoch = 1800000000;
  remote::Command water{1, remote::Action::Water, epoch + 60};
  service.command(water, 100, epoch, 0, true);
  assert(service.controller().pumping());
  assert(pumpLevel == config::PUMP_ACTIVE_LEVEL);
  service.suspend(101);
  assert(!service.controller().pumping());
  assert(pumpLevel == off);
  // A command queued just before OTA must resolve, while keeping the pump off.
  water.id = 2;
  service.command(water, 102, epoch, 0, true, true);
  assert(service.lastCommandId() == 2);
  assert(!strcmp(service.lastCommandResult(), "ota_busy"));
  assert(!service.controller().pumping());
  assert(pumpLevel == off);
  // Holding OTA through multiple loops cannot restart the interrupted cycle.
  service.suspend(103);
  service.suspend(104);
  assert(pumpLevel == off);
  service.update(105, true);
  assert(!service.controller().pumping());
  IrrigationService idle;
  idle.begin();
  idle.command(water, 200, epoch, 0, true, true);
  assert(!idle.controller().pumping());
  assert(!strcmp(idle.lastCommandResult(), "ota_busy"));
  // Normal commands are available again once OTA has been released.
  water.id = 3;
  idle.command(water, 201, epoch, 0, true);
  assert(idle.controller().pumping());
  idle.suspend(202);
  assert(pumpLevel == off);
  std::cout << "Irrigation OTA pump interlock tests passed\n";
}
