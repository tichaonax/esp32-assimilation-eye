// FlashPattern: interval, hold, minimum and maximum, ramps and abrupt mode.

#include <unity.h>

#include "FlashPattern.h"
#include "tests.h"

using eye::EyeConfig;
using eye::FlashPattern;
using eye::LED_COUNT;

static const float TOLERANCE = 0.002f;

static EyeConfig rampedConfig() {
  EyeConfig c = eye::defaultConfig();
  c.flash.intervalMs = 1000;
  c.flash.rampUpMs = 200;
  c.flash.durationMs = 300;
  c.flash.rampDownMs = 400;
  c.flash.minBrightness = 10;
  c.flash.maxBrightness = 90;
  return c;
}

static void test_flash_abrupt_is_on_for_the_duration_then_off() {
  const EyeConfig c = eye::defaultConfig();  // 1000 ms interval, 200 ms hold, no ramps
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, FlashPattern::levelAt(c.flash, 0));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, FlashPattern::levelAt(c.flash, 199));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, FlashPattern::levelAt(c.flash, 200));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, FlashPattern::levelAt(c.flash, 999));
}

static void test_flash_ramps_up_holds_and_ramps_down() {
  const EyeConfig c = rampedConfig();
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.1f, FlashPattern::levelAt(c.flash, 0));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, FlashPattern::levelAt(c.flash, 100));  // half way up
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.9f, FlashPattern::levelAt(c.flash, 200));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.9f, FlashPattern::levelAt(c.flash, 499));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, FlashPattern::levelAt(c.flash, 700));  // half way down
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.1f, FlashPattern::levelAt(c.flash, 900));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.1f, FlashPattern::levelAt(c.flash, 999));
}

static void test_flash_ramp_never_leaves_the_min_max_band() {
  const EyeConfig c = rampedConfig();
  float previous = FlashPattern::levelAt(c.flash, 0);
  for (uint32_t t = 0; t < 1000; t++) {
    const float level = FlashPattern::levelAt(c.flash, t);
    TEST_ASSERT_TRUE(level >= 0.1f - TOLERANCE && level <= 0.9f + TOLERANCE);
    // 1 ms apart, the level never jumps: that is what makes it a ramp.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, previous, level);
    previous = level;
  }
}

static void test_flash_ramp_up_only_drops_abruptly_after_the_hold() {
  EyeConfig c = rampedConfig();
  c.flash.rampDownMs = 0;
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.9f, FlashPattern::levelAt(c.flash, 499));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.1f, FlashPattern::levelAt(c.flash, 500));
}

static void test_flash_without_hold_is_a_triangle() {
  EyeConfig c = rampedConfig();
  c.flash.durationMs = 0;
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.86f, FlashPattern::levelAt(c.flash, 190));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.9f, FlashPattern::levelAt(c.flash, 200));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.88f, FlashPattern::levelAt(c.flash, 210));
}

static void test_flash_lights_all_leds_equally() {
  const EyeConfig c = rampedConfig();
  FlashPattern flash;
  float out[LED_COUNT];
  for (int tick = 0; tick < 300; tick++) {
    flash.render(c, 7, out);
    for (size_t i = 1; i < LED_COUNT; i++) {
      TEST_ASSERT_EQUAL_FLOAT(out[0], out[i]);
    }
  }
}

static void test_flash_repeats_every_interval() {
  const EyeConfig c = rampedConfig();
  FlashPattern flash;
  float out[LED_COUNT];
  flash.render(c, 100, out);
  const float first = out[0];
  flash.render(c, 1000, out);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, first, out[0]);
  flash.render(c, 3000, out);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, first, out[0]);
  TEST_ASSERT_EQUAL_UINT32(100, flash.phaseMs());
}

static void test_flash_survives_interval_change_and_long_gaps() {
  EyeConfig c = rampedConfig();
  FlashPattern flash;
  float out[LED_COUNT];
  flash.render(c, 950, out);
  c.flash.intervalMs = 400;  // now shorter than the current phase
  flash.render(c, 0, out);
  TEST_ASSERT_TRUE(flash.phaseMs() < 400);
  flash.render(c, 4000000000u, out);
  TEST_ASSERT_TRUE(flash.phaseMs() < 400);
}

static void test_flash_reset_restarts_the_cycle() {
  const EyeConfig c = rampedConfig();
  FlashPattern flash;
  float out[LED_COUNT];
  flash.render(c, 640, out);
  flash.reset();
  TEST_ASSERT_EQUAL_UINT32(0, flash.phaseMs());
}

void runFlashTests() {
  RUN_TEST(test_flash_abrupt_is_on_for_the_duration_then_off);
  RUN_TEST(test_flash_ramps_up_holds_and_ramps_down);
  RUN_TEST(test_flash_ramp_never_leaves_the_min_max_band);
  RUN_TEST(test_flash_ramp_up_only_drops_abruptly_after_the_hold);
  RUN_TEST(test_flash_without_hold_is_a_triangle);
  RUN_TEST(test_flash_lights_all_leds_equally);
  RUN_TEST(test_flash_repeats_every_interval);
  RUN_TEST(test_flash_survives_interval_change_and_long_gaps);
  RUN_TEST(test_flash_reset_restarts_the_cycle);
}
