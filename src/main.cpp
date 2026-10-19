#include <Arduino.h>
#include "NamikaApp.h"

namespace {
NamikaApp app;
}

void setup() {
  app.begin();
}

void loop() {
  app.update();
  delay(5);
}
