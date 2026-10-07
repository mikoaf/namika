#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "EyeAnimation.h"

// Adapted from the supplied rounded-eye animations, using Adafruit directly.
// Leave a status row above and below the eyes on the 128x64 screen.
class EyeRenderer {
 public:
  void draw(Adafruit_SSD1306 &display, eyes::Animation mode, uint32_t elapsed,
            uint32_t now) {
    constexpr int width = 40, refHeight = 32, radius = 10;
    int height = refHeight, offsetX = 0, offsetY = 0;
    if (mode != previous_) {
      previous_ = mode;
      gazeX_ = gazeY_ = oldX_ = oldY_ = 0;
      nextMove_ = now;
      moving_ = false;
    }
    if (mode == eyes::Animation::WAKEUP) {
      const uint32_t progress = elapsed < eyes::Controller::WAKEUP_MS ?
          elapsed : eyes::Controller::WAKEUP_MS;
      height = 2 + (refHeight - 2) * progress / eyes::Controller::WAKEUP_MS;
    } else if (mode == eyes::Animation::SLEEP) {
      height = 2;
    } else if (mode == eyes::Animation::SACCADE_RANDOM) {
      if (uint32_t(now - nextMove_) >= 1400) {
        oldX_ = gazeX_; oldY_ = gazeY_;
        // Absolute targets prevent cumulative movement off-screen.
        gazeX_ = random(-8, 9); gazeY_ = random(-3, 4);
        moveStarted_ = nextMove_ = now;
        moving_ = true;
      }
      const uint32_t progress = moving_ ? uint32_t(now - moveStarted_) : 160;
      if (progress < 160) {
        offsetX = oldX_ + (gazeX_ - oldX_) * int(progress) / 160;
        offsetY = oldY_ + (gazeY_ - oldY_) * int(progress) / 160;
        height -= progress < 80 ? 6 : 3;
      } else { offsetX = gazeX_; offsetY = gazeY_; moving_ = false; }
    }
    if (mode != eyes::Animation::WAKEUP && mode != eyes::Animation::SLEEP &&
        now % 5000 < 120) height = 2;
    const int eyeRadius = min(radius, max(0, height / 2 - 1));
    for (int side = 0; side < 2; ++side) {
      const int centerX = (side == 0 ? 39 : 89) + offsetX;
      const int centerY = 30 + offsetY;
      const int x = centerX - width / 2, y = centerY - height / 2;
      display.fillRoundRect(x, y, width, height, eyeRadius, SSD1306_WHITE);
      if (height > 2 && mode == eyes::Animation::HAPPY) {
        // Rising lower eyelids reproduce the supplied happy-eye shape.
        const int rise = min(16, int(elapsed / 25));
        const int cutY = centerY + refHeight / 2 - rise;
        if (side == 0)
          display.fillTriangle(x - 1, cutY, x + width + 1, cutY + 7,
                               x - 1, centerY + refHeight, SSD1306_BLACK);
        else
          display.fillTriangle(x + width + 1, cutY, x - 1, cutY + 7,
                               x + width + 1, centerY + refHeight, SSD1306_BLACK);
      } else if (height > 2 && mode == eyes::Animation::SAD) {
        // Inner upper eyelids droop toward the nose: a new sad expression.
        if (side == 0)
          display.fillTriangle(x - 1, y - 1, x + width + 1, y - 1,
                               x + width + 1, y + 15, SSD1306_BLACK);
        else
          display.fillTriangle(x - 1, y - 1, x + width + 1, y - 1,
                               x - 1, y + 15, SSD1306_BLACK);
      }
    }
    if (mode == eyes::Animation::SLEEP) {
      display.setCursor(109, 17 + ((elapsed / 700) % 2) * 2);
      display.print("zZ");
    } else if (mode == eyes::Animation::SAD) {
      display.drawLine(57, 51, 64, 48, SSD1306_WHITE);
      display.drawLine(64, 48, 71, 51, SSD1306_WHITE);
    }
  }
 private:
  eyes::Animation previous_ = eyes::Animation::RESET;
  int gazeX_ = 0, gazeY_ = 0, oldX_ = 0, oldY_ = 0;
  uint32_t nextMove_ = 0, moveStarted_ = 0;
  bool moving_ = false;
};
