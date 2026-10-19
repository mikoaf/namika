#pragma once
#include <Adafruit_SSD1306.h>
#include "PlantMonitor.h"
#include "EyeRenderer.h"
#include "WateringController.h"

class FaceDisplay {
 public:
  FaceDisplay();
  void begin(uint32_t now);
  void update(uint32_t now, const plant::Status &plant, const watering::Controller &watering);
 private:
  Adafruit_SSD1306 display_;
  EyeRenderer renderer_;
  eyes::Controller animation_;
  eyes::TouchDebouncer touch_;
  bool ready_ = false;
  uint32_t lastFrame_ = 0;
  void draw(uint32_t now, const plant::Status &plant, const watering::Controller &watering);
};
