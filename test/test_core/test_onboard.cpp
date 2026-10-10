// OnboardLogic: the four modes of the board's own LED, and how the Animator
// triggers the blink on a pattern change.

#include <string.h>

#include <unity.h>

#include "Animator.h"
#include "Gamma.h"
#include "OnboardLogic.h"
#include "tests.h"

using eye::OnboardLogic;
using eye::OnboardMode;
using eye::OnboardSettings;

static const float TOLERANCE = 0.002f;

static OnboardSettings settings(OnboardMode mode) {
  OnboardSettings s = eye::defaultConfig().onboard;  // brightness 50, 2 blinks of 120 ms
  s.mode = mode;
  return s;
}

static void test_onboard_off_and_on() {
  OnboardLogic logic;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, logic.update(settings(OnboardMode::Off), 5));
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, logic.update(settings(OnboardMode::On), 5));
}

static void test_onboard_heartbeat_is_two_beats_then_rest() {
  OnboardLogic logic;
  const OnboardSettings s = settings(OnboardMode::Heartbeat);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, logic.update(s, 0));    // 0 ms: first beat
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, logic.update(s, 150));  // 150 ms: gap
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, logic.update(s, 150));  // 300 ms: second beat
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, logic.update(s, 300));  // 600 ms: rest
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.5f, logic.update(s, 650));  // 1250 ms: next cycle
}

static void test_onboard_blink_is_idle_until_the_pattern_changes() {
  OnboardLogic logic;
  const OnboardSettings s = settings(OnboardMode::BlinkOnPatternChange);
  for (int i = 0; i < 100; i++) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, logic.update(s, 5));
  }
}

static void test_onboard_blinks_the_set_number_of_times() {
  OnboardLogic logic;
  OnboardSettings s = settings(OnboardMode::BlinkOnPatternChange);
  s.blinkCount = 3;
  s.blinkMs = 100;
  logic.notifyPatternChange();

  // Sample every millisecond and count off -> on edges.
  int blinks = 0;
  bool wasOn = false;
  int onMs = 0;
  for (int t = 0; t < 2000; t++) {
    const bool on = logic.update(s, 1) > 0.0f;
    if (on && !wasOn) {
      blinks++;
    }
    if (on) {
      onMs++;
    }
    wasOn = on;
  }
  TEST_ASSERT_EQUAL_INT(3, blinks);
  TEST_ASSERT_INT_WITHIN(3, 300, onMs);
  TEST_ASSERT_FALSE(wasOn);
}

static void test_onboard_blink_ends_after_a_long_gap() {
  OnboardLogic logic;
  const OnboardSettings s = settings(OnboardMode::BlinkOnPatternChange);
  logic.notifyPatternChange();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, logic.update(s, 4000000000u));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, logic.update(s, 5));
}

static void test_onboard_burst_from_another_mode_does_not_play_later() {
  OnboardLogic logic;
  logic.notifyPatternChange();
  logic.update(settings(OnboardMode::On), 5);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, logic.update(settings(OnboardMode::BlinkOnPatternChange), 5));
}

static void test_animator_blinks_onboard_led_when_the_pattern_changes() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[eye::LED_COUNT];

  animator.render(5, out);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, animator.onboardLevel());

  eye::EyeConfig c = animator.config();
  c.brightness = 80;  // not a pattern change
  animator.setConfig(c);
  animator.render(5, out);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, animator.onboardLevel());

  strcpy(c.pattern, "flash");
  animator.setConfig(c);
  animator.render(5, out);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, eye::gamma::correct(0.5f), animator.onboardLevel());
}

static void test_animator_onboard_led_works_while_the_ring_is_stopped() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[eye::LED_COUNT];

  eye::EyeConfig c = animator.config();
  c.running = false;
  c.onboard.mode = OnboardMode::On;
  c.onboard.brightness = 100;
  animator.setConfig(c);
  animator.render(5, out);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out[0]);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, animator.onboardLevel());
}

static void test_animator_restart_does_not_count_as_a_pattern_change() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[eye::LED_COUNT];

  eye::EyeConfig c = animator.config();
  strcpy(c.pattern, "flash");  // as if loaded from saved settings at start-up
  animator.setConfig(c);
  animator.restart();
  animator.render(5, out);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, animator.onboardLevel());
}

void runOnboardTests() {
  RUN_TEST(test_onboard_off_and_on);
  RUN_TEST(test_onboard_heartbeat_is_two_beats_then_rest);
  RUN_TEST(test_onboard_blink_is_idle_until_the_pattern_changes);
  RUN_TEST(test_onboard_blinks_the_set_number_of_times);
  RUN_TEST(test_onboard_blink_ends_after_a_long_gap);
  RUN_TEST(test_onboard_burst_from_another_mode_does_not_play_later);
  RUN_TEST(test_animator_blinks_onboard_led_when_the_pattern_changes);
  RUN_TEST(test_animator_onboard_led_works_while_the_ring_is_stopped);
  RUN_TEST(test_animator_restart_does_not_count_as_a_pattern_change);
}
