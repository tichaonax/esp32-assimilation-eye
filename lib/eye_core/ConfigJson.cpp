#include "ConfigJson.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace eye {
namespace json {

namespace {

constexpr char DIRECTION_CLOCKWISE[] = "clockwise";
constexpr char DIRECTION_COUNTER[] = "counterClockwise";

struct ModeName {
  OnboardMode mode;
  const char* name;
};
const ModeName MODE_NAMES[] = {
    {OnboardMode::Off, "off"},
    {OnboardMode::On, "on"},
    {OnboardMode::Heartbeat, "heartbeat"},
    {OnboardMode::BlinkOnPatternChange, "blinkOnPatternChange"},
};

const char* modeName(OnboardMode mode) {
  for (const ModeName& entry : MODE_NAMES) {
    if (entry.mode == mode) {
      return entry.name;
    }
  }
  return MODE_NAMES[0].name;
}

// Reads one group of settings and collects the first problem found.
class Reader {
 public:
  Reader(JsonObjectConst object, const char* prefix, char* error) : _object(object), _prefix(prefix), _error(error) {}

  bool flag(const char* key, bool& out) {
    JsonVariantConst value = _object[key];
    if (value.isNull()) {
      return true;
    }
    if (!value.is<bool>()) {
      return fail(key, "must be true or false");
    }
    out = value.as<bool>();
    return true;
  }

  bool number(const char* key, int32_t& out) {
    JsonVariantConst value = _object[key];
    if (value.isNull()) {
      return true;
    }
    if (value.is<bool>() || !value.is<double>()) {
      return fail(key, "must be a number");
    }
    double number = value.as<double>();
    if (isnan(number)) {
      return fail(key, "must be a number");
    }
    // Far outside every setting's range, and safely inside int32_t.
    const double bound = 1000000000.0;
    if (number > bound) {
      number = bound;
    } else if (number < -bound) {
      number = -bound;
    }
    out = static_cast<int32_t>(lround(number));
    return true;
  }

  // Leaves out null if the key is absent.
  bool text(const char* key, const char*& out) {
    out = nullptr;
    JsonVariantConst value = _object[key];
    if (value.isNull()) {
      return true;
    }
    if (!value.is<const char*>()) {
      return fail(key, "must be text");
    }
    out = value.as<const char*>();
    return true;
  }

  bool fail(const char* key, const char* problem) {
    snprintf(_error, ERROR_SIZE, "%s%s %s", _prefix, key, problem);
    return false;
  }

