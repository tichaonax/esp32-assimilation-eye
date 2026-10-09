// Assimilation Eye — firmware entry point.
//
// Phase 3: brings up the LED ring, runs the power-on self-test and starts the
// animation with the compiled-in default settings. While BRINGUP_PROBE is
// defined, the BOOT button (or the letter n on the serial console) steps
// through a set of demonstration settings, since there is no interface yet.

#include <Arduino.h>

#include "AnimationTask.h"
#include "HardwareConfig.h"
#include "LedDriver.h"
#include "SelfTest.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

static LedDriver leds;
static AnimationTask animation(leds);

#ifdef BRINGUP_PROBE
namespace probe {

struct Step {
  const char* description;
  void (*edit)(eye::EyeConfig&);
};

void usePattern(eye::EyeConfig& c, const char* name) {
  strncpy(c.pattern, name, eye::PATTERN_NAME_SIZE - 1);
  c.pattern[eye::PATTERN_NAME_SIZE - 1] = '\0';
}

// Each step starts from the default settings and changes what it names.
const Step STEPS[] = {
    {"comet, clockwise, default settings", [](eye::EyeConfig&) {}},
    {"comet, counter-clockwise", [](eye::EyeConfig& c) { c.comet.direction = eye::Direction::CounterClockwise; }},
    {"comet, slow (400 ms per LED), tail of 2",
     [](eye::EyeConfig& c) {
       c.comet.stepIntervalMs = 400;
       c.comet.tailLength = 2;
     }},
    {"flash, abrupt, default settings", [](eye::EyeConfig& c) { usePattern(c, "flash"); }},
    {"flash, ramp up 300 ms, hold 100 ms, ramp down 500 ms",
     [](eye::EyeConfig& c) {
       usePattern(c, "flash");
       c.flash.rampUpMs = 300;
       c.flash.durationMs = 100;
       c.flash.rampDownMs = 500;
     }},
    {"flash, slow pulse between 10% and 100% over 2 seconds",
     [](eye::EyeConfig& c) {
       usePattern(c, "flash");
       c.flash.intervalMs = 2000;
       c.flash.minBrightness = 10;
       c.flash.rampUpMs = 1000;
       c.flash.durationMs = 0;
       c.flash.rampDownMs = 1000;
     }},
};
constexpr size_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);

constexpr uint32_t DEBOUNCE_MS = 40;
constexpr uint32_t ONBOARD_BLINK_MS = 150;
constexpr uint32_t REPORT_MS = 5000;

size_t current = 0;
bool buttonStable = false;
bool buttonLast = false;
uint32_t buttonChangedMs = 0;
uint32_t onboardOffAtMs = 0;
uint32_t lastReportMs = 0;
uint32_t lastReportFrames = 0;

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

void show(size_t index) {
  current = index % STEP_COUNT;
  eye::EyeConfig config = eye::defaultConfig();
  STEPS[current].edit(config);
  const bool ok = animation.apply(config);
  Serial.printf("[demo] %u of %u: %s%s\n", static_cast<unsigned>(current + 1), static_cast<unsigned>(STEP_COUNT),
                STEPS[current].description, ok ? "" : " — REJECTED");
  setOnboard(true);
  onboardOffAtMs = millis() + ONBOARD_BLINK_MS;
}

void begin() {
  if (hw::ONBOARD_LED_PRESENT) {
    pinMode(hw::ONBOARD_LED_PIN, OUTPUT);
    setOnboard(false);
  }
  pinMode(hw::BOOT_BUTTON_PIN, INPUT_PULLUP);
  Serial.println(F("[demo] Press the BOOT button, or send n, for the next demonstration."));
  Serial.println(F("[demo] The onboard LED blinks once on each change."));
  show(0);
  lastReportMs = millis();
  lastReportFrames = animation.frameCount();
}

void loop() {
  const uint32_t now = millis();

  const bool raw = buttonPressed();
  if (raw != buttonLast) {
    buttonLast = raw;
    buttonChangedMs = now;
  } else if (raw != buttonStable && now - buttonChangedMs >= DEBOUNCE_MS) {
    buttonStable = raw;
    if (buttonStable) {
      Serial.println(F("[demo] BOOT button pressed"));
      show(current + 1);
    }
  }

  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 'n' || c == 'N') {
      show(current + 1);
    }
  }

  if (onboardOffAtMs != 0 && static_cast<int32_t>(now - onboardOffAtMs) >= 0) {
    setOnboard(false);
    onboardOffAtMs = 0;
  }

  if (now - lastReportMs >= REPORT_MS) {
    const uint32_t frames = animation.frameCount();
    Serial.printf("[demo] animation: %.1f frames per second\n",
                  (frames - lastReportFrames) * 1000.0f / static_cast<float>(now - lastReportMs));
    lastReportMs = now;
    lastReportFrames = frames;
  }
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

  const bool animationOk = animation.begin();
  Serial.printf("Animation: %u patterns, %u ms tick — %s\n", static_cast<unsigned>(animation.patterns().count()),
                static_cast<unsigned>(AnimationTask::TICK_MS), animationOk ? "running" : "FAILED to start");

#ifdef BRINGUP_PROBE
  probe::begin();
#endif
}

void loop() {
#ifdef BRINGUP_PROBE
  probe::loop();
  delay(5);
#else
  delay(1000);
#endif
}
