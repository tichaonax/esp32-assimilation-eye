#include "AnimationTask.h"

#include "HardwareConfig.h"

static_assert(eye::LED_COUNT == hw::RING_LED_COUNT, "pattern code and pin map disagree on the LED count");

namespace {
constexpr uint32_t STACK_BYTES = 4096;
constexpr UBaseType_t PRIORITY = 2;  // above loop() (1), below the Wi-Fi stack
constexpr BaseType_t CORE = 1;       // the Wi-Fi stack lives on core 0
}  // namespace

AnimationTask::AnimationTask(LedDriver& leds) : _leds(leds), _animator(_registry), _shared(eye::defaultConfig()) {}

bool AnimationTask::begin() {
  if (_task != nullptr) {
    return true;
  }
  _mutex = xSemaphoreCreateMutex();
  if (_mutex == nullptr) {
    return false;
  }
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

void AnimationTask::taskEntry(void* self) {
  static_cast<AnimationTask*>(self)->run();
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

    _animator.render(elapsed, frame);
    for (size_t i = 0; i < eye::LED_COUNT; i++) {
      _leds.set(i, frame[i]);
    }
    _frames = _frames + 1;
  }
}
