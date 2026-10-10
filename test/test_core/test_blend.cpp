// BlendPattern: comet and flash in turn, the crossfade between them and the
// random direction.
//
// Defaults: 8 s of comet, 1.5 s crossfade, 6 s of flash, 1.5 s crossfade,
// random direction on. One cycle is 17 s.

#include <math.h>

#include <unity.h>

#include "BlendPattern.h"
#include "CometPattern.h"
#include "FlashPattern.h"
#include "PatternRegistry.h"
#include "tests.h"

using eye::BlendPattern;
using eye::EyeConfig;
using eye::LED_COUNT;
using Stage = eye::BlendPattern::Stage;

static const float TOLERANCE = 0.002f;
static const uint32_t FRAME_MS = 5;

// A flash that is easy to tell from the comet: steady at 40%.
static EyeConfig steadyFlashConfig() {
  EyeConfig c = eye::defaultConfig();
  c.flash.minBrightness = 40;
  c.flash.maxBrightness = 40;
  return c;
}

// Runs frame by frame for the given time.
static void run(BlendPattern& blend, const EyeConfig& c, uint32_t ms, float out[LED_COUNT]) {
  for (uint32_t t = 0; t < ms; t += FRAME_MS) {
    blend.render(c, FRAME_MS, out);
  }
}

static void test_blend_starts_as_the_comet() {
  const EyeConfig c = steadyFlashConfig();
  BlendPattern blend;
  eye::CometPattern comet;
  float out[LED_COUNT];
  float expected[LED_COUNT];

  blend.render(c, 0, out);
  comet.render(c, 0, expected);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, blend.mix());
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, expected[i], out[i]);
  }
}

static void test_blend_follows_the_four_stages_in_order() {
  const EyeConfig c = steadyFlashConfig();
  BlendPattern blend;
  float out[LED_COUNT];

  run(blend, c, 7990, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  run(blend, c, 20, out);  // 8010
  TEST_ASSERT_TRUE(blend.stage() == Stage::ToFlash);
  run(blend, c, 1500, out);  // 9510
  TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, blend.mix());
  run(blend, c, 6000, out);  // 15510
  TEST_ASSERT_TRUE(blend.stage() == Stage::ToComet);
  run(blend, c, 1500, out);  // 17010: one whole cycle
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, blend.mix());
}

static void test_blend_shows_only_the_flash_in_the_flash_stage() {
  const EyeConfig c = steadyFlashConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 12000, out);

  TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.4f, out[i]);
  }
}

static void test_blend_crossfade_is_half_and_half_at_its_midpoint() {
  const EyeConfig c = steadyFlashConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 8750, out);  // half-way through the first crossfade

  TEST_ASSERT_TRUE(blend.stage() == Stage::ToFlash);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, blend.mix());

  // LED by LED it is the average of the two, so the flash's 40% lifts the
  // dark LEDs to 20% and nothing goes dark.
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_TRUE(out[i] > 0.19f);
  }
}

static void test_blend_never_steps_between_frames() {
  // A slow comet and a steady flash, so any step would come from the blend.
  EyeConfig c = steadyFlashConfig();
  c.comet.stepIntervalMs = 400;
  BlendPattern blend;
  float previous[LED_COUNT];
  float out[LED_COUNT];
  blend.render(c, 0, previous);

  float largest = 0.0f;
  for (uint32_t t = 0; t < 40000; t += FRAME_MS) {
    blend.render(c, FRAME_MS, out);
    for (size_t i = 0; i < LED_COUNT; i++) {
      const float change = fabsf(out[i] - previous[i]);
      if (change > largest) {
        largest = change;
      }
      previous[i] = out[i];
    }
  }
  // The comet alone moves an LED by 5/400 of full scale per frame. The
  // crossfade adds at most 1.5 * 5/1500 on top.
  TEST_ASSERT_TRUE(largest < 0.02f);
}

static void test_blend_mix_only_ever_moves_gradually() {
  const EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  float previous = 0.0f;
  float largest = 0.0f;
  for (uint32_t t = 0; t < 40000; t += FRAME_MS) {
    blend.render(c, FRAME_MS, out);
    const float change = fabsf(blend.mix() - previous);
    if (change > largest) {
      largest = change;
    }
    previous = blend.mix();
  }
  TEST_ASSERT_TRUE(largest < 0.006f);
}

