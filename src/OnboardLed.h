// PWM output for the board's own LED. What it should show is decided in
// lib/eye_core/OnboardLogic; this only drives the pin.
#pragma once

class OnboardLed {
 public:
  // Does nothing, and returns true, on a board without a controllable LED.
  bool begin();

  // PWM duty fraction, 0.0 (off) to 1.0 (full). Out-of-range values are clamped.
  void set(float level);

  bool available() const;

 private:
  bool _attached = false;
};
