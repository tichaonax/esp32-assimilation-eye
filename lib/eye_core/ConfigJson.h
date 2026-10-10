// LED settings to and from JSON. The same shape is used by the API, the
// WebSocket and the copy saved in flash:
//
//   {
//     "running": true, "pattern": "comet", "brightness": 60,
//     "comet":   { "direction": "clockwise", "stepIntervalMs": 90, "headBrightness": 100,
//                  "tailLength": 4, "tailFalloff": 50 },
//     "flash":   { "intervalMs": 1000, "durationMs": 200, "minBrightness": 0,
//                  "maxBrightness": 100, "rampUpMs": 0, "rampDownMs": 0 },
//     "blend":   { "cometMs": 8000, "flashMs": 6000, "crossfadeMs": 1500, "randomDirection": true },
//     "onboard": { "mode": "blinkOnPatternChange", "brightness": 50, "blinkCount": 2, "blinkMs": 120 }
//   }
//
// direction: "clockwise" | "counterClockwise"
// mode:      "off" | "on" | "heartbeat" | "blinkOnPatternChange"
#pragma once

#include <stddef.h>

#include <ArduinoJson.h>

#include "EyeConfig.h"

namespace eye {
namespace json {

constexpr size_t ERROR_SIZE = 96;

// Writes every setting into target.
void write(const EyeConfig& config, JsonObject target);

// Applies the settings present in source on top of config; anything absent is
// left as it is, and unknown keys are ignored. Numbers are taken as given,
// rounded to whole numbers; bringing them into range is sanitize()'s job.
//
// Returns false with a message in error if a value has the wrong type or a
// direction or mode is not recognised. In that case config is left untouched.
bool read(JsonVariantConst source, EyeConfig& config, char error[ERROR_SIZE]);

}  // namespace json
}  // namespace eye
