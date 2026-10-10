// Factory reset: back to the default sign-in, no saved network, the default
// access point name and password, and the default LED settings. The board
// then restarts.
//
// Two ways to ask for it:
//   - hold the BOOT button for ten seconds. After three seconds the ring
//     blinks fast as a warning; letting go before ten cancels.
//   - the System screen, which asks for the current password first.
//
// The BOOT button must be pressed after the board has started: holding it
// while power is applied puts the chip in its USB download mode instead.
#pragma once

#include <Arduino.h>

#include "AnimationTask.h"
#include "Auth.h"
#include "ConfigStore.h"
#include "WifiManager.h"

class FactoryReset {
 public:
  static constexpr uint32_t WARN_AFTER_MS = 3000;
  static constexpr uint32_t RESET_AFTER_MS = 10000;
  static constexpr uint32_t SHORT_PRESS_MAX_MS = 1000;

  enum class Event { None, ShortPress };

  FactoryReset(AnimationTask& animation, ConfigStore& store, Auth& auth, WifiManager& wifi);

  void begin();

  // Call from loop(): watches the button and carries out a requested reset.
  // Reports a short press so the caller can give it another use.
  Event loop();

  // Asks for a reset, which loop() then carries out. Safe from any task.
  void request() { _requested = true; }

 private:
  bool pressed() const;
  void perform();

  AnimationTask& _animation;
  ConfigStore& _store;
  Auth& _auth;
  WifiManager& _wifi;

  volatile bool _requested = false;
  bool _done = false;
  bool _stable = false;
  bool _last = false;
  uint32_t _changedMs = 0;
  uint32_t _pressedMs = 0;
  bool _warning = false;
};
