#include "CometPattern.h"

#include <math.h>

namespace eye {

namespace {

constexpr float RING = static_cast<float>(LED_COUNT);

float wrap(float value) {
  value = fmodf(value, RING);
  if (value < 0.0f) {
    value += RING;
  }
  // fmodf can return RING itself after the correction for tiny negatives.
  return value >= RING ? 0.0f : value;
}

// Brightness of the comet at a whole number of LEDs behind the head.
float levelAt(int ledsBehind, float head, float keep, int tail) {
  if (ledsBehind < 0 || ledsBehind > tail) {
    return 0.0f;
  }
  return head * powf(keep, static_cast<float>(ledsBehind));
}

// Brightness at any distance behind the head (negative = ahead of it).
// Straight lines between the whole-LED levels, falling to zero one LED ahead
// of the head and one LED past the end of the tail, so the comet glides
// instead of jumping from LED to LED.
float profile(float behind, float head, float keep, int tail) {
  if (behind <= -1.0f || behind >= static_cast<float>(tail + 1)) {
    return 0.0f;
  }
  const float whole = floorf(behind);
  const float fraction = behind - whole;
  const int k = static_cast<int>(whole);
  const float a = levelAt(k, head, keep, tail);
  const float b = levelAt(k + 1, head, keep, tail);
  return a + (b - a) * fraction;
}

}  // namespace

void CometPattern::render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) {
  const CometSettings& comet = config.comet;
  const bool clockwise = comet.direction == Direction::Clockwise;

  const float stepMs = comet.stepIntervalMs > 0 ? static_cast<float>(comet.stepIntervalMs) : 1.0f;
  const float travelled = static_cast<float>(elapsedMs) / stepMs;
  _position = wrap(_position + (clockwise ? travelled : -travelled));

  const float head = static_cast<float>(comet.headBrightness) / 100.0f;
  const float keep = 1.0f - static_cast<float>(comet.tailFalloff) / 100.0f;
  const int tail = static_cast<int>(comet.tailLength);

  for (size_t i = 0; i < LED_COUNT; i++) {
    const float led = static_cast<float>(i);
    // The tail trails behind the direction of travel.
    const float behind = clockwise ? wrap(_position - led) : wrap(led - _position);
    // Each LED is seen twice round the ring: once behind the head, and once
    // from the other side as the head approaches it. Adding the two is what
    // carries the comet smoothly from LED 8 back to LED 1.
    const float level = profile(behind, head, keep, tail) + profile(behind - RING, head, keep, tail);
    out[i] = level > 1.0f ? 1.0f : level;
  }
}

}  // namespace eye
