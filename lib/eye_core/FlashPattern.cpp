#include "FlashPattern.h"

namespace eye {

float FlashPattern::levelAt(const FlashSettings& flash, uint32_t phaseMs) {
  const float low = static_cast<float>(flash.minBrightness) / 100.0f;
  const float high = static_cast<float>(flash.maxBrightness) / 100.0f;
  const uint32_t rampUp = flash.rampUpMs > 0 ? static_cast<uint32_t>(flash.rampUpMs) : 0;
  const uint32_t hold = flash.durationMs > 0 ? static_cast<uint32_t>(flash.durationMs) : 0;
  const uint32_t rampDown = flash.rampDownMs > 0 ? static_cast<uint32_t>(flash.rampDownMs) : 0;

  uint32_t t = phaseMs;
  if (t < rampUp) {
    return low + (high - low) * static_cast<float>(t) / static_cast<float>(rampUp);
  }
  t -= rampUp;
  if (t < hold) {
    return high;
  }
  t -= hold;
  if (t < rampDown) {
    return high - (high - low) * static_cast<float>(t) / static_cast<float>(rampDown);
  }
  return low;
}

void FlashPattern::render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) {
  const uint32_t interval = config.flash.intervalMs > 0 ? static_cast<uint32_t>(config.flash.intervalMs) : 1;
  // Reduce both before adding so a very long gap cannot overflow.
  _phaseMs = (_phaseMs % interval + elapsedMs % interval) % interval;

  const float level = levelAt(config.flash, _phaseMs);
  for (size_t i = 0; i < LED_COUNT; i++) {
    out[i] = level;
  }
}

}  // namespace eye
