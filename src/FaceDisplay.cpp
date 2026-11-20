#include "FaceDisplay.h"
#include "AppConfig.h"
#include <Arduino.h>
#include <Wire.h>
#include <cstdio>
#include <cstring>

FaceDisplay::FaceDisplay()
    : display_(config::OLED_WIDTH, config::OLED_HEIGHT, &Wire, -1) {}

void FaceDisplay::begin(uint32_t now) {
  pinMode(config::TOUCH_SENSOR_PIN,
          config::TOUCH_ACTIVE_LEVEL == HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  ready_ = display_.begin(SSD1306_SWITCHCAPVCC, config::OLED_ADDRESS);
  display_.cp437(true);
  if (!ready_) Serial.println("OLED initialization failed");
  animation_.begin(now);
}

void FaceDisplay::update(uint32_t now, const plant::Status &plant,
                         const watering::Controller &watering, const LocalClock &clock) {
  const bool touched = touch_.update(
      digitalRead(config::TOUCH_SENSOR_PIN) == config::TOUCH_ACTIVE_LEVEL, now);
  const eyes::Animation previous = animation_.mode();
  animation_.update(now, plant.sleeping, touched, plant.environmentKnown(), plant.mood.overall);
  if (animation_.mode() != previous)
    Serial.printf("Animation: %s\n", eyes::name(animation_.mode()));
  if (uint32_t(now - lastFrame_) >= config::DISPLAY_MS) {
    lastFrame_ = now;
    draw(now, plant, watering, clock);
  }
}

void FaceDisplay::draw(uint32_t now, const plant::Status &plant,
                       const watering::Controller &watering, const LocalClock &clock) {
  if (!ready_) return;
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(0, 0);
  display_.print("NaMiKa");
  display_.setCursor(config::OLED_WIDTH - 5 * 6, 0);
  if (clock.valid) display_.printf("%02d:%02d", clock.hour, clock.minute);
  else display_.print("--:--");
  renderer_.draw(display_, animation_.mode(), animation_.elapsed(now), now);
  display_.setCursor(0, 56);
  // Diagnostics stay visible when touch or sleep overrides the face.
  if (watering.state() == watering::State::Locked || watering.pumping() ||
      watering.state() == watering::State::Soaking) display_.print(watering.status());
  else if (!plant.sensorsValid) display_.print("Cek sensor");
  else if ((now / 4000) % 2 == 0 && !plant.soilCalibrated && plant.mood.overall != 1)
    display_.print("Kalibrasi tanah");
  else if ((now / 4000) % 2 == 0 && plant.sampleCount < config::WINDOW_SAMPLES)
    display_.printf("Data %u/%u", unsigned(plant.sampleCount), unsigned(config::WINDOW_SAMPLES));
  else if ((now / 4000) % 2 == 0 && plant.mood.overall != 3)
    display_.print(plant.mood.reason);
  else {
    display_.printf("%.0f", plant.average.temperature);
    display_.write(uint8_t(0xF8));  // Degree symbol in the built-in font.
    display_.print("C");
    char humidity[8];
    snprintf(humidity, sizeof(humidity), "%.0f%%", plant.average.humidity);
    display_.setCursor(config::OLED_WIDTH - int(strlen(humidity)) * 6, 56);
    display_.print(humidity);
  }
  display_.display();
}
