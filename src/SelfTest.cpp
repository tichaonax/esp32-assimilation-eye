#include "SelfTest.h"

#include <Arduino.h>

namespace selftest {

namespace {
constexpr uint32_t STEP_MS = 200;
constexpr uint32_t FADE_MS = 600;
constexpr uint32_t FADE_TICK_MS = 5;
constexpr float STEP_LEVEL = 0.6f;
constexpr float FADE_PEAK = 0.6f;

void fade(LedDriver& leds, float from, float to) {
  const uint32_t ticks = FADE_MS / FADE_TICK_MS;
  for (uint32_t t = 0; t <= ticks; t++) {
    const float x = static_cast<float>(t) / ticks;
    // Squared, so the fade looks even to the eye until gamma correction arrives in Phase 3.
    const float level = from + (to - from) * x;
    leds.setAll(level * level);
    delay(FADE_TICK_MS);
  }
}
}  // namespace

void run(LedDriver& leds) {
  leds.off();
  for (size_t i = 0; i < hw::RING_LED_COUNT; i++) {
    leds.set(i, STEP_LEVEL);
    delay(STEP_MS);
    leds.set(i, 0.0f);
  }
  fade(leds, 0.0f, FADE_PEAK);
  fade(leds, FADE_PEAK, 0.0f);
  leds.off();
}

}  // namespace selftest
