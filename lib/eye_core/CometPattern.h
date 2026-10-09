// Rotating comet: a bright head travelling round the ring with a fading tail.
#pragma once

#include "Pattern.h"

namespace eye {

class CometPattern : public Pattern {
 public:
  const char* name() const override { return "comet"; }
  const char* label() const override { return "Rotating comet"; }
  void reset() override { _position = 0.0f; }
  void render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) override;

  // Head position in LEDs, 0 <= position < LED_COUNT. 0 is LED 1. Fractional
  // while the head is between two LEDs.
  float position() const { return _position; }

 private:
  float _position = 0.0f;
};

}  // namespace eye