static void test_blend_zero_crossfade_is_a_cut() {
  EyeConfig c = steadyFlashConfig();
  c.blend.crossfadeMs = 0;
  BlendPattern blend;
  float out[LED_COUNT];

  run(blend, c, 7995, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  blend.render(c, 10, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.4f, out[i]);
  }
  run(blend, c, 6000, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
}

static void test_blend_comet_keeps_moving_while_hidden() {
  // After one whole cycle the comet is where a comet left alone would be,
  // not back at LED 1.
  EyeConfig c = steadyFlashConfig();
  c.blend.randomDirection = false;
  BlendPattern blend;
  eye::CometPattern comet;
  float out[LED_COUNT];
  float expected[LED_COUNT];

  const uint32_t ms = 17500;
  run(blend, c, ms, out);
  for (uint32_t t = 0; t < ms; t += FRAME_MS) {
    comet.render(c, FRAME_MS, expected);
  }
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected[i], out[i]);
  }
}

static void test_blend_flash_keeps_its_rhythm_while_hidden() {
  EyeConfig c = eye::defaultConfig();
  c.flash.intervalMs = 700;
  c.flash.durationMs = 100;
  BlendPattern blend;
  eye::FlashPattern flash;
  float out[LED_COUNT];
  float expected[LED_COUNT];

  const uint32_t ms = 10000;  // inside the flash stage
  run(blend, c, ms, out);
  for (uint32_t t = 0; t < ms; t += FRAME_MS) {
    flash.render(c, FRAME_MS, expected);
  }
  TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, expected[0], out[0]);
}

static void test_blend_changing_times_mid_run_does_not_jump() {
  EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 8600, out);  // part-way through the crossfade
  const float before = blend.mix();
  TEST_ASSERT_TRUE(before > 0.1f && before < 0.9f);

  // Every time changed at once, the crossfade made much longer.
  c.blend.cometMs = 1000;
  c.blend.flashMs = 30000;
  c.blend.crossfadeMs = 5000;
  blend.render(c, FRAME_MS, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::ToFlash);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, before, blend.mix());
}

static void test_blend_shortening_a_stage_below_where_it_is_moves_on_smoothly() {
  EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 6000, out);  // 6 s into the comet stage

  c.blend.cometMs = 2000;
  blend.render(c, FRAME_MS, out);
  // It starts the crossfade from the beginning; it does not skip ahead.
  TEST_ASSERT_TRUE(blend.stage() == Stage::ToFlash);
  TEST_ASSERT_TRUE(blend.mix() < 0.01f);
}

static void test_blend_direction_changes_only_while_the_comet_is_hidden() {
  const EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  blend.seed(12345);
  float out[LED_COUNT];
  blend.render(c, 0, out);

  eye::Direction previous = blend.direction();
  int changes = 0;
  for (uint32_t t = 0; t < 600000; t += FRAME_MS) {  // 10 minutes, about 35 cycles
    blend.render(c, FRAME_MS, out);
    if (blend.direction() != previous) {
      changes++;
      TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);
      TEST_ASSERT_EQUAL_FLOAT(1.0f, blend.mix());
      previous = blend.direction();
    }
  }
  // Even odds over 35 returns: it changes many times, but not every time.
  TEST_ASSERT_TRUE(changes >= 8);
  TEST_ASSERT_TRUE(changes <= 28);
}

static void test_blend_first_showing_uses_the_set_direction() {
  EyeConfig c = eye::defaultConfig();
  c.comet.direction = eye::Direction::CounterClockwise;
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 8000, out);
  TEST_ASSERT_TRUE(blend.direction() == eye::Direction::CounterClockwise);
}

static void test_blend_both_directions_occur() {
  const EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  blend.seed(2026);
  float out[LED_COUNT];
  int clockwise = 0;
  int counter = 0;
  for (int cycle = 0; cycle < 200; cycle++) {
    // One whole cycle in four large steps: straight to each stage.
    blend.render(c, 9500, out);  // into the flash stage
    if (blend.direction() == eye::Direction::Clockwise) {
      clockwise++;
    } else {
      counter++;
    }
    blend.render(c, 7500, out);
  }
  TEST_ASSERT_TRUE(clockwise > 60);
  TEST_ASSERT_TRUE(counter > 60);
}

