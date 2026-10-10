#include "FactoryConfig.h"

#include <ArduinoJson.h>

#include "ConfigJson.h"

// Written by scripts/led_defaults.py before every build. It defines
// EYE_LED_DEFAULTS_JSON only if there is a led-defaults.json.
#if __has_include("generated/led_defaults.h")
#include "generated/led_defaults.h"
#endif

namespace eye {

namespace {
#ifdef EYE_LED_DEFAULTS_JSON
const char CUSTOM_JSON[] = EYE_LED_DEFAULTS_JSON;
#else
const char CUSTOM_JSON[] = "";
#endif
}  // namespace

const char* factoryConfigJson() {
  return CUSTOM_JSON;
}

bool factoryConfigIsCustom() {
  return CUSTOM_JSON[0] != '\0';
}

EyeConfig factoryConfig() {
  EyeConfig config = defaultConfig();
  if (!factoryConfigIsCustom()) {
    return config;
  }
  // The build script has already checked the file, so this is not expected
  // to fail; if it does, the built-in defaults are the safe answer.
  JsonDocument doc;
  char error[json::ERROR_SIZE];
  if (deserializeJson(doc, CUSTOM_JSON) != DeserializationError::Ok ||
      !json::read(doc.as<JsonVariantConst>(), config, error)) {
    return defaultConfig();
  }
  sanitize(config);
  return config;
}

}  // namespace eye
