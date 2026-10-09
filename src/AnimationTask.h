// Runs the LED animation in its own FreeRTOS task on a fixed tick, so nothing
// else the firmware does (web requests, saving settings) can stall it.
//
// config() and apply() are safe to call from any task.
#pragma once

#include <Arduino.h>

#include "Animator.h"
#include "EyeConfig.h"
#include "LedDriver.h"
#include "PatternRegistry.h"

class AnimationTask {
 public:
  static constexpr uint32_t TICK_MS = 5;  // 200 frames per second

  explicit AnimationTask(LedDriver& leds);

  // Starts the task with the default settings. Call once, after LedDriver::begin().
  bool begin();

  // A copy of the settings in force.
  eye::EyeConfig config() const;

  // Sanitizes the settings and hands them to the animation, which picks them
  // up on its next tick. Returns false, changing nothing, if the pattern name
  // is not a registered pattern.
  bool apply(eye::EyeConfig config);

  const eye::PatternRegistry& patterns() const { return _registry; }

  // Frames drawn since start-up; used to check the tick rate.
  uint32_t frameCount() const { return _frames; }

 private:
  static void taskEntry(void* self);
  void run();

  LedDriver& _leds;
  eye::PatternRegistry _registry;
  eye::Animator _animator;  // touched only by the animation task after begin()

  SemaphoreHandle_t _mutex = nullptr;
  eye::EyeConfig _shared;  // guarded by _mutex
  bool _dirty = false;     // guarded by _mutex

  TaskHandle_t _task = nullptr;
  volatile uint32_t _frames = 0;
};
