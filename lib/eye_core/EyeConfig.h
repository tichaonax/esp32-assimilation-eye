// LED settings: what the user can change, the factory defaults and the rules
// that keep every value in range.
//
// No Arduino calls anywhere in lib/eye_core, so it is unit-tested on the PC.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace eye {

constexpr size_t LED_COUNT = 8;
constexpr size_t PATTERN_NAME_SIZE = 16;  // including the terminator

enum class Direction : uint8_t { Clockwise = 0, CounterClockwise = 1 };

enum class OnboardMode : uint8_t { Off = 0, On = 1, Heartbeat = 2, BlinkOnPatternChange = 3 };

// Numeric settings are int32_t so a caller can store whatever it was given and
// let sanitize() bring it into range.

struct CometSettings {
  Direction direction;
  int32_t stepIntervalMs;  // time for the head to travel one LED
  int32_t headBrightness;  // percent
  int32_t tailLength;      // LEDs lit behind the head
  int32_t tailFalloff;     // percent dimmer per LED along the tail
};

struct FlashSettings {
  int32_t intervalMs;     // start of one flash to the start of the next
  int32_t durationMs;     // time held at maximum
  int32_t minBrightness;  // percent
  int32_t maxBrightness;  // percent
  int32_t rampUpMs;       // 0 = abrupt
  int32_t rampDownMs;     // 0 = abrupt
};

// The combined pattern plays the comet and the flash in turn, as each is set
// up above, and crossfades between them.
struct BlendSettings {
  int32_t cometMs;       // time showing only the comet
  int32_t flashMs;       // time showing only the flash
  int32_t crossfadeMs;   // time to fade from one to the other; 0 = cut
  bool randomDirection;  // pick the comet's direction at random each time it returns
};

struct OnboardSettings {
  OnboardMode mode;
  int32_t brightness;  // percent
  int32_t blinkCount;
  int32_t blinkMs;
};

struct EyeConfig {
  bool running;
  char pattern[PATTERN_NAME_SIZE];
  int32_t brightness;  // percent, applied on top of the pattern
  CometSettings comet;
  FlashSettings flash;
  BlendSettings blend;
  OnboardSettings onboard;
};

namespace limits {
constexpr int32_t PERCENT_MIN = 0;
constexpr int32_t PERCENT_MAX = 100;
constexpr int32_t COMET_STEP_MS_MIN = 20;
constexpr int32_t COMET_STEP_MS_MAX = 2000;
constexpr int32_t COMET_TAIL_MIN = 0;
constexpr int32_t COMET_TAIL_MAX = static_cast<int32_t>(LED_COUNT) - 1;
constexpr int32_t COMET_FALLOFF_MIN = 10;
constexpr int32_t COMET_FALLOFF_MAX = 90;
constexpr int32_t FLASH_INTERVAL_MS_MIN = 100;
constexpr int32_t FLASH_INTERVAL_MS_MAX = 10000;
constexpr int32_t FLASH_DURATION_MS_MIN = 0;
constexpr int32_t FLASH_DURATION_MS_MAX = 10000;
constexpr int32_t FLASH_RAMP_MS_MIN = 0;
constexpr int32_t FLASH_RAMP_MS_MAX = 5000;
constexpr int32_t BLEND_HOLD_MS_MIN = 1000;
constexpr int32_t BLEND_HOLD_MS_MAX = 60000;
constexpr int32_t BLEND_CROSSFADE_MS_MIN = 0;
constexpr int32_t BLEND_CROSSFADE_MS_MAX = 5000;
constexpr int32_t ONBOARD_BLINK_COUNT_MIN = 1;
constexpr int32_t ONBOARD_BLINK_COUNT_MAX = 5;
constexpr int32_t ONBOARD_BLINK_MS_MIN = 50;
constexpr int32_t ONBOARD_BLINK_MS_MAX = 500;
}  // namespace limits

constexpr char DEFAULT_PATTERN[] = "comet";

EyeConfig defaultConfig();

// Brings every value into range. Returns true if anything had to change.
//   - numbers are clamped to their limits
//   - flash minimum is lowered to the maximum if it is above it
//   - flash ramp up + duration + ramp down is fitted inside the interval:
//     the hold time is shortened first, then the two ramps are scaled down
//   - unknown direction or onboard mode values fall back to the default
//   - an empty or unterminated pattern name falls back to the default
// Whether the pattern name is a registered pattern is the registry's question.
bool sanitize(EyeConfig& config);

}  // namespace eye
