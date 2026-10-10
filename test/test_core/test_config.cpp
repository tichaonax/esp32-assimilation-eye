// EyeConfig: defaults and the validate-and-clamp rules.

#include <string.h>

#include <unity.h>

#include "EyeConfig.h"
#include "tests.h"

using eye::EyeConfig;

static void test_defaults_match_the_settings_table() {
  const EyeConfig c = eye::defaultConfig();
  TEST_ASSERT_TRUE(c.running);
  TEST_ASSERT_EQUAL_STRING("comet", c.pattern);
  TEST_ASSERT_EQUAL_INT32(60, c.brightness);

  TEST_ASSERT_TRUE(c.comet.direction == eye::Direction::Clockwise);
  TEST_ASSERT_EQUAL_INT32(90, c.comet.stepIntervalMs);
  TEST_ASSERT_EQUAL_INT32(100, c.comet.headBrightness);
  TEST_ASSERT_EQUAL_INT32(4, c.comet.tailLength);
  TEST_ASSERT_EQUAL_INT32(50, c.comet.tailFalloff);

  TEST_ASSERT_EQUAL_INT32(1000, c.flash.intervalMs);
  TEST_ASSERT_EQUAL_INT32(200, c.flash.durationMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.minBrightness);
  TEST_ASSERT_EQUAL_INT32(100, c.flash.maxBrightness);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.rampUpMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.rampDownMs);

  TEST_ASSERT_EQUAL_INT32(8000, c.blend.cometMs);
  TEST_ASSERT_EQUAL_INT32(6000, c.blend.flashMs);
  TEST_ASSERT_EQUAL_INT32(1500, c.blend.crossfadeMs);
  TEST_ASSERT_TRUE(c.blend.randomDirection);

  TEST_ASSERT_TRUE(c.onboard.mode == eye::OnboardMode::BlinkOnPatternChange);
  TEST_ASSERT_EQUAL_INT32(50, c.onboard.brightness);
  TEST_ASSERT_EQUAL_INT32(2, c.onboard.blinkCount);
  TEST_ASSERT_EQUAL_INT32(120, c.onboard.blinkMs);
}

static void test_defaults_are_already_valid() {
  EyeConfig c = eye::defaultConfig();
  TEST_ASSERT_FALSE(eye::sanitize(c));
}

static void test_values_below_range_are_raised() {
  EyeConfig c = eye::defaultConfig();
  c.brightness = -5;
  c.comet.stepIntervalMs = 0;
  c.comet.headBrightness = -1;
  c.comet.tailLength = -3;
  c.comet.tailFalloff = 0;
  c.flash.intervalMs = 5;
  c.flash.durationMs = -10;
  c.flash.rampUpMs = -1;
  c.flash.rampDownMs = -1;
  c.blend.cometMs = 999;
  c.blend.flashMs = 0;
  c.blend.crossfadeMs = -1;
  c.onboard.blinkCount = 0;
  c.onboard.blinkMs = 1;

  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_INT32(0, c.brightness);
  TEST_ASSERT_EQUAL_INT32(20, c.comet.stepIntervalMs);
  TEST_ASSERT_EQUAL_INT32(0, c.comet.headBrightness);
  TEST_ASSERT_EQUAL_INT32(0, c.comet.tailLength);
  TEST_ASSERT_EQUAL_INT32(10, c.comet.tailFalloff);
  TEST_ASSERT_EQUAL_INT32(100, c.flash.intervalMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.durationMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.rampUpMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.rampDownMs);
  TEST_ASSERT_EQUAL_INT32(1000, c.blend.cometMs);
  TEST_ASSERT_EQUAL_INT32(1000, c.blend.flashMs);
  TEST_ASSERT_EQUAL_INT32(0, c.blend.crossfadeMs);
  TEST_ASSERT_EQUAL_INT32(1, c.onboard.blinkCount);
  TEST_ASSERT_EQUAL_INT32(50, c.onboard.blinkMs);
}

