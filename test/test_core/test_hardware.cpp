// HardwareConfig: the pin map must be safe to drive LEDs from.

#include <unity.h>

#include "EyeConfig.h"
#include "HardwareConfig.h"
#include "tests.h"

static bool contains(const uint8_t* list, size_t count, uint8_t value) {
  for (size_t i = 0; i < count; i++) {
    if (list[i] == value) {
      return true;
    }
  }
  return false;
}

static void test_ring_has_eight_leds() {
  TEST_ASSERT_EQUAL_UINT(8, hw::RING_LED_COUNT);
  TEST_ASSERT_EQUAL_UINT(8, sizeof(hw::RING_LED_PINS) / sizeof(hw::RING_LED_PINS[0]));
  TEST_ASSERT_EQUAL_UINT(hw::RING_LED_COUNT, eye::LED_COUNT);
}

static void test_ring_pins_are_unique() {
  for (size_t i = 0; i < hw::RING_LED_COUNT; i++) {
    for (size_t j = i + 1; j < hw::RING_LED_COUNT; j++) {
      TEST_ASSERT_NOT_EQUAL(hw::RING_LED_PINS[i], hw::RING_LED_PINS[j]);
    }
  }
}

static void test_ring_avoids_reserved_pins() {
  // Boot strapping pins, USB serial, internal flash and input-only pins.
  static const uint8_t reserved[] = {0, 2, 5, 12, 15, 1, 3, 6, 7, 8, 9, 10, 11, 34, 35, 36, 37, 38, 39};
  for (size_t i = 0; i < hw::RING_LED_COUNT; i++) {
    TEST_ASSERT_FALSE_MESSAGE(contains(reserved, sizeof(reserved), hw::RING_LED_PINS[i]),
                              "ring LED assigned to a reserved pin");
  }
}

static void test_ring_avoids_onboard_led_and_button() {
  TEST_ASSERT_FALSE(contains(hw::RING_LED_PINS, hw::RING_LED_COUNT, hw::ONBOARD_LED_PIN));
  TEST_ASSERT_FALSE(contains(hw::RING_LED_PINS, hw::RING_LED_COUNT, hw::BOOT_BUTTON_PIN));
}

static void test_pwm_duty_range_matches_resolution() {
  TEST_ASSERT_EQUAL_UINT32(4095, hw::PWM_MAX_DUTY);
  TEST_ASSERT_TRUE(hw::PWM_RESOLUTION_BITS >= 8);
}

void runHardwareTests() {
  RUN_TEST(test_ring_has_eight_leds);
  RUN_TEST(test_ring_pins_are_unique);
  RUN_TEST(test_ring_avoids_reserved_pins);
  RUN_TEST(test_ring_avoids_onboard_led_and_button);
  RUN_TEST(test_pwm_duty_range_matches_resolution);
}
