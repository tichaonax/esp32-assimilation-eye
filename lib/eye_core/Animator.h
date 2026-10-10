// Turns the current settings into a frame: picks the active pattern, applies
// the running flag and overall brightness, and gamma-corrects the result.
// Also works out what the onboard LED should be doing.
//
// Not thread-safe. AnimationTask owns one and is the only caller.
#pragma once

#include <stdint.h>

#include "EyeConfig.h"
#include "OnboardLogic.h"
#include "PatternRegistry.h"

namespace eye {

class Animator {
 public:
  explicit Animator(PatternRegistry& registry);

  // Sanitizes and adopts the settings. Returns false, changing nothing, if the
  // pattern name is not registered. Switching pattern restarts the new one and
  // tells the onboard LED.
  bool setConfig(const EyeConfig& config);

  const EyeConfig& config() const { return _config; }

  // Back to the start of the active pattern, with the onboard LED idle. Used
  // after loading saved settings so start-up does not count as a pattern change.
  void restart();

  // Advances by elapsedMs and writes the PWM duty fraction (0..1) for each
  // ring LED. While stopped, every ring LED is off and the pattern does not
  // advance. The onboard LED keeps following its own mode either way.
  void render(uint32_t elapsedMs, float out[LED_COUNT]);

  // PWM duty fraction (0..1) for the onboard LED, as of the last render().
  float onboardLevel() const { return _onboardLevel; }

 private:
  PatternRegistry& _registry;
  EyeConfig _config;
  Pattern* _active;
  OnboardLogic _onboard;
  float _onboardLevel = 0.0f;
};

}  // namespace eye
