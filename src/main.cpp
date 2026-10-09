// Assimilation Eye — firmware entry point.
//
// Phase 2: brings up the LED ring, runs the power-on self-test and, while
// BRINGUP_PROBE is defined, repeats a slow LED chase and reports the onboard
// LED and BOOT button on the serial console.

#include <Arduino.h>

#include "HardwareConfig.h"
#include "LedDriver.h"
#include "SelfTest.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

static LedDriver leds;

#ifdef BRINGUP_PROBE
namespace probe {

constexpr uint32_t STEP_MS = 600;

size_t current = 0;
uint32_t lastStepMs = 0;
bool onboardOn = false;
bool lastButtonPressed = false;

void setOnboard(bool on) {
  if (!hw::ONBOARD_LED_PRESENT) {
    return;
  }
  digitalWrite(hw::ONBOARD_LED_PIN, on == hw::ONBOARD_LED_ACTIVE_HIGH ? HIGH : LOW);
}

bool buttonPressed() {
  const bool low = digitalRead(hw::BOOT_BUTTON_PIN) == LOW;
  return hw::BOOT_BUTTON_ACTIVE_LOW ? low : !low;
}

void begin() {
  if (hw::ONBOARD_LED_PRESENT) {
    pinMode(hw::ONBOARD_LED_PIN, OUTPUT);
    setOnboard(false);
  }
  pinMode(hw::BOOT_BUTTON_PIN, INPUT_PULLUP);
  Serial.println(F("[probe] Bring-up probe running."));
  Serial.println(F("[probe] The ring steps LED 1 -> LED 8, one at a time, and repeats."));
  Serial.printf("[probe] Onboard LED on GPIO %u should blink with each step.\n", hw::ONBOARD_LED_PIN);
  Serial.println(F("[probe] Press the BOOT button to see it reported here."));
}

void loop() {
  const uint32_t now = millis();

  const bool pressed = buttonPressed();
  if (pressed != lastButtonPressed) {
    lastButtonPressed = pressed;
    Serial.printf("[probe] BOOT button %s\n", pressed ? "PRESSED" : "released");
  }

  if (now - lastStepMs < STEP_MS) {
    return;
  }
  lastStepMs = now;

  leds.off();
  leds.set(current, 0.6f);
  onboardOn = !onboardOn;
  setOnboard(onboardOn);
  Serial.printf("[probe] LED %u lit (GPIO %u), onboard LED %s\n", static_cast<unsigned>(current + 1),
                hw::RING_LED_PINS[current], onboardOn ? "on" : "off");
  current = (current + 1) % hw::RING_LED_COUNT;
}

}  // namespace probe
#endif  // BRINGUP_PROBE

void setup() {
  Serial.begin(hw::SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=== Assimilation Eye ==="));
  Serial.printf("Firmware %s, built %s %s\n", FIRMWARE_VERSION, __DATE__, __TIME__);
  // Core 3.x reports the revision as major * 100 + minor.
  Serial.printf("Chip %s rev %u.%u, %u core(s), flash %u MB\n", ESP.getChipModel(), ESP.getChipRevision() / 100,
                ESP.getChipRevision() % 100, ESP.getChipCores(),
                static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("Sketch %u bytes, free slot space %u bytes\n", static_cast<unsigned>(ESP.getSketchSize()),
                static_cast<unsigned>(ESP.getFreeSketchSpace()));

  const bool ringOk = leds.begin();
  Serial.printf("LED ring: %u outputs at %u Hz, %u-bit — %s\n", static_cast<unsigned>(hw::RING_LED_COUNT),
                static_cast<unsigned>(hw::PWM_FREQUENCY_HZ), hw::PWM_RESOLUTION_BITS,
                ringOk ? "ready" : "FAILED to attach one or more pins");

  Serial.println(F("Self-test: LED 1 -> LED 8, then fade."));
  selftest::run(leds);
  Serial.println(F("Self-test done."));

#ifdef BRINGUP_PROBE
  probe::begin();
#endif
}

void loop() {
#ifdef BRINGUP_PROBE
  probe::loop();
#else
  delay(1000);
#endif
}
