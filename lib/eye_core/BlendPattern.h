// Comet and flash combined: the ring shows the comet for a while, crossfades
// into the flash, shows the flash for a while and crossfades back, for ever.
//
// It uses the comet and flash settings as they are. Both run underneath the
// whole time, so neither restarts when it fades back in. With random
// direction on, the comet's direction is picked afresh each time the flash
// has fully taken over, which is the one moment the change cannot be seen.
#pragma once

#include "CometPattern.h"
#include "FlashPattern.h"
#include "Pattern.h"

namespace eye {

class BlendPattern : public Pattern {
 public:
  enum class Stage : uint8_t { Comet = 0, ToFlash = 1, Flash = 2, ToComet = 3 };

  const char* name() const override { return "blend"; }
  const char* label() const override { return "Comet and flash"; }
  void reset() override;
  void seed(uint32_t value) override;
  void render(const EyeConfig& config, uint32_t elapsedMs, float out[LED_COUNT]) override;

  Stage stage() const { return _stage; }

  // How much of the frame is the flash: 0 = only the comet, 1 = only the flash.
  float mix() const;

  // The direction the comet is travelling in, as of the last render().
  Direction direction() const { return _direction; }

 private:
  void advance(const BlendSettings& blend, uint32_t elapsedMs);
  void enter(Stage stage, bool randomDirection);
  uint32_t nextRandom();

  CometPattern _comet;
  FlashPattern _flash;

  Stage _stage = Stage::Comet;
  // Time into a Comet or Flash stage. A crossfade is tracked as a fraction
  // instead, so changing the crossfade time part-way through does not jump.
  float _stageMs = 0.0f;
  float _fade = 0.0f;

  Direction _direction = Direction::Clockwise;
  bool _hasDirection = false;
  uint32_t _random = 0x9E3779B9u;
};

}  // namespace eye
