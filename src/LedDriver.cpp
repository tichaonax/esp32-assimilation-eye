#include "LedDriver.h"

#include <Arduino.h>

bool LedDriver::begin() {
  bool ok = true;
  for (size_t i = 0; i < hw::RING_LED_COUNT; i++) {
    // Arduino core 3.x: the LEDC channel is chosen for us, the pin is the handle.
    _attached[i] = ledcAttach(hw::RING_LED_PINS[i], hw::PWM_FREQUENCY_HZ, hw::PWM_RESOLUTION_BITS);
    ok = ok && _attached[i];
  }
  off();
  return ok;
}

void LedDriver::set(size_t index, float level) {
  if (index >= hw::RING_LED_COUNT || !_attached[index]) {
    return;
  }
  if (!(level > 0.0f)) {  // also catches NaN
    level = 0.0f;
  } else if (level > 1.0f) {
    level = 1.0f;
  }
  const uint32_t duty = static_cast<uint32_t>(level * hw::PWM_MAX_DUTY + 0.5f);
  ledcWrite(hw::RING_LED_PINS[index], duty);
}

void LedDriver::setAll(float level) {
  for (size_t i = 0; i < hw::RING_LED_COUNT; i++) {
    set(i, level);
  }
}

void LedDriver::off() {
  setAll(0.0f);
}
