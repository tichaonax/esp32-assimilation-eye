// Assimilation Eye — firmware entry point.
//
// Brings up the LED ring and onboard LED, runs the power-on self-test, starts
// the animation with the saved settings, then Wi-Fi, sign-in and the web
// server. While BRINGUP_PROBE is defined, a short press of the BOOT button (or
// the letter n on the serial console) switches pattern and the console gets a
// periodic health line.

#include <Arduino.h>

#include "AnimationTask.h"
#include "Auth.h"
#include "BuildInfo.h"
#include "ConfigStore.h"
#include "FactoryReset.h"
#include "HardwareConfig.h"
#include "LedDriver.h"
#include "OnboardLed.h"
#include "OtaUpdate.h"
#include "SelfTest.h"
#include "SystemControl.h"
#include "WebApi.h"
#include "WifiManager.h"

static LedDriver leds;
static OnboardLed onboardLed;
static AnimationTask animation(leds, onboardLed);
static ConfigStore store(animation);
static Auth auth;
static WifiManager wifi;
static OtaUpdate ota(animation);
static FactoryReset factoryReset(animation, store, auth, wifi);
static WebApi api(animation, store, onboardLed, auth, wifi, ota, factoryReset);

#ifdef BRINGUP_PROBE
namespace probe {

constexpr uint32_t REPORT_MS = 30000;

uint32_t lastReportMs = 0;
uint32_t lastReportFrames = 0;

// Moves to the next registered pattern, through the same path the API uses.
void nextPattern() {
  const eye::PatternRegistry& registry = animation.patterns();
  eye::EyeConfig config = animation.config();
  size_t index = 0;
  for (size_t i = 0; i < registry.count(); i++) {
    if (strcmp(registry.at(i)->name(), config.pattern) == 0) {
      index = i;
      break;
    }
  }
  const eye::Pattern* next = registry.at((index + 1) % registry.count());
  strncpy(config.pattern, next->name(), eye::PATTERN_NAME_SIZE - 1);
  config.pattern[eye::PATTERN_NAME_SIZE - 1] = '\0';
  config.running = true;
  api.updateConfig(config);
  Serial.printf("[probe] Pattern: %s\n", next->label());
}

void begin() {
  Serial.println(F("[probe] Press the BOOT button briefly, or send n, to switch pattern."));
  lastReportMs = millis();
  lastReportFrames = animation.frameCount();
}

void loop() {
  const uint32_t now = millis();

  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 'n' || c == 'N') {
      nextPattern();
    }
  }

  if (now - lastReportMs >= REPORT_MS) {
    const uint32_t frames = animation.frameCount();
    Serial.printf("[probe] animation %.1f frames per second, free memory %u bytes\n",
                  (frames - lastReportFrames) * 1000.0f / static_cast<float>(now - lastReportMs),
                  static_cast<unsigned>(ESP.getFreeHeap()));
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
  Serial.printf("Firmware %s, built %s%s\n", buildinfo::version(), buildinfo::builtAt(),
                buildinfo::developmentBuild() ? " — DEVELOPMENT BUILD, do not distribute" : "");
  // Core 3.x reports the revision as major * 100 + minor.
  Serial.printf("Chip %s rev %u.%u, %u core(s), flash %u MB. Last start: %s.\n", ESP.getChipModel(),
                ESP.getChipRevision() / 100, ESP.getChipRevision() % 100, ESP.getChipCores(),
                static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)), systemcontrol::resetReason());
  Serial.printf("Firmware %u bytes of a %u-byte slot\n", static_cast<unsigned>(ESP.getSketchSize()),
                static_cast<unsigned>(OtaUpdate::slotBytes()));

  const bool ringOk = leds.begin();
  Serial.printf("LED ring: %u outputs at %u Hz, %u-bit — %s\n", static_cast<unsigned>(hw::RING_LED_COUNT),
                static_cast<unsigned>(hw::PWM_FREQUENCY_HZ), hw::PWM_RESOLUTION_BITS,
                ringOk ? "ready" : "FAILED to attach one or more pins");
  const bool onboardOk = onboardLed.begin();
  Serial.printf("Onboard LED: %s\n",
                !hw::ONBOARD_LED_PRESENT ? "not fitted on this board" : onboardOk ? "ready" : "FAILED to attach");

  Serial.println(F("Self-test: LED 1 -> LED 8, then fade."));
  selftest::run(leds);
  Serial.println(F("Self-test done."));

  eye::EyeConfig config = eye::defaultConfig();
  const bool restored = store.load(config);
  Serial.printf("Settings: %s\n", restored ? "restored from flash" : "using defaults (nothing usable saved)");

  const bool animationOk = animation.begin(config);
  Serial.printf("Animation: %u patterns, %u ms tick, pattern \"%s\" — %s\n",
                static_cast<unsigned>(animation.patterns().count()), static_cast<unsigned>(AnimationTask::TICK_MS),
                animation.config().pattern, animationOk ? "running" : "FAILED to start");

  auth.begin();
  factoryReset.begin();
  wifi.begin();
  api.begin();

#ifdef BRINGUP_PROBE
  probe::begin();
#endif
}

void loop() {
  wifi.loop();
  api.loop();
  store.loop();

  const FactoryReset::Event event = factoryReset.loop();
#ifdef BRINGUP_PROBE
  if (event == FactoryReset::Event::ShortPress) {
    probe::nextPattern();
  }
  probe::loop();
#else
  (void)event;
#endif

  if (systemcontrol::restartDue()) {
    // Anything changed in the last moments is written before the lights go out.
    store.saveNow();
    Serial.println(F("[system] Restarting."));
    Serial.flush();
    ESP.restart();
  }

  delay(5);
}
