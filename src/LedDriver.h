// PWM output for the eight ring LEDs. One LEDC output per LED, so every LED
// has its own brightness.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "HardwareConfig.h"

class LedDriver {
 public:
  // Attaches every ring pin to hardware PWM and switches the ring off.
  // Returns false if any pin could not be attached.
  bool begin();

  // index is 0-based: 0 is LED 1. Out-of-range indexes are ignored.
  // level is the PWM duty as a fraction, 0.0 (off) to 1.0 (fully on).
  void set(size_t index, float level);
  void setAll(float level);
  void off();

 private:
  bool _attached[hw::RING_LED_COUNT] = {};
};