static void test_values_above_range_are_lowered() {
  EyeConfig c = eye::defaultConfig();
  c.brightness = 250;
  c.comet.stepIntervalMs = 99999;
  c.comet.headBrightness = 101;
  c.comet.tailLength = 8;
  c.comet.tailFalloff = 100;
  c.flash.intervalMs = 60000;
  c.flash.maxBrightness = 400;
  c.blend.cometMs = 60001;
  c.blend.flashMs = 1000000;
  c.blend.crossfadeMs = 5001;
  c.onboard.brightness = 101;
  c.onboard.blinkCount = 9;
  c.onboard.blinkMs = 5000;

  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_INT32(100, c.brightness);
  TEST_ASSERT_EQUAL_INT32(2000, c.comet.stepIntervalMs);
  TEST_ASSERT_EQUAL_INT32(100, c.comet.headBrightness);
  TEST_ASSERT_EQUAL_INT32(7, c.comet.tailLength);
  TEST_ASSERT_EQUAL_INT32(90, c.comet.tailFalloff);
  TEST_ASSERT_EQUAL_INT32(10000, c.flash.intervalMs);
  TEST_ASSERT_EQUAL_INT32(100, c.flash.maxBrightness);
  TEST_ASSERT_EQUAL_INT32(60000, c.blend.cometMs);
  TEST_ASSERT_EQUAL_INT32(60000, c.blend.flashMs);
  TEST_ASSERT_EQUAL_INT32(5000, c.blend.crossfadeMs);
  TEST_ASSERT_EQUAL_INT32(100, c.onboard.brightness);
  TEST_ASSERT_EQUAL_INT32(5, c.onboard.blinkCount);
  TEST_ASSERT_EQUAL_INT32(500, c.onboard.blinkMs);
}

static void test_flash_minimum_cannot_exceed_maximum() {
  EyeConfig c = eye::defaultConfig();
  c.flash.minBrightness = 80;
  c.flash.maxBrightness = 30;
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_INT32(30, c.flash.minBrightness);
  TEST_ASSERT_EQUAL_INT32(30, c.flash.maxBrightness);
}

static void test_flash_hold_is_shortened_to_fit_the_interval() {
  EyeConfig c = eye::defaultConfig();
  c.flash.intervalMs = 1000;
  c.flash.rampUpMs = 300;
  c.flash.durationMs = 900;
  c.flash.rampDownMs = 200;
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_INT32(300, c.flash.rampUpMs);
  TEST_ASSERT_EQUAL_INT32(500, c.flash.durationMs);
  TEST_ASSERT_EQUAL_INT32(200, c.flash.rampDownMs);
}

static void test_flash_ramps_are_scaled_when_they_alone_do_not_fit() {
  EyeConfig c = eye::defaultConfig();
  c.flash.intervalMs = 1000;
  c.flash.rampUpMs = 3000;
  c.flash.durationMs = 200;
  c.flash.rampDownMs = 1000;
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_INT32(750, c.flash.rampUpMs);
  TEST_ASSERT_EQUAL_INT32(0, c.flash.durationMs);
  TEST_ASSERT_EQUAL_INT32(250, c.flash.rampDownMs);
}

static void test_flash_timing_that_exactly_fills_the_interval_is_kept() {
  EyeConfig c = eye::defaultConfig();
  c.flash.intervalMs = 1000;
  c.flash.rampUpMs = 250;
  c.flash.durationMs = 500;
  c.flash.rampDownMs = 250;
  TEST_ASSERT_FALSE(eye::sanitize(c));
}

static void test_unknown_enum_values_fall_back_to_defaults() {
  EyeConfig c = eye::defaultConfig();
  c.comet.direction = static_cast<eye::Direction>(7);
  c.onboard.mode = static_cast<eye::OnboardMode>(42);
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_TRUE(c.comet.direction == eye::Direction::Clockwise);
  TEST_ASSERT_TRUE(c.onboard.mode == eye::OnboardMode::BlinkOnPatternChange);
}

static void test_pattern_name_is_always_terminated_and_never_empty() {
  EyeConfig c = eye::defaultConfig();
  memset(c.pattern, 'x', sizeof(c.pattern));
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_UINT(eye::PATTERN_NAME_SIZE - 1, strlen(c.pattern));

  c.pattern[0] = '\0';
  TEST_ASSERT_TRUE(eye::sanitize(c));
  TEST_ASSERT_EQUAL_STRING("comet", c.pattern);
}

void runConfigTests() {
  RUN_TEST(test_defaults_match_the_settings_table);
  RUN_TEST(test_defaults_are_already_valid);
  RUN_TEST(test_values_below_range_are_raised);
  RUN_TEST(test_values_above_range_are_lowered);
  RUN_TEST(test_flash_minimum_cannot_exceed_maximum);
  RUN_TEST(test_flash_hold_is_shortened_to_fit_the_interval);
  RUN_TEST(test_flash_ramps_are_scaled_when_they_alone_do_not_fit);
  RUN_TEST(test_flash_timing_that_exactly_fills_the_interval_is_kept);
  RUN_TEST(test_unknown_enum_values_fall_back_to_defaults);
  RUN_TEST(test_pattern_name_is_always_terminated_and_never_empty);
}
