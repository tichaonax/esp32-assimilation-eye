#include "OnboardLed.h"

#include <Arduino.h>

#include "HardwareConfig.h"

bool OnboardLed::begin() {
  if (!hw::ONBOARD_LED_PRESENT) {
    return true;
  }
  _attached = ledcAttach(hw::ONBOARD_LED_PIN, hw::PWM_FREQUENCY_HZ, hw::PWM_RESOLUTION_BITS);
  set(0.0f);
  return _attached;
}

void OnboardLed::set(float level) {
  if (!_attached) {
    return;
  }
  if (!(level > 0.0f)) {  // also catches NaN
    level = 0.0f;
  } else if (level > 1.0f) {
    level = 1.0f;
  }
  if (!hw::ONBOARD_LED_ACTIVE_HIGH) {
    level = 1.0f - level;
  }
  ledcWrite(hw::ONBOARD_LED_PIN, static_cast<uint32_t>(level * hw::PWM_MAX_DUTY + 0.5f));
}

bool OnboardLed::available() const {
  return hw::ONBOARD_LED_PRESENT && _attached;
}
