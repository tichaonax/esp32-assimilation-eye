// The list of available patterns, looked up by name.
#pragma once

#include <stddef.h>

#include <stdint.h>

#include "BlendPattern.h"
#include "CometPattern.h"
#include "FlashPattern.h"
#include "Pattern.h"

namespace eye {

class PatternRegistry {
 public:
  static constexpr size_t CAPACITY = 8;

  // Registers the built-in patterns.
  PatternRegistry();

  // Returns false if the registry is full or the name is already taken.
  bool add(Pattern* pattern);

  size_t count() const { return _count; }
  Pattern* at(size_t index) const { return index < _count ? _patterns[index] : nullptr; }

  // nullptr if no pattern has that name.
  Pattern* find(const char* name) const;

  // Seeds every pattern that leaves something to chance.
  void seed(uint32_t value);

 private:
  Pattern* _patterns[CAPACITY] = {};
  size_t _count = 0;

  CometPattern _comet;
  FlashPattern _flash;
  BlendPattern _blend;
};

}  // namespace eye
