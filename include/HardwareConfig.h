// Hardware configuration — the only file that knows which pin does what.
//
// Kept free of Arduino includes so the unit tests can check it on the PC.
// Wiring for both supported boards is in docs/hardware-guide.md.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hw {

// ---- LED ring -------------------------------------------------------------

constexpr size_t RING_LED_COUNT = 8;

// GPIO for LED 1..8. LED 1 is at the top of the lens and the numbering runs
// clockwise as the audience sees it. Reorder this array if the ring is wired
// differently; nothing else needs to change.
//
//   LED:            1   2   3   4   5   6    7    8
//   DEVKIT V1:    D23 D22 D21 D19 D18 TX2  RX2  D4
//   DevKitC 38:  IO23 IO22 IO21 IO19 IO18 IO17 IO16 IO4
//
// Confirmed on the DEVKIT V1 on 2026-10-10: all eight light, in this order.
constexpr uint8_t RING_LED_PINS[RING_LED_COUNT] = {23, 22, 21, 19, 18, 17, 16, 4};

// ---- PWM ------------------------------------------------------------------

// 5 kHz is far above visible flicker and comfortable for most cameras.
constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 12;
constexpr uint32_t PWM_MAX_DUTY = (1u << PWM_RESOLUTION_BITS) - 1;

// ---- Onboard LED ----------------------------------------------------------

// DOIT DEVKIT V1: blue LED on GPIO 2, lit when the pin is high. Confirmed
// blinking on the board in hand on 2026-10-10.
// Genuine 38-pin DevKitC: no controllable LED — set ONBOARD_LED_PRESENT false.
constexpr bool ONBOARD_LED_PRESENT = true;
constexpr uint8_t ONBOARD_LED_PIN = 2;
constexpr bool ONBOARD_LED_ACTIVE_HIGH = true;

// ---- BOOT button ----------------------------------------------------------

// GPIO 0, pulled up on the board, reads low while pressed. Only read after
// start-up; holding it during reset puts the chip in download mode.
constexpr uint8_t BOOT_BUTTON_PIN = 0;
constexpr bool BOOT_BUTTON_ACTIVE_LOW = true;

// ---- Serial console -------------------------------------------------------

constexpr uint32_t SERIAL_BAUD = 115200;

}  // namespace hw
