#include "ConfigStore.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "ConfigJson.h"

namespace {
constexpr char NVS_NAMESPACE[] = "eye";
constexpr char KEY_VERSION[] = "ver";
constexpr char KEY_SETTINGS[] = "led";
}  // namespace

ConfigStore::ConfigStore(AnimationTask& animation) : _animation(animation) {}

bool ConfigStore::load(eye::EyeConfig& config) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    return false;  // nothing has ever been saved
  }
  const uint16_t version = prefs.getUShort(KEY_VERSION, 0);
  const String text = prefs.isKey(KEY_SETTINGS) ? prefs.getString(KEY_SETTINGS) : String();
  prefs.end();

  if (version != LAYOUT_VERSION || text.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, text) != DeserializationError::Ok) {
    Serial.println(F("[store] Saved settings are not readable; ignoring them."));
    return false;
  }
  // Start from the defaults so a setting added in later firmware has a value.
  eye::EyeConfig loaded = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  if (!eye::json::read(doc.as<JsonVariantConst>(), loaded, error)) {
    Serial.printf("[store] Saved settings are not usable (%s); ignoring them.\n", error);
    return false;
  }
  eye::sanitize(loaded);
  config = loaded;
  _stored = text;
  return true;
}

void ConfigStore::requestSave() {
  const uint32_t now = millis();
  if (!_pending) {
    _firstRequestAtMs = now;
  }
  _requestedAtMs = now;
  _pending = true;
}

void ConfigStore::loop() {
  if (!_pending) {
    return;
  }
  const uint32_t now = millis();
  const bool quiet = now - _requestedAtMs >= SAVE_DELAY_MS;
  // Someone adjusting things non-stop never leaves a quiet moment; save anyway.
  const bool overdue = now - _firstRequestAtMs >= MAX_UNSAVED_MS;
  if (!quiet && !overdue) {
    return;
  }
  // Cleared before saving, so a change that arrives meanwhile asks again.
  _pending = false;
  saveNow();
}

bool ConfigStore::saveNow() {
  JsonDocument doc;
  eye::json::write(_animation.config(), doc.to<JsonObject>());
  String text;
  serializeJson(doc, text);
  if (text == _stored) {
    return true;
  }

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println(F("[store] Could not open flash storage; settings not saved."));
    return false;
  }
  const bool ok = prefs.putUShort(KEY_VERSION, LAYOUT_VERSION) > 0 && prefs.putString(KEY_SETTINGS, text) > 0;
  prefs.end();

  if (ok) {
    _stored = text;
    Serial.printf("[store] Settings saved (%u bytes).\n", static_cast<unsigned>(text.length()));
  } else {
    Serial.println(F("[store] Writing settings to flash failed."));
  }
  return ok;
}
