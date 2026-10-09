#include "Animator.h"

#include "Gamma.h"

namespace eye {

Animator::Animator(PatternRegistry& registry)
    : _registry(registry), _config(defaultConfig()), _active(registry.find(_config.pattern)) {}

bool Animator::setConfig(const EyeConfig& config) {
  EyeConfig candidate = config;
  sanitize(candidate);
  Pattern* pattern = _registry.find(candidate.pattern);
  if (pattern == nullptr) {
    return false;
  }
  if (pattern != _active) {
    pattern->reset();
    _active = pattern;
  }
  _config = candidate;
  return true;
}

void Animator::render(uint32_t elapsedMs, float out[LED_COUNT]) {
  if (!_config.running || _active == nullptr) {
    for (size_t i = 0; i < LED_COUNT; i++) {
      out[i] = 0.0f;
    }
    return;
  }

  _active->render(_config, elapsedMs, out);

  const float overall = static_cast<float>(_config.brightness) / 100.0f;
  for (size_t i = 0; i < LED_COUNT; i++) {
    out[i] = gamma::correct(out[i] * overall);
  }
}

}  // namespace eye
