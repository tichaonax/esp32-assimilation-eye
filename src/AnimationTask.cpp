#include "AnimationTask.h"

#include <esp_random.h>
#include <math.h>

#include "FactoryConfig.h"
#include "Gamma.h"
#include "HardwareConfig.h"

static_assert(eye::LED_COUNT == hw::RING_LED_COUNT, "pattern code and pin map disagree on the LED count");

namespace {
constexpr uint32_t STACK_BYTES = 4096;
// Above the web server's task (10), so a slow request such as checking a
// password cannot hold up a frame; below the Wi-Fi stack (23). A frame takes
// well under a millisecond, so this starves nothing.
constexpr UBaseType_t PRIORITY = 18;
constexpr BaseType_t CORE = 1;  // the Wi-Fi stack lives on core 0

constexpr uint32_t UPDATING_PERIOD_MS = 1600;
constexpr uint32_t ARMING_PERIOD_MS = 250;
}  // namespace

AnimationTask::AnimationTask(LedDriver& leds, OnboardLed& onboard)
    : _leds(leds), _onboard(onboard), _animator(_registry), _shared(eye::defaultConfig()) {}

bool AnimationTask::begin(const eye::EyeConfig& initial) {
  if (_task != nullptr) {
    return true;
  }
  _mutex = xSemaphoreCreateMutex();
  if (_mutex == nullptr) {
    return false;
  }
  _registry.seed(esp_random());

  // No task yet, so the animator can be set up directly.
  if (!_animator.setConfig(initial)) {
    if (!_animator.setConfig(eye::factoryConfig())) {
      _animator.setConfig(eye::defaultConfig());
    }
  }
  _animator.restart();
  _shared = _animator.config();

  return xTaskCreatePinnedToCore(taskEntry, "animation", STACK_BYTES, this, PRIORITY, &_task, CORE) == pdPASS;
}

eye::EyeConfig AnimationTask::config() const {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const eye::EyeConfig copy = _shared;
  xSemaphoreGive(_mutex);
  return copy;
}

bool AnimationTask::apply(eye::EyeConfig config) {
  eye::sanitize(config);
  if (_registry.find(config.pattern) == nullptr) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _shared = config;
  _dirty = true;
  xSemaphoreGive(_mutex);
  return true;
}

uint32_t AnimationTask::takeWorstGapMs() {
  const uint32_t worst = _worstGapMs;
  _worstGapMs = 0;
  return worst;
}

void AnimationTask::taskEntry(void* self) {
  static_cast<AnimationTask*>(self)->run();
}

void AnimationTask::drawIndicator(Indicator indicator, uint32_t elapsedMs, float frame[eye::LED_COUNT]) {
  _indicatorMs += elapsedMs;
  float level = 0.0f;
  switch (indicator) {
    case Indicator::Updating: {
      const float phase = static_cast<float>(_indicatorMs % UPDATING_PERIOD_MS) / UPDATING_PERIOD_MS;
      level = 0.15f + 0.45f * (0.5f - 0.5f * cosf(phase * 2.0f * static_cast<float>(M_PI)));
      break;
    }
    case Indicator::ResetArming:
      level = (_indicatorMs % ARMING_PERIOD_MS) < ARMING_PERIOD_MS / 2 ? 0.7f : 0.0f;
      break;
    case Indicator::ResetConfirmed:
      level = 0.9f;
      break;
    case Indicator::None:
      break;
  }
  const float duty = eye::gamma::correct(level);
  for (size_t i = 0; i < eye::LED_COUNT; i++) {
    frame[i] = duty;
  }
}

void AnimationTask::run() {
  float frame[eye::LED_COUNT];
  eye::EyeConfig incoming;
  TickType_t wake = xTaskGetTickCount();
  uint32_t lastMs = millis();

  for (;;) {
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(TICK_MS));

    // The whole settings struct is copied under the lock and used outside it,
    // so the lock is never held while drawing.
    bool changed = false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (_dirty) {
      incoming = _shared;
      _dirty = false;
      changed = true;
    }
    xSemaphoreGive(_mutex);
    if (changed) {
      _animator.setConfig(incoming);
    }

    // Measured, not assumed: a late tick moves the animation by the time that
    // really passed, so speed stays right.
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastMs;
    lastMs = now;
    if (elapsed > _worstGapMs) {
      _worstGapMs = elapsed;
    }

    const Indicator indicator = _indicator;
    if (indicator == Indicator::None) {
      _indicatorMs = 0;
      _animator.render(elapsed, frame);
    } else {
      // The pattern is paused, not advanced, so it resumes where it was.
      _animator.render(0, frame);
      drawIndicator(indicator, elapsed, frame);
    }

    for (size_t i = 0; i < eye::LED_COUNT; i++) {
      _leds.set(i, frame[i]);
    }
    _onboard.set(_animator.onboardLevel());
    _frames = _frames + 1;
  }
}
