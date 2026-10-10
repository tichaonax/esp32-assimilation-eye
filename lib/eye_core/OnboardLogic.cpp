#include "OnboardLogic.h"

namespace eye {

void OnboardLogic::reset() {
  _heartbeatMs = 0;
  _blinking = false;
  _blinkMs = 0;
}

void OnboardLogic::notifyPatternChange() {
  _blinking = true;
  _blinkMs = 0;
}

float OnboardLogic::update(const OnboardSettings& settings, uint32_t elapsedMs) {
  const float level = static_cast<float>(settings.brightness) / 100.0f;

  if (settings.mode != OnboardMode::BlinkOnPatternChange) {
    // A burst left over from another mode must not play later.
    _blinking = false;
  }

  switch (settings.mode) {
    case OnboardMode::Off:
      return 0.0f;

    case OnboardMode::On:
      return level;

    case OnboardMode::Heartbeat: {
      _heartbeatMs = (_heartbeatMs + elapsedMs % HEARTBEAT_PERIOD_MS) % HEARTBEAT_PERIOD_MS;
      const uint32_t secondBeatStart = HEARTBEAT_BEAT_MS + HEARTBEAT_GAP_MS;
      const bool firstBeat = _heartbeatMs < HEARTBEAT_BEAT_MS;
      const bool secondBeat = _heartbeatMs >= secondBeatStart && _heartbeatMs < secondBeatStart + HEARTBEAT_BEAT_MS;
      return (firstBeat || secondBeat) ? level : 0.0f;
    }

    case OnboardMode::BlinkOnPatternChange: {
      if (!_blinking) {
        return 0.0f;
      }
      const uint32_t onMs = settings.blinkMs > 0 ? static_cast<uint32_t>(settings.blinkMs) : 1;
      const uint32_t count = settings.blinkCount > 0 ? static_cast<uint32_t>(settings.blinkCount) : 1;
      const uint32_t period = onMs * 2;
      const uint32_t total = period * count;
      // Stop at the end instead of wrapping, however long the gap was.
      if (_blinkMs >= total || elapsedMs >= total - _blinkMs) {
        _blinkMs = total;
      } else {
        _blinkMs += elapsedMs;
      }
      if (_blinkMs >= total) {
        _blinking = false;
        return 0.0f;
      }
      return (_blinkMs % period) < onMs ? level : 0.0f;
    }
  }
  return 0.0f;
}

}  // namespace eye
