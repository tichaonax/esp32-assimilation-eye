// Runs the LED animation in its own FreeRTOS task on a fixed tick, so nothing
// else the firmware does (web requests, saving settings) can stall it.
//
// Everything public is safe to call from any task once begin() has run.
#pragma once

#include <Arduino.h>

#include "Animator.h"
#include "EyeConfig.h"
#include "LedDriver.h"
#include "OnboardLed.h"
#include "PatternRegistry.h"

class AnimationTask {
 public:
  static constexpr uint32_t TICK_MS = 5;  // 200 frames per second

  // Something the ring shows instead of the pattern, to say what the board
  // itself is doing.
  enum class Indicator : uint8_t {
    None,
    Updating,        // firmware upload in progress: slow pulse
    ResetArming,     // BOOT held, factory reset coming: fast blink
    ResetConfirmed,  // factory reset done: steady light until the restart
  };

  AnimationTask(LedDriver& leds, OnboardLed& onboard);

  // Starts the task with the given settings (sanitized first; the defaults are
  // used if the pattern name is not registered). Call once, after the LED
  // outputs have been set up.
  bool begin(const eye::EyeConfig& initial);

  // A copy of the settings in force.
  eye::EyeConfig config() const;

  // Sanitizes the settings and hands them to the animation, which picks them
  // up on its next tick. Returns false, changing nothing, if the pattern name
  // is not a registered pattern.
  bool apply(eye::EyeConfig config);

  void setIndicator(Indicator indicator) { _indicator = indicator; }
  Indicator indicator() const { return _indicator; }

  const eye::PatternRegistry& patterns() const { return _registry; }

  // Frames drawn since start-up.
  uint32_t frameCount() const { return _frames; }

  // Longest time between two frames since this was last called, then starts
  // measuring afresh. 5 is perfect; a visible hitch is some tens.
  uint32_t takeWorstGapMs();

 private:
  static void taskEntry(void* self);
  void run();
  void drawIndicator(Indicator indicator, uint32_t elapsedMs, float frame[eye::LED_COUNT]);

  LedDriver& _leds;
  OnboardLed& _onboard;
  eye::PatternRegistry _registry;
  eye::Animator _animator;  // touched only by the animation task after begin()

  SemaphoreHandle_t _mutex = nullptr;
  eye::EyeConfig _shared;  // guarded by _mutex
  bool _dirty = false;     // guarded by _mutex

  TaskHandle_t _task = nullptr;
  volatile Indicator _indicator = Indicator::None;
  uint32_t _indicatorMs = 0;  // animation task only
  volatile uint32_t _frames = 0;
  volatile uint32_t _worstGapMs = 0;
};
