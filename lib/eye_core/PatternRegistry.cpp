#include "PatternRegistry.h"

#include <string.h>

namespace eye {

PatternRegistry::PatternRegistry() {
  // One line per pattern. The first one registered is the default.
  add(&_comet);
  add(&_flash);
  add(&_blend);
}

bool PatternRegistry::add(Pattern* pattern) {
  if (pattern == nullptr || _count >= CAPACITY || find(pattern->name()) != nullptr) {
    return false;
  }
  _patterns[_count++] = pattern;
  return true;
}

void PatternRegistry::seed(uint32_t value) {
  for (size_t i = 0; i < _count; i++) {
    _patterns[i]->seed(value + static_cast<uint32_t>(i));
  }
}

Pattern* PatternRegistry::find(const char* name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < _count; i++) {
    if (strcmp(_patterns[i]->name(), name) == 0) {
      return _patterns[i];
    }
  }
  return nullptr;
}

}  // namespace eye
