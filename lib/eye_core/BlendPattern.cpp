#include "BlendPattern.h"

#include <math.h>

namespace eye {

namespace {

// Eases in and out, so a crossfade has no visible start or stop.
float smooth(float t) {
  if (t <= 0.0f) {
    return 0.0f;
  }
  if (t >= 1.0f) {
    return 1.0f;
  }
  return t * t * (3.0f - 2.0f * t);
}

float positive(int32_t value) {
  return value > 0 ? static_cast<float>(value) : 0.0f;
}

}  // namespace

void BlendPattern::reset() {
  _comet.reset();
  _flash.reset();
  _stage = Stage::Comet;
  _stageMs = 0.0f;
  _fade = 0.0f;
  _hasDirection = false;
}

void BlendPattern::seed(uint32_t value) {
  // The generator below never leaves zero.
  _random = value != 0 ? value : 0x9E3779B9u;
}

uint32_t BlendPattern::nextRandom() {
  // xorshift32: small, fast and plenty for a coin toss.
  _random ^= _random << 13;
  _random ^= _random >> 17;
  _random ^= _random << 5;
  return _random;
}

float BlendPattern::mix() const {
  switch (_stage) {
    case Stage::Comet:
      return 0.0f;
    case Stage::ToFlash:
      return smooth(_fade);
    case Stage::Flash:
      return 1.0f;
    case Stage::ToComet:
      return 1.0f - smooth(_fade);
  }
  return 0.0f;
}

void BlendPattern::enter(Stage stage, bool randomDirection) {
  _stage = stage;
  _stageMs = 0.0f;
  _fade = 0.0f;
  if (stage == Stage::Flash && randomDirection) {
    // The comet is fully hidden from here until the next crossfade begins.
    _direction = (nextRandom() & 0x10000u) != 0 ? Direction::Clockwise : Direction::CounterClockwise;
  }
}

void BlendPattern::advance(const BlendSettings& blend, uint32_t elapsedMs) {
  const float cometMs = positive(blend.cometMs);
  const float flashMs = positive(blend.flashMs);
  const float crossfadeMs = positive(blend.crossfadeMs);
  const float cycleMs = cometMs + flashMs + 2.0f * crossfadeMs;
  if (cycleMs <= 0.0f) {
    return;
  }

  // After a very long gap only the place in the cycle matters.
  float remaining = static_cast<float>(elapsedMs);
  if (remaining >= cycleMs) {
    remaining = fmodf(remaining, cycleMs);
  }

  // At most one trip round the four stages, plus the one it started in.
  for (int step = 0; step < 5 && remaining > 0.0f; step++) {
    const bool holding = _stage == Stage::Comet || _stage == Stage::Flash;
    float room;
    if (holding) {
      // Zero if the time was just shortened below where the stage already is.
      const float length = _stage == Stage::Comet ? cometMs : flashMs;
      room = length > _stageMs ? length - _stageMs : 0.0f;
    } else {
      room = (1.0f - _fade) * crossfadeMs;
    }

    if (remaining < room) {
      if (holding) {
        _stageMs += remaining;
      } else {
        _fade += remaining / crossfadeMs;
      }
      return;
    }
    remaining -= room;
    enter(static_cast<Stage>((static_cast<uint8_t>(_stage) + 1) % 4), blend.randomDirection);
  }
}

void BlendPattern::render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) {
  if (!config.blend.randomDirection || !_hasDirection) {
    _direction = config.comet.direction;
    _hasDirection = true;
  }
  advance(config.blend, elapsedMs);

  // Both always advance, seen or not.
  EyeConfig cometConfig = config;
  cometConfig.comet.direction = _direction;
  float comet[LED_COUNT];
  float flash[LED_COUNT];
  _comet.render(cometConfig, elapsedMs, comet);
  _flash.render(config, elapsedMs, flash);

  const float flashShare = mix();
  for (size_t i = 0; i < LED_COUNT; i++) {
    out[i] = comet[i] + (flash[i] - comet[i]) * flashShare;
  }
}

}  // namespace eye
