// PatternRegistry and Animator: pattern lookup, switching, the running flag
// and overall brightness.

#include <string.h>

#include <unity.h>

#include "Animator.h"
#include "Gamma.h"
#include "PatternRegistry.h"
#include "tests.h"

using eye::EyeConfig;
using eye::LED_COUNT;

static const float TOLERANCE = 0.002f;

namespace {
// A pattern that is not built in, to prove one can be added from outside.
class SolidPattern : public eye::Pattern {
 public:
  const char* name() const override { return "solid"; }
  const char* label() const override { return "Solid"; }
  void reset() override { resets++; }
  void render(const EyeConfig&, uint32_t, float out[LED_COUNT]) override {
    for (size_t i = 0; i < LED_COUNT; i++) {
      out[i] = 1.0f;
    }
  }
  int resets = 0;
};
}  // namespace

static void test_registry_has_the_built_in_patterns() {
  eye::PatternRegistry registry;
  TEST_ASSERT_EQUAL_UINT(3, registry.count());
  TEST_ASSERT_NOT_NULL(registry.find("comet"));
  TEST_ASSERT_NOT_NULL(registry.find("flash"));
  TEST_ASSERT_NOT_NULL(registry.find("blend"));
  TEST_ASSERT_EQUAL_STRING("comet", registry.at(0)->name());
  TEST_ASSERT_NULL(registry.at(3));
}

static void test_registry_rejects_unknown_and_duplicate_names() {
  eye::PatternRegistry registry;
  TEST_ASSERT_NULL(registry.find("sparkle"));
  TEST_ASSERT_NULL(registry.find(""));
  TEST_ASSERT_NULL(registry.find(nullptr));

  eye::CometPattern another;
  TEST_ASSERT_FALSE(registry.add(&another));
  TEST_ASSERT_FALSE(registry.add(nullptr));
  TEST_ASSERT_EQUAL_UINT(3, registry.count());
}

static void test_registry_accepts_a_new_pattern() {
  eye::PatternRegistry registry;
  SolidPattern solid;
  TEST_ASSERT_TRUE(registry.add(&solid));
  TEST_ASSERT_EQUAL_UINT(4, registry.count());
  TEST_ASSERT_TRUE(registry.find("solid") == &solid);

  eye::Animator animator(registry);
  EyeConfig c = eye::defaultConfig();
  strcpy(c.pattern, "solid");
  c.brightness = 100;
  TEST_ASSERT_TRUE(animator.setConfig(c));
  TEST_ASSERT_EQUAL_INT(1, solid.resets);

  float out[LED_COUNT];
  animator.render(5, out);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, out[i]);
  }
}

static void test_animator_starts_with_the_default_settings() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  TEST_ASSERT_EQUAL_STRING("comet", animator.config().pattern);
  TEST_ASSERT_TRUE(animator.config().running);
}

static void test_animator_applies_overall_brightness_and_gamma() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[LED_COUNT];
  animator.render(0, out);  // comet at the start, overall brightness 60%

  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, eye::gamma::correct(0.6f), out[0]);          // head
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, eye::gamma::correct(0.6f * 0.5f), out[7]);   // first tail LED
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, out[2]);
}

static void test_animator_stopped_is_dark_and_does_not_advance() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float before[LED_COUNT];
  float out[LED_COUNT];
  animator.render(45, before);

  EyeConfig c = animator.config();
  c.running = false;
  TEST_ASSERT_TRUE(animator.setConfig(c));
  animator.render(5000, out);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[i]);
  }

  c.running = true;
  TEST_ASSERT_TRUE(animator.setConfig(c));
  animator.render(0, out);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, before[i], out[i]);
  }
}

static void test_animator_brightness_zero_is_dark() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  EyeConfig c = animator.config();
  c.brightness = 0;
  animator.setConfig(c);
  float out[LED_COUNT];
  animator.render(10, out);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[i]);
  }
}

static void test_animator_rejects_an_unknown_pattern_and_keeps_running() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  EyeConfig c = animator.config();
  strcpy(c.pattern, "sparkle");
  c.brightness = 5;
  TEST_ASSERT_FALSE(animator.setConfig(c));
  TEST_ASSERT_EQUAL_STRING("comet", animator.config().pattern);
  TEST_ASSERT_EQUAL_INT32(60, animator.config().brightness);
}

static void test_animator_sanitizes_what_it_is_given() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  EyeConfig c = animator.config();
  c.brightness = 900;
  c.comet.tailLength = 50;
  TEST_ASSERT_TRUE(animator.setConfig(c));
  TEST_ASSERT_EQUAL_INT32(100, animator.config().brightness);
  TEST_ASSERT_EQUAL_INT32(7, animator.config().comet.tailLength);
}

static void test_animator_switching_pattern_restarts_it() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[LED_COUNT];

  EyeConfig c = animator.config();
  c.brightness = 100;
  strcpy(c.pattern, "flash");
  TEST_ASSERT_TRUE(animator.setConfig(c));
  animator.render(500, out);  // past the 200 ms hold: dark
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, out[0]);

  strcpy(c.pattern, "comet");
  animator.setConfig(c);
  animator.render(180, out);
  strcpy(c.pattern, "flash");
  animator.setConfig(c);
  animator.render(0, out);  // restarted, so back at the start of a flash
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, out[i]);
  }
}

static void test_animator_changing_a_setting_does_not_restart_the_pattern() {
  eye::PatternRegistry registry;
  eye::Animator animator(registry);
  float out[LED_COUNT];
  animator.render(180, out);  // head on LED 3

  EyeConfig c = animator.config();
  c.brightness = 100;
  animator.setConfig(c);
  animator.render(0, out);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, out[2]);
}

void runAnimatorTests() {
  RUN_TEST(test_registry_has_the_built_in_patterns);
  RUN_TEST(test_registry_rejects_unknown_and_duplicate_names);
  RUN_TEST(test_registry_accepts_a_new_pattern);
  RUN_TEST(test_animator_starts_with_the_default_settings);
  RUN_TEST(test_animator_applies_overall_brightness_and_gamma);
  RUN_TEST(test_animator_stopped_is_dark_and_does_not_advance);
  RUN_TEST(test_animator_brightness_zero_is_dark);
  RUN_TEST(test_animator_rejects_an_unknown_pattern_and_keeps_running);
  RUN_TEST(test_animator_sanitizes_what_it_is_given);
  RUN_TEST(test_animator_switching_pattern_restarts_it);
  RUN_TEST(test_animator_changing_a_setting_does_not_restart_the_pattern);
}
