// What every LED pattern provides.
//
// To add a pattern: write a class that implements this, then register it in
// the PatternRegistry constructor. Settings it needs go in EyeConfig.
#pragma once

#include <stdint.h>

#include "EyeConfig.h"

namespace eye {

class Pattern {
 public:
  virtual ~Pattern() = default;

  // Identifier stored in settings and used by the API, e.g. "comet".
  virtual const char* name() const = 0;

  // Name shown in the interface, e.g. "Rotating comet".
  virtual const char* label() const = 0;

  // Back to the start of the animation.
  virtual void reset() = 0;

  // Advances the animation by elapsedMs and writes the perceived brightness
  // (0..1) of each LED. Overall brightness and gamma are applied by the caller.
  //
  // Patterns advance by elapsed time instead of reading a clock, so changing a
  // speed setting mid-run carries on from where the animation is, with no jump.
  virtual void render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) = 0;
};

}  // namespace eye
