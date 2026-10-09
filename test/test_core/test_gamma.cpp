// Gamma: perceived brightness to PWM duty fraction.

#include <math.h>

#include <unity.h>

#include "Gamma.h"
#include "tests.h"

using eye::gamma::correct;

static void test_gamma_keeps_the_end_points() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, correct(0.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, correct(1.0f));
}

static void test_gamma_dims_the_middle() {
  // Half perceived brightness needs well under half the light.
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.2176f, correct(0.5f));
}

static void test_gamma_always_increases() {
  float previous = correct(0.0f);
  for (int i = 1; i <= 100; i++) {
    const float value = correct(static_cast<float>(i) / 100.0f);
    TEST_ASSERT_TRUE_MESSAGE(value > previous, "gamma curve is not strictly increasing");
    previous = value;
  }
}

static void test_gamma_clamps_bad_input() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, correct(-0.5f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, correct(3.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, correct(NAN));
}

void runGammaTests() {
  RUN_TEST(test_gamma_keeps_the_end_points);
  RUN_TEST(test_gamma_dims_the_middle);
  RUN_TEST(test_gamma_always_increases);
  RUN_TEST(test_gamma_clamps_bad_input);
}
