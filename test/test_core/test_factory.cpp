// FactoryConfig: the defaults a reset returns to, which are the built-in ones
// unless the build was made with a led-defaults.json.
//
// These run against whatever file this build was made with, so they pass with
// no file, and with any file the build script accepts.

#include <string.h>

#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "ConfigJson.h"
#include "FactoryConfig.h"
#include "PatternRegistry.h"
#include "tests.h"

using eye::EyeConfig;

static std::string toText(const EyeConfig& config) {
  JsonDocument doc;
  eye::json::write(config, doc.to<JsonObject>());
  std::string text;
  serializeJson(doc, text);
  return text;
}

static void test_factory_settings_are_valid() {
  EyeConfig c = eye::factoryConfig();
  // Nothing for sanitize() to correct: what the file says is what is used.
  TEST_ASSERT_FALSE(eye::sanitize(c));
}

static void test_factory_pattern_is_registered() {
  const EyeConfig c = eye::factoryConfig();
  eye::PatternRegistry registry;
  TEST_ASSERT_NOT_NULL_MESSAGE(registry.find(c.pattern), c.pattern);
}

static void test_factory_is_the_same_every_time() {
  TEST_ASSERT_EQUAL_STRING(toText(eye::factoryConfig()).c_str(), toText(eye::factoryConfig()).c_str());
}

static void test_factory_without_a_file_is_the_built_in_defaults() {
  if (eye::factoryConfigIsCustom()) {
    TEST_IGNORE_MESSAGE("this build has a led-defaults.json");
  }
  TEST_ASSERT_EQUAL_STRING("", eye::factoryConfigJson());
  TEST_ASSERT_EQUAL_STRING(toText(eye::defaultConfig()).c_str(), toText(eye::factoryConfig()).c_str());
}

static void test_factory_with_a_file_is_the_built_in_defaults_with_the_file_on_top() {
  if (!eye::factoryConfigIsCustom()) {
    TEST_IGNORE_MESSAGE("this build has no led-defaults.json");
  }
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(deserializeJson(doc, eye::factoryConfigJson()) == DeserializationError::Ok,
                           "the compiled-in led-defaults.json does not parse");

  EyeConfig expected = eye::defaultConfig();
  char error[eye::json::ERROR_SIZE];
  TEST_ASSERT_TRUE_MESSAGE(eye::json::read(doc.as<JsonVariantConst>(), expected, error), error);
  TEST_ASSERT_EQUAL_STRING(toText(expected).c_str(), toText(eye::factoryConfig()).c_str());
}

static void test_factory_file_names_only_real_settings() {
  if (!eye::factoryConfigIsCustom()) {
    TEST_IGNORE_MESSAGE("this build has no led-defaults.json");
  }
  // Writing the result back out must reproduce every value in the file: a
  // misspelt setting would be missing, and a clamped one would differ.
  JsonDocument file;
  deserializeJson(file, eye::factoryConfigJson());
  JsonDocument result;
  eye::json::write(eye::factoryConfig(), result.to<JsonObject>());

  for (JsonPairConst entry : file.as<JsonObjectConst>()) {
    const char* key = entry.key().c_str();
    if (entry.value().is<JsonObjectConst>()) {
      for (JsonPairConst inner : entry.value().as<JsonObjectConst>()) {
        TEST_ASSERT_TRUE_MESSAGE(result[key][inner.key()] == inner.value(), inner.key().c_str());
      }
    } else {
      TEST_ASSERT_TRUE_MESSAGE(result[key] == entry.value(), key);
    }
  }
}

void runFactoryTests() {
  RUN_TEST(test_factory_settings_are_valid);
  RUN_TEST(test_factory_pattern_is_registered);
  RUN_TEST(test_factory_is_the_same_every_time);
  RUN_TEST(test_factory_without_a_file_is_the_built_in_defaults);
  RUN_TEST(test_factory_with_a_file_is_the_built_in_defaults_with_the_file_on_top);
  RUN_TEST(test_factory_file_names_only_real_settings);
}
