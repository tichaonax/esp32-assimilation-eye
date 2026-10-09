// Turns the current settings into a frame: picks the active pattern, applies
// the running flag and overall brightness, and gamma-corrects the result.
//
// Not thread-safe. AnimationTask owns one and is the only caller.
#pragma once

#include <stdint.h>

#include "EyeConfig.h"
#include "PatternRegistry.h"

namespace eye {

class Animator {
 public:
  explicit Animator(PatternRegistry& registry);

  // Sanitizes and adopts the settings. Returns false, changing nothing, if the
  // pattern name is not registered. Switching pattern restarts the new one.
  bool setConfig(const EyeConfig& config);

  const EyeConfig& config() const { return _config; }

  // Advances by elapsedMs and writes the PWM duty fraction (0..1) for each
  // LED. While stopped, every LED is off and the pattern does not advance.
  void render(uint32_t elapsedMs, float out[LED_COUNT]);

 private:
  PatternRegistry& _registry;
  EyeConfig _config;
  Pattern* _active;
};

}  // namespace eye
