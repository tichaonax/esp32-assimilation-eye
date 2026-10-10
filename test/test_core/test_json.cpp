// ConfigJson: settings to and from JSON, partial updates and type checking.

#include <string.h>

#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "ConfigJson.h"
#include "tests.h"

using eye::EyeConfig;

static std::string toText(const EyeConfig& config) {
  JsonDocument doc;
  eye::json::write(config, doc.to<JsonObject>());
  std::string text;
  serializeJson(doc, text);
  return text;
}

// Applies a JSON text to config. Returns what read() returned.
static bool apply(const char* text, EyeConfig& config, char* error) {
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(deserializeJson(doc, text) == DeserializationError::Ok, "test JSON did not parse");
  return eye::json::read(doc.as<JsonVariantConst>(), config, error);
}

static void test_json_writes_every_setting() {
  JsonDocument doc;
  eye::json::write(eye::defaultConfig(), doc.to<JsonObject>());

  TEST_ASSERT_TRUE(doc["running"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("comet", doc["pattern"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(60, doc["brightness"].as<int>());
  TEST_ASSERT_EQUAL_STRING("clockwise", doc["comet"]["direction"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(90, doc["comet"]["stepIntervalMs"].as<int>());
  TEST_ASSERT_EQUAL_INT(4, doc["comet"]["tailLength"].as<int>());
  TEST_ASSERT_EQUAL_INT(1000, doc["flash"]["intervalMs"].as<int>());
  TEST_ASSERT_EQUAL_INT(100, doc["flash"]["maxBrightness"].as<int>());
  TEST_ASSERT_EQUAL_INT(8000, doc["blend"]["cometMs"].as<int>());
  TEST_ASSERT_EQUAL_INT(6000, doc["blend"]["flashMs"].as<int>());
  TEST_ASSERT_EQUAL_INT(1500, doc["blend"]["crossfadeMs"].as<int>());
  TEST_ASSERT_TRUE(doc["blend"]["randomDirection"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("blinkOnPatternChange", doc["onboard"]["mode"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(120, doc["onboard"]["blinkMs"].as<int>());
}

static void test_json_document_does_not_depend_on_the_config_staying_alive() {
  JsonDocument doc;
  {
    EyeConfig* scoped = new EyeConfig(eye::defaultConfig());
    strcpy(scoped->pattern, "flash");
    eye::json::write(*scoped, doc.to<JsonObject>());
    // Overwrite and free it, as happens to a temporary on the device.
    memset(scoped->pattern, '#', sizeof(scoped->pattern) - 1);
    delete scoped;
  }
  TEST_ASSERT_EQUAL_STRING("flash", doc["pattern"].as<const char*>());
}

static void test_json_round_trip_keeps_every_setting() {
  EyeConfig original = eye::defaultConfig();
  original.running = false;
  strcpy(original.pattern, "flash");
  original.brightness = 37;
  original.comet.direction = eye::Direction::CounterClockwise;
  original.comet.stepIntervalMs = 250;
  original.comet.headBrightness = 80;
  original.comet.tailLength = 6;
  original.comet.tailFalloff = 30;
  original.flash.intervalMs = 2500;
  original.flash.durationMs = 300;
  original.flash.minBrightness = 5;
  original.flash.maxBrightness = 95;
  original.flash.rampUpMs = 400;
  original.flash.rampDownMs = 600;
  original.blend.cometMs = 12000;
  original.blend.flashMs = 3000;
  original.blend.crossfadeMs = 750;
  original.blend.randomDirection = false;
  original.onboard.mode = eye::OnboardMode::Heartbeat;
  original.onboard.brightness = 20;
  original.onboard.blinkCount = 4;
  original.onboard.blinkMs = 80;

  const std::string text = toText(original);
  EyeConfig restored = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE(apply(text.c_str(), restored, error));
  TEST_ASSERT_EQUAL_STRING(text.c_str(), toText(restored).c_str());
}

static void test_json_partial_update_changes_only_what_is_named() {
  EyeConfig c = eye::defaultConfig();
  const std::string before = toText(c);
  char error[eye::json::ERROR_SIZE];

  TEST_ASSERT_TRUE(apply("{\"comet\":{\"tailLength\":2}}", c, error));
  TEST_ASSERT_EQUAL_INT32(2, c.comet.tailLength);
  c.comet.tailLength = 4;
  TEST_ASSERT_EQUAL_STRING(before.c_str(), toText(c).c_str());

  TEST_ASSERT_TRUE(apply("{}", c, error));
  TEST_ASSERT_EQUAL_STRING(before.c_str(), toText(c).c_str());
}

static void test_json_settings_saved_before_the_combined_pattern_still_load() {
  // What firmware 0.6.0 wrote to flash: no "blend" group at all.
  const char* saved =
      "{\"running\":true,\"pattern\":\"flash\",\"brightness\":45,"
      "\"comet\":{\"direction\":\"counterClockwise\",\"stepIntervalMs\":120,\"headBrightness\":90,"
      "\"tailLength\":3,\"tailFalloff\":40},"
      "\"flash\":{\"intervalMs\":800,\"durationMs\":100,\"minBrightness\":0,\"maxBrightness\":100,"
      "\"rampUpMs\":50,\"rampDownMs\":50},"
      "\"onboard\":{\"mode\":\"heartbeat\",\"brightness\":30,\"blinkCount\":2,\"blinkMs\":120}}";
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE(apply(saved, c, error));
  TEST_ASSERT_EQUAL_STRING("flash", c.pattern);
  TEST_ASSERT_EQUAL_INT32(120, c.comet.stepIntervalMs);
  TEST_ASSERT_EQUAL_INT32(8000, c.blend.cometMs);
  TEST_ASSERT_EQUAL_INT32(6000, c.blend.flashMs);
  TEST_ASSERT_EQUAL_INT32(1500, c.blend.crossfadeMs);
  TEST_ASSERT_TRUE(c.blend.randomDirection);
}

static void test_json_combined_pattern_settings_are_read_and_checked() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE(apply("{\"blend\":{\"cometMs\":2000,\"randomDirection\":false}}", c, error));
  TEST_ASSERT_EQUAL_INT32(2000, c.blend.cometMs);
  TEST_ASSERT_EQUAL_INT32(6000, c.blend.flashMs);
  TEST_ASSERT_FALSE(c.blend.randomDirection);

  TEST_ASSERT_FALSE(apply("{\"blend\":{\"randomDirection\":1}}", c, error));
  TEST_ASSERT_EQUAL_STRING("blend.randomDirection must be true or false", error);
  TEST_ASSERT_FALSE(apply("{\"blend\":{\"crossfadeMs\":\"slow\"}}", c, error));
  TEST_ASSERT_EQUAL_STRING("blend.crossfadeMs must be a number", error);
  TEST_ASSERT_FALSE(apply("{\"blend\":5}", c, error));
  TEST_ASSERT_EQUAL_STRING("blend must be an object", error);
  TEST_ASSERT_EQUAL_INT32(2000, c.blend.cometMs);
}

static void test_json_unknown_keys_are_ignored() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE(apply("{\"sparkle\":1,\"comet\":{\"wobble\":true},\"brightness\":10}", c, error));
  TEST_ASSERT_EQUAL_INT32(10, c.brightness);
}

static void test_json_numbers_are_rounded_and_left_for_sanitize_to_clamp() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE(apply("{\"brightness\":41.6,\"comet\":{\"stepIntervalMs\":1e30},\"flash\":{\"rampUpMs\":-1e30}}", c,
                         error));
  TEST_ASSERT_EQUAL_INT32(42, c.brightness);
  TEST_ASSERT_TRUE(c.comet.stepIntervalMs > 2000);
  TEST_ASSERT_TRUE(c.flash.rampUpMs < 0);

  eye::sanitize(c);
  TEST_ASSERT_EQUAL_INT32(2000, c.comet.stepIntervalMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.rampUpMs);
}

static void test_json_wrong_types_are_rejected_and_nothing_changes() {
  const char* bad[] = {
      "{\"brightness\":\"bright\"}",
      "{\"brightness\":true}",
      "{\"running\":1}",
      "{\"pattern\":5}",
      "{\"comet\":7}",
      "{\"comet\":{\"tailLength\":\"long\"}}",
      "{\"comet\":{\"direction\":3}}",
      "{\"flash\":[1,2]}",
      "{\"flash\":{\"intervalMs\":{}}}",
      "{\"onboard\":{\"mode\":false}}",
      // A good value first, to prove it is not applied when a later one is bad.
      "{\"brightness\":5,\"onboard\":{\"blinkCount\":\"two\"}}",
  };
  for (const char* text : bad) {
    EyeConfig c = eye::defaultConfig();
    const std::string before = toText(c);
    char error[eye::json::ERROR_SIZE];
    TEST_ASSERT_FALSE_MESSAGE(apply(text, c, error), text);
    TEST_ASSERT_TRUE_MESSAGE(strlen(error) > 0, text);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(before.c_str(), toText(c).c_str(), text);
  }
}

static void test_json_error_names_the_setting() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_FALSE(apply("{\"comet\":{\"tailLength\":\"long\"}}", c, error));
  TEST_ASSERT_EQUAL_STRING("comet.tailLength must be a number", error);
}

static void test_json_unknown_direction_and_mode_are_rejected() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_FALSE(apply("{\"comet\":{\"direction\":\"sideways\"}}", c, error));
  TEST_ASSERT_FALSE(apply("{\"onboard\":{\"mode\":\"disco\"}}", c, error));
  TEST_ASSERT_TRUE(apply("{\"comet\":{\"direction\":\"counterClockwise\"},\"onboard\":{\"mode\":\"heartbeat\"}}", c,
                         error));
  TEST_ASSERT_TRUE(c.comet.direction == eye::Direction::CounterClockwise);
  TEST_ASSERT_TRUE(c.onboard.mode == eye::OnboardMode::Heartbeat);
}

static void test_json_pattern_name_must_fit() {
  EyeConfig c = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_FALSE(apply("{\"pattern\":\"\"}", c, error));
  TEST_ASSERT_FALSE(apply("{\"pattern\":\"a-very-long-pattern-name\"}", c, error));
  TEST_ASSERT_EQUAL_STRING("comet", c.pattern);
  // Whether a name that fits is a real pattern is the registry's question.
  TEST_ASSERT_TRUE(apply("{\"pattern\":\"flash\"}", c, error));
  TEST_ASSERT_EQUAL_STRING("flash", c.pattern);
}

static void test_json_anything_but_an_object_is_rejected() {
  const char* bad[] = {"[1,2,3]", "\"comet\"", "42", "null", "true"};
  for (const char* text : bad) {
    EyeConfig c = eye::defaultConfig();
    char error[eye::json::ERROR_SIZE];
    TEST_ASSERT_FALSE_MESSAGE(apply(text, c, error), text);
  }
}

void runJsonTests() {
  RUN_TEST(test_json_writes_every_setting);
  RUN_TEST(test_json_document_does_not_depend_on_the_config_staying_alive);
  RUN_TEST(test_json_round_trip_keeps_every_setting);
  RUN_TEST(test_json_partial_update_changes_only_what_is_named);
  RUN_TEST(test_json_settings_saved_before_the_combined_pattern_still_load);
  RUN_TEST(test_json_combined_pattern_settings_are_read_and_checked);
  RUN_TEST(test_json_unknown_keys_are_ignored);
  RUN_TEST(test_json_numbers_are_rounded_and_left_for_sanitize_to_clamp);
  RUN_TEST(test_json_wrong_types_are_rejected_and_nothing_changes);
  RUN_TEST(test_json_error_names_the_setting);
  RUN_TEST(test_json_unknown_direction_and_mode_are_rejected);
  RUN_TEST(test_json_pattern_name_must_fit);
  RUN_TEST(test_json_anything_but_an_object_is_rejected);
}
