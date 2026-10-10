// Keeps the LED settings in flash (NVS) so the prop restarts as it was left.
//
// Settings are stored as the same JSON the API uses, next to a layout version.
// A missing, unreadable or differently-versioned copy is ignored and the
// defaults are used, so a firmware change can never load garbage.
//
// Saving is debounced: a slider drag produces many changes a second, and only
// the settings in force a couple of seconds after the last one are written.
#pragma once

#include <Arduino.h>

#include "AnimationTask.h"
#include "EyeConfig.h"

class ConfigStore {
 public:
  static constexpr uint16_t LAYOUT_VERSION = 1;
  static constexpr uint32_t SAVE_DELAY_MS = 2000;

  explicit ConfigStore(AnimationTask& animation);

  // Reads the saved settings. Returns false, leaving config untouched, if
  // there are none that can be used.
  bool load(eye::EyeConfig& config);

  // Notes that the settings changed. Safe to call from any task.
  void requestSave();

  // Call from loop(): writes the settings once they have been quiet for
  // SAVE_DELAY_MS. Skips the write if nothing differs from what is stored.
  void loop();

  // Writes the current settings now. Returns false if the write failed.
  bool saveNow();

 private:
  AnimationTask& _animation;
  String _stored;  // what is in flash, to avoid rewriting the same thing
  volatile bool _pending = false;
  volatile uint32_t _requestedAtMs = 0;
};
