// Behaviour of the board's own LED, kept apart from the ring: off, on,
// heartbeat, or a short burst of blinks whenever the ring pattern changes.
#pragma once

#include <stdint.h>

#include "EyeConfig.h"

namespace eye {

class OnboardLogic {
 public:
  // Heartbeat: two short beats, then a rest.
  static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1200;
  static constexpr uint32_t HEARTBEAT_BEAT_MS = 120;
  static constexpr uint32_t HEARTBEAT_GAP_MS = 120;

  void reset();

  // Starts a burst of blinks if the mode is "blink on pattern change".
  void notifyPatternChange();

  // Advances by elapsedMs and returns the perceived brightness, 0..1.
  float update(const OnboardSettings& settings, uint32_t elapsedMs);

 private:
  uint32_t _heartbeatMs = 0;
  bool _blinking = false;
  uint32_t _blinkMs = 0;
};

}  // namespace eye
