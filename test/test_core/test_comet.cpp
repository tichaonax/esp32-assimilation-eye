// CometPattern: head, tail, direction, speed and wraparound.
//
// LED n is index n-1. Defaults: clockwise, 90 ms per LED, head 100%,
// tail of 4, each tail LED 50% dimmer than the one before.

#include <unity.h>

#include "CometPattern.h"
#include "tests.h"

using eye::EyeConfig;
using eye::LED_COUNT;

static const float TOLERANCE = 0.002f;

static void assertFrame(const float expected[LED_COUNT], const float actual[LED_COUNT]) {
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, expected[i], actual[i]);
  }
}

static void test_comet_starts_with_head_on_led_1_and_tail_behind() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 0, out);

  // Clockwise travel, so the tail lies on LEDs 8, 7, 6, 5.
  const float expected[LED_COUNT] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0625f, 0.125f, 0.25f, 0.5f};
  assertFrame(expected, out);
}

static void test_comet_moves_one_led_per_step_interval() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 90, out);

  const float expected[LED_COUNT] = {0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0625f, 0.125f, 0.25f};
  assertFrame(expected, out);
}

static void test_comet_glides_between_leds() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 45, out);  // half a step: head midway between LED 1 and LED 2

  const float expected[LED_COUNT] = {0.75f, 0.5f, 0.0f, 0.0f, 0.03125f, 0.09375f, 0.1875f, 0.375f};
  assertFrame(expected, out);
}

static void test_comet_wraps_smoothly_from_led_8_to_led_1() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 675, out);  // 7.5 steps: head midway between LED 8 and LED 1

  const float expected[LED_COUNT] = {0.5f, 0.0f, 0.0f, 0.03125f, 0.09375f, 0.1875f, 0.375f, 0.75f};
  assertFrame(expected, out);

  comet.render(c, 45, out);  // completes the lap
  const float start[LED_COUNT] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0625f, 0.125f, 0.25f, 0.5f};
  assertFrame(start, out);
}

static void test_comet_counter_clockwise_is_the_mirror_image() {
  EyeConfig clockwise = eye::defaultConfig();
  EyeConfig counter = eye::defaultConfig();
  counter.comet.direction = eye::Direction::CounterClockwise;

  eye::CometPattern a;
  eye::CometPattern b;
  float cw[LED_COUNT];
  float ccw[LED_COUNT];
  // Uneven tick lengths, to cover fractional positions.
  const uint32_t ticks[] = {0, 5, 40, 45, 130, 7, 500, 93};
  for (uint32_t tick : ticks) {
    a.render(clockwise, tick, cw);
    b.render(counter, tick, ccw);
    for (size_t i = 0; i < LED_COUNT; i++) {
      TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, cw[(LED_COUNT - i) % LED_COUNT], ccw[i]);
    }
  }
}

static void test_comet_counter_clockwise_moves_from_led_1_to_led_8() {
  EyeConfig c = eye::defaultConfig();
  c.comet.direction = eye::Direction::CounterClockwise;
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 90, out);

  // Head on LED 8, tail on LEDs 1, 2, 3, 4.
  const float expected[LED_COUNT] = {0.5f, 0.25f, 0.125f, 0.0625f, 0.0f, 0.0f, 0.0f, 1.0f};
  assertFrame(expected, out);
}

static void test_comet_tail_length_zero_lights_only_the_head() {
  EyeConfig c = eye::defaultConfig();
  c.comet.tailLength = 0;
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 180, out);

  const float expected[LED_COUNT] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  assertFrame(expected, out);
}

static void test_comet_longest_tail_lights_the_whole_ring() {
  EyeConfig c = eye::defaultConfig();
  c.comet.tailLength = 7;
  c.comet.tailFalloff = 10;
  eye::CometPattern comet;
  float out[LED_COUNT];
  const uint32_t ticks[] = {0, 30, 45, 61, 700};
  for (uint32_t tick : ticks) {
    comet.render(c, tick, out);
    for (size_t i = 0; i < LED_COUNT; i++) {
      TEST_ASSERT_TRUE_MESSAGE(out[i] > 0.4f, "an LED went dark with a full-ring tail");
      TEST_ASSERT_TRUE_MESSAGE(out[i] <= 1.0f, "an LED went above full brightness");
    }
  }
}

static void test_comet_head_brightness_and_falloff_are_applied() {
  EyeConfig c = eye::defaultConfig();
  c.comet.headBrightness = 80;
  c.comet.tailLength = 2;
  c.comet.tailFalloff = 25;
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 0, out);

  const float expected[LED_COUNT] = {0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.45f, 0.6f};
  assertFrame(expected, out);
}

static void test_comet_speed_change_does_not_move_the_head() {
  EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float before[LED_COUNT];
  float after[LED_COUNT];
  comet.render(c, 315, before);  // 3.5 steps in

  c.comet.stepIntervalMs = 1500;
  comet.render(c, 0, after);
  assertFrame(before, after);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.5f, comet.position());

  comet.render(c, 750, after);  // half a step at the new speed
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, comet.position());
}

static void test_comet_position_stays_on_the_ring_over_a_long_run() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  for (int i = 0; i < 20000; i++) {
    comet.render(c, 5, out);
    TEST_ASSERT_TRUE(comet.position() >= 0.0f && comet.position() < static_cast<float>(LED_COUNT));
  }
  comet.render(c, 4000000000u, out);  // a very long gap must not break it
  TEST_ASSERT_TRUE(comet.position() >= 0.0f && comet.position() < static_cast<float>(LED_COUNT));
}

static void test_comet_reset_returns_to_led_1() {
  const EyeConfig c = eye::defaultConfig();
  eye::CometPattern comet;
  float out[LED_COUNT];
  comet.render(c, 400, out);
  comet.reset();
  comet.render(c, 0, out);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, out[0]);
}

void runCometTests() {
  RUN_TEST(test_comet_starts_with_head_on_led_1_and_tail_behind);
  RUN_TEST(test_comet_moves_one_led_per_step_interval);
  RUN_TEST(test_comet_glides_between_leds);
  RUN_TEST(test_comet_wraps_smoothly_from_led_8_to_led_1);
  RUN_TEST(test_comet_counter_clockwise_is_the_mirror_image);
  RUN_TEST(test_comet_counter_clockwise_moves_from_led_1_to_led_8);
  RUN_TEST(test_comet_tail_length_zero_lights_only_the_head);
  RUN_TEST(test_comet_longest_tail_lights_the_whole_ring);
  RUN_TEST(test_comet_head_brightness_and_falloff_are_applied);
  RUN_TEST(test_comet_speed_change_does_not_move_the_head);
  RUN_TEST(test_comet_position_stays_on_the_ring_over_a_long_run);
  RUN_TEST(test_comet_reset_returns_to_led_1);
}