static void test_blend_different_seeds_give_different_sequences() {
  const EyeConfig c = eye::defaultConfig();
  BlendPattern a;
  BlendPattern b;
  a.seed(1);
  b.seed(2);
  float out[LED_COUNT];
  int differences = 0;
  for (int cycle = 0; cycle < 64; cycle++) {
    a.render(c, 9500, out);
    b.render(c, 9500, out);
    if (a.direction() != b.direction()) {
      differences++;
    }
    a.render(c, 7500, out);
    b.render(c, 7500, out);
  }
  TEST_ASSERT_TRUE(differences > 0);
}

static void test_blend_random_off_follows_the_comet_setting() {
  EyeConfig c = eye::defaultConfig();
  c.blend.randomDirection = false;
  BlendPattern blend;
  blend.seed(99);
  float out[LED_COUNT];
  for (uint32_t t = 0; t < 120000; t += FRAME_MS) {
    blend.render(c, FRAME_MS, out);
    TEST_ASSERT_TRUE(blend.direction() == eye::Direction::Clockwise);
  }
  c.comet.direction = eye::Direction::CounterClockwise;
  blend.render(c, FRAME_MS, out);
  TEST_ASSERT_TRUE(blend.direction() == eye::Direction::CounterClockwise);
}

static void test_blend_reset_goes_back_to_the_comet() {
  const EyeConfig c = steadyFlashConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  run(blend, c, 12000, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Flash);

  blend.reset();
  blend.render(c, 0, out);
  TEST_ASSERT_TRUE(blend.stage() == Stage::Comet);
  TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, out[0]);  // comet head back on LED 1
}

static void test_blend_survives_a_very_long_gap() {
  const EyeConfig c = eye::defaultConfig();
  BlendPattern blend;
  float out[LED_COUNT];
  blend.render(c, 4000000000u, out);
  for (size_t i = 0; i < LED_COUNT; i++) {
    TEST_ASSERT_TRUE(out[i] >= 0.0f && out[i] <= 1.0f);
  }
  blend.render(c, FRAME_MS, out);
  TEST_ASSERT_TRUE(blend.mix() >= 0.0f && blend.mix() <= 1.0f);
}

static void test_blend_output_stays_in_range() {
  EyeConfig c = eye::defaultConfig();
  c.comet.tailLength = 7;
  c.comet.tailFalloff = 10;
  c.flash.rampUpMs = 300;
  c.flash.rampDownMs = 300;
  BlendPattern blend;
  float out[LED_COUNT];
  for (uint32_t t = 0; t < 40000; t += FRAME_MS) {
    blend.render(c, FRAME_MS, out);
    for (size_t i = 0; i < LED_COUNT; i++) {
      TEST_ASSERT_TRUE(out[i] >= 0.0f && out[i] <= 1.0f);
    }
  }
}

static void test_blend_is_registered_as_the_third_pattern() {
  eye::PatternRegistry registry;
  eye::Pattern* blend = registry.find("blend");
  TEST_ASSERT_NOT_NULL(blend);
  TEST_ASSERT_TRUE(registry.at(2) == blend);
  TEST_ASSERT_EQUAL_STRING("Comet and flash", blend->label());
}

void runBlendTests() {
  RUN_TEST(test_blend_starts_as_the_comet);
  RUN_TEST(test_blend_follows_the_four_stages_in_order);
  RUN_TEST(test_blend_shows_only_the_flash_in_the_flash_stage);
  RUN_TEST(test_blend_crossfade_is_half_and_half_at_its_midpoint);
  RUN_TEST(test_blend_never_steps_between_frames);
  RUN_TEST(test_blend_mix_only_ever_moves_gradually);
  RUN_TEST(test_blend_zero_crossfade_is_a_cut);
  RUN_TEST(test_blend_comet_keeps_moving_while_hidden);
  RUN_TEST(test_blend_flash_keeps_its_rhythm_while_hidden);
  RUN_TEST(test_blend_changing_times_mid_run_does_not_jump);
  RUN_TEST(test_blend_shortening_a_stage_below_where_it_is_moves_on_smoothly);
  RUN_TEST(test_blend_direction_changes_only_while_the_comet_is_hidden);
  RUN_TEST(test_blend_first_showing_uses_the_set_direction);
  RUN_TEST(test_blend_both_directions_occur);
  RUN_TEST(test_blend_different_seeds_give_different_sequences);
  RUN_TEST(test_blend_random_off_follows_the_comet_setting);
  RUN_TEST(test_blend_reset_goes_back_to_the_comet);
  RUN_TEST(test_blend_survives_a_very_long_gap);
  RUN_TEST(test_blend_output_stays_in_range);
  RUN_TEST(test_blend_is_registered_as_the_third_pattern);
}
