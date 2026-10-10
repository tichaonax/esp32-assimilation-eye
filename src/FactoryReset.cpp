#include "FactoryReset.h"

#include "EyeConfig.h"
#include "HardwareConfig.h"
#include "SystemControl.h"

namespace {
constexpr uint32_t DEBOUNCE_MS = 40;
// Long enough to see the ring's steady "done" light before the restart.
constexpr uint32_t RESTART_DELAY_MS = 2000;
}  // namespace

FactoryReset::FactoryReset(AnimationTask& animation, ConfigStore& store, Auth& auth, WifiManager& wifi)
    : _animation(animation), _store(store), _auth(auth), _wifi(wifi) {}

void FactoryReset::begin() {
  pinMode(hw::BOOT_BUTTON_PIN, INPUT_PULLUP);
}

bool FactoryReset::pressed() const {
  const bool low = digitalRead(hw::BOOT_BUTTON_PIN) == LOW;
  return hw::BOOT_BUTTON_ACTIVE_LOW ? low : !low;
}

void FactoryReset::perform() {
  _done = true;
  Serial.println(F("[reset] FACTORY RESET: default sign-in, no saved network, default access point, default LEDs."));
  _auth.factoryReset();
  _wifi.eraseSaved();
  _animation.apply(eye::defaultConfig());
  _store.saveNow();
  _animation.setIndicator(AnimationTask::Indicator::ResetConfirmed);
  systemcontrol::scheduleRestart(RESTART_DELAY_MS);
}

FactoryReset::Event FactoryReset::loop() {
  if (_done) {
    return Event::None;
  }
  if (_requested) {
    perform();
    return Event::None;
  }

  const uint32_t now = millis();
  const bool raw = pressed();
  if (raw != _last) {
    _last = raw;
    _changedMs = now;
    return Event::None;
  }
  if (raw != _stable && now - _changedMs >= DEBOUNCE_MS) {
    _stable = raw;
    if (_stable) {
      _pressedMs = now;
    } else {
      const uint32_t held = now - _pressedMs;
      if (_warning) {
        _warning = false;
        _animation.setIndicator(AnimationTask::Indicator::None);
        Serial.println(F("[reset] BOOT released early; factory reset cancelled."));
      }
      if (held <= SHORT_PRESS_MAX_MS) {
        return Event::ShortPress;
      }
    }
    return Event::None;
  }

  if (_stable) {
    const uint32_t held = now - _pressedMs;
    if (held >= RESET_AFTER_MS) {
      perform();
    } else if (held >= WARN_AFTER_MS && !_warning) {
      _warning = true;
      _animation.setIndicator(AnimationTask::Indicator::ResetArming);
      Serial.printf("[reset] BOOT held: keep holding %u more seconds for a factory reset, or let go to cancel.\n",
                    static_cast<unsigned>((RESET_AFTER_MS - WARN_AFTER_MS) / 1000));
    }
  }
  return Event::None;
}