 private:
  JsonObjectConst _object;
  const char* _prefix;
  char* _error;
};

// Finds a nested group. Absent is fine; present but not an object is an error.
bool group(JsonObjectConst root, const char* key, JsonObjectConst& out, bool& present, char* error) {
  JsonVariantConst value = root[key];
  present = !value.isNull();
  if (!present) {
    return true;
  }
  if (!value.is<JsonObjectConst>()) {
    snprintf(error, ERROR_SIZE, "%s must be an object", key);
    return false;
  }
  out = value.as<JsonObjectConst>();
  return true;
}

bool readComet(JsonObjectConst object, CometSettings& comet, char* error) {
  Reader reader(object, "comet.", error);
  const char* direction = nullptr;
  if (!reader.text("direction", direction)) {
    return false;
  }
  if (direction != nullptr) {
    if (strcmp(direction, DIRECTION_CLOCKWISE) == 0) {
      comet.direction = Direction::Clockwise;
    } else if (strcmp(direction, DIRECTION_COUNTER) == 0) {
      comet.direction = Direction::CounterClockwise;
    } else {
      return reader.fail("direction", "must be clockwise or counterClockwise");
    }
  }
  return reader.number("stepIntervalMs", comet.stepIntervalMs) &&
         reader.number("headBrightness", comet.headBrightness) && reader.number("tailLength", comet.tailLength) &&
         reader.number("tailFalloff", comet.tailFalloff);
}

bool readFlash(JsonObjectConst object, FlashSettings& flash, char* error) {
  Reader reader(object, "flash.", error);
  return reader.number("intervalMs", flash.intervalMs) && reader.number("durationMs", flash.durationMs) &&
         reader.number("minBrightness", flash.minBrightness) && reader.number("maxBrightness", flash.maxBrightness) &&
         reader.number("rampUpMs", flash.rampUpMs) && reader.number("rampDownMs", flash.rampDownMs);
}

bool readOnboard(JsonObjectConst object, OnboardSettings& onboard, char* error) {
  Reader reader(object, "onboard.", error);
  const char* mode = nullptr;
  if (!reader.text("mode", mode)) {
    return false;
  }
  if (mode != nullptr) {
    bool found = false;
    for (const ModeName& entry : MODE_NAMES) {
      if (strcmp(mode, entry.name) == 0) {
        onboard.mode = entry.mode;
        found = true;
        break;
      }
    }
    if (!found) {
      return reader.fail("mode", "must be off, on, heartbeat or blinkOnPatternChange");
    }
  }
  return reader.number("brightness", onboard.brightness) && reader.number("blinkCount", onboard.blinkCount) &&
         reader.number("blinkMs", onboard.blinkMs);
}

}  // namespace

void write(const EyeConfig& config, JsonObject target) {
  target["running"] = config.running;
  // As a pointer, not as the char array: ArduinoJson treats an array as a
  // string literal and keeps only its address, which would dangle as soon as
  // the caller's config goes away. A const char* is copied into the document.
  target["pattern"] = static_cast<const char*>(config.pattern);
  target["brightness"] = config.brightness;

  JsonObject comet = target["comet"].to<JsonObject>();
  comet["direction"] = config.comet.direction == Direction::CounterClockwise ? DIRECTION_COUNTER : DIRECTION_CLOCKWISE;
  comet["stepIntervalMs"] = config.comet.stepIntervalMs;
  comet["headBrightness"] = config.comet.headBrightness;
  comet["tailLength"] = config.comet.tailLength;
  comet["tailFalloff"] = config.comet.tailFalloff;

  JsonObject flash = target["flash"].to<JsonObject>();
  flash["intervalMs"] = config.flash.intervalMs;
  flash["durationMs"] = config.flash.durationMs;
  flash["minBrightness"] = config.flash.minBrightness;
  flash["maxBrightness"] = config.flash.maxBrightness;
  flash["rampUpMs"] = config.flash.rampUpMs;
  flash["rampDownMs"] = config.flash.rampDownMs;

  JsonObject onboard = target["onboard"].to<JsonObject>();
  onboard["mode"] = modeName(config.onboard.mode);
  onboard["brightness"] = config.onboard.brightness;
  onboard["blinkCount"] = config.onboard.blinkCount;
  onboard["blinkMs"] = config.onboard.blinkMs;
}

bool read(JsonVariantConst source, EyeConfig& config, char error[ERROR_SIZE]) {
  error[0] = '\0';
  if (!source.is<JsonObjectConst>()) {
    snprintf(error, ERROR_SIZE, "settings must be a JSON object");
    return false;
  }
  const JsonObjectConst root = source.as<JsonObjectConst>();

  // Work on a copy so a bad value part-way through changes nothing.
  EyeConfig next = config;
  Reader top(root, "", error);

  const char* pattern = nullptr;
  if (!top.flag("running", next.running) || !top.text("pattern", pattern) ||
      !top.number("brightness", next.brightness)) {
    return false;
  }
  if (pattern != nullptr) {
    const size_t length = strlen(pattern);
    if (length == 0 || length >= PATTERN_NAME_SIZE) {
      return top.fail("pattern", "is not a known pattern");
    }
    memcpy(next.pattern, pattern, length + 1);
  }

  JsonObjectConst object;
  bool present = false;
  if (!group(root, "comet", object, present, error) || (present && !readComet(object, next.comet, error))) {
    return false;
  }
  if (!group(root, "flash", object, present, error) || (present && !readFlash(object, next.flash, error))) {
    return false;
  }
  if (!group(root, "onboard", object, present, error) || (present && !readOnboard(object, next.onboard, error))) {
    return false;
  }

  config = next;
  return true;
}

}  // namespace json
}  // namespace eye
