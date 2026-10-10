// Firmware update over the air: takes an uploaded firmware.bin in pieces,
// writes it to the spare firmware slot and, if the whole image checks out,
// makes that slot the one to start from.
//
// The running firmware is never touched. An upload that is refused, cut off
// or fails its final check leaves the board exactly as it was.
//
// The ring shows a slow pulse for as long as an update is in progress.
//
// Each step returns nullptr on success, or a message saying what went wrong.
#pragma once

#include <Arduino.h>

#include "AnimationTask.h"

class OtaUpdate {
 public:
  explicit OtaUpdate(AnimationTask& animation);

  // Looks at the start of the file and, if it is plausible firmware for this
  // board, prepares the spare slot. uploadBytes is the size of the upload as
  // announced by the browser.
  const char* start(const uint8_t* first, size_t length, size_t uploadBytes);

  const char* write(const uint8_t* data, size_t length);

  // Verifies the complete image and selects it for the next start.
  const char* finish();

  // Abandons an update in progress. Safe to call when there is none.
  void abort();

  bool active() const { return _active; }

  // Size of the slot an update is written to.
  static size_t slotBytes();

 private:
  const char* fail();

  AnimationTask& _animation;
  volatile bool _active = false;
  size_t _written = 0;
  char _message[96] = {};
};
