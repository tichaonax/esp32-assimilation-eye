// Perceptual brightness correction.
//
// The eye responds to light roughly logarithmically, so a PWM duty of 50%
// looks far brighter than "half". Patterns work in perceived brightness
// (0..1); this converts to the fraction of light the LED must actually emit.
#pragma once

namespace eye {
namespace gamma {

constexpr float EXPONENT = 2.2f;

// Perceived brightness 0..1 -> PWM duty fraction 0..1. Out-of-range input is
// clamped and NaN is treated as 0.
float correct(float level);

}  // namespace gamma
}  // namespace eye
