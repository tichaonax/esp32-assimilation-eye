// Assimilation Eye — firmware entry point.
//
// Brings up the LED ring and onboard LED, runs the power-on self-test, starts
// the animation with the saved settings, then Wi-Fi, sign-in and the web
// server.

#include <Arduino.h>

#include "AnimationTask.h"
#include "Auth.h"
#include "BuildInfo.h"
#include "ConfigStore.h"
#include "FactoryConfig.h"
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

  eye::EyeConfig config = eye::factoryConfig();
  const bool restored = store.load(config);
  Serial.printf("Settings: %s\n", restored ? "restored from flash" : "using defaults (nothing usable saved)");
  Serial.printf("LED defaults: %s\n",
                buildinfo::customLedDefaults() ? "from this build's led-defaults.json" : "built-in");

  const bool animationOk = animation.begin(config);
  Serial.printf("Animation: %u patterns, %u ms tick, pattern \"%s\" — %s\n",
                static_cast<unsigned>(animation.patterns().count()), static_cast<unsigned>(AnimationTask::TICK_MS),
                animation.config().pattern, animationOk ? "running" : "FAILED to start");

  auth.begin();
  factoryReset.begin();
  wifi.begin();
  api.begin();
}

void loop() {
  wifi.loop();
  api.loop();
  store.loop();

  factoryReset.loop();

  if (systemcontrol::restartDue()) {
    // Anything changed in the last moments is written before the lights go out.
    store.saveNow();
    Serial.println(F("[system] Restarting."));
    Serial.flush();
    ESP.restart();
  }

  delay(5);
}
