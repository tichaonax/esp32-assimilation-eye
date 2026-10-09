// Synchronized flash: all eight LEDs rise to a maximum, hold, and fall back
// to a minimum together, once per interval. Ramps of zero give an abrupt flash.
#pragma once

#include "Pattern.h"

namespace eye {

class FlashPattern : public Pattern {
 public:
  const char* name() const override { return "flash"; }
  const char* label() const override { return "Synchronized flash"; }
  void reset() override { _phaseMs = 0; }
  void render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) override;

  // Brightness (0..1) at a given time into one cycle:
  //   ramp up -> hold at maximum -> ramp down -> rest at minimum
  static float levelAt(const FlashSettings& flash, uint32_t phaseMs);

  // Time into the current cycle.
  uint32_t phaseMs() const { return _phaseMs; }

 private:
  uint32_t _phaseMs = 0;
};

}  // namespace eye
