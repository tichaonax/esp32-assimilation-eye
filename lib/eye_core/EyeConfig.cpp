#include "EyeConfig.h"

#include <string.h>

namespace eye {

namespace {

bool clampField(int32_t& value, int32_t low, int32_t high) {
  if (value < low) {
    value = low;
    return true;
  }
  if (value > high) {
    value = high;
    return true;
  }
  return false;
}

bool sanitizePatternName(char* name) {
  bool changed = false;
  if (name[PATTERN_NAME_SIZE - 1] != '\0') {
    name[PATTERN_NAME_SIZE - 1] = '\0';
    changed = true;
  }
  if (name[0] == '\0') {
    strncpy(name, DEFAULT_PATTERN, PATTERN_NAME_SIZE - 1);
    changed = true;
  }
  return changed;
}

bool sanitizeFlashTiming(FlashSettings& flash) {
  const int32_t ramps = flash.rampUpMs + flash.rampDownMs;
  if (ramps > flash.intervalMs) {
    // Even without a hold the ramps do not fit: keep their proportion.
    flash.rampUpMs = static_cast<int32_t>(static_cast<int64_t>(flash.rampUpMs) * flash.intervalMs / ramps);
    flash.rampDownMs = flash.intervalMs - flash.rampUpMs;
    flash.durationMs = 0;
    return true;
  }
  if (ramps + flash.durationMs > flash.intervalMs) {
    flash.durationMs = flash.intervalMs - ramps;
    return true;
  }
  return false;
}

}  // namespace

EyeConfig defaultConfig() {
  EyeConfig config = {};
  config.running = true;
  strncpy(config.pattern, DEFAULT_PATTERN, PATTERN_NAME_SIZE - 1);
  config.brightness = 60;

  config.comet.direction = Direction::Clockwise;
  config.comet.stepIntervalMs = 90;
  config.comet.headBrightness = 100;
  config.comet.tailLength = 4;
  config.comet.tailFalloff = 50;

  config.flash.intervalMs = 1000;
  config.flash.durationMs = 200;
  config.flash.minBrightness = 0;
  config.flash.maxBrightness = 100;
  config.flash.rampUpMs = 0;
  config.flash.rampDownMs = 0;

  config.blend.cometMs = 8000;
  config.blend.flashMs = 6000;
  config.blend.crossfadeMs = 1500;
  config.blend.randomDirection = true;

  config.onboard.mode = OnboardMode::BlinkOnPatternChange;
  config.onboard.brightness = 50;
  config.onboard.blinkCount = 2;
  config.onboard.blinkMs = 120;
  return config;
}

bool sanitize(EyeConfig& config) {
  using namespace limits;
  bool changed = false;

  changed |= sanitizePatternName(config.pattern);
  changed |= clampField(config.brightness, PERCENT_MIN, PERCENT_MAX);

  if (config.comet.direction != Direction::Clockwise && config.comet.direction != Direction::CounterClockwise) {
    config.comet.direction = Direction::Clockwise;
    changed = true;
  }
  changed |= clampField(config.comet.stepIntervalMs, COMET_STEP_MS_MIN, COMET_STEP_MS_MAX);
  changed |= clampField(config.comet.headBrightness, PERCENT_MIN, PERCENT_MAX);
  changed |= clampField(config.comet.tailLength, COMET_TAIL_MIN, COMET_TAIL_MAX);
  changed |= clampField(config.comet.tailFalloff, COMET_FALLOFF_MIN, COMET_FALLOFF_MAX);

  changed |= clampField(config.flash.intervalMs, FLASH_INTERVAL_MS_MIN, FLASH_INTERVAL_MS_MAX);
  changed |= clampField(config.flash.durationMs, FLASH_DURATION_MS_MIN, FLASH_DURATION_MS_MAX);
  changed |= clampField(config.flash.minBrightness, PERCENT_MIN, PERCENT_MAX);
  changed |= clampField(config.flash.maxBrightness, PERCENT_MIN, PERCENT_MAX);
  changed |= clampField(config.flash.rampUpMs, FLASH_RAMP_MS_MIN, FLASH_RAMP_MS_MAX);
  changed |= clampField(config.flash.rampDownMs, FLASH_RAMP_MS_MIN, FLASH_RAMP_MS_MAX);
  if (config.flash.minBrightness > config.flash.maxBrightness) {
    config.flash.minBrightness = config.flash.maxBrightness;
    changed = true;
  }
  changed |= sanitizeFlashTiming(config.flash);

  changed |= clampField(config.blend.cometMs, BLEND_HOLD_MS_MIN, BLEND_HOLD_MS_MAX);
  changed |= clampField(config.blend.flashMs, BLEND_HOLD_MS_MIN, BLEND_HOLD_MS_MAX);
  changed |= clampField(config.blend.crossfadeMs, BLEND_CROSSFADE_MS_MIN, BLEND_CROSSFADE_MS_MAX);

  if (static_cast<uint8_t>(config.onboard.mode) > static_cast<uint8_t>(OnboardMode::BlinkOnPatternChange)) {
    config.onboard.mode = OnboardMode::BlinkOnPatternChange;
    changed = true;
  }
  changed |= clampField(config.onboard.brightness, PERCENT_MIN, PERCENT_MAX);
  changed |= clampField(config.onboard.blinkCount, ONBOARD_BLINK_COUNT_MIN, ONBOARD_BLINK_COUNT_MAX);
  changed |= clampField(config.onboard.blinkMs, ONBOARD_BLINK_MS_MIN, ONBOARD_BLINK_MS_MAX);

  return changed;
}

}  // namespace eye
