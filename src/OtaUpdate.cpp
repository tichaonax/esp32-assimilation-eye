#include "OtaUpdate.h"

#include <Update.h>

#include "FirmwareImage.h"

OtaUpdate::OtaUpdate(AnimationTask& animation) : _animation(animation) {}

size_t OtaUpdate::slotBytes() {
  return ESP.getFreeSketchSpace();
}

// Ends the update and reports what the updater said went wrong.
const char* OtaUpdate::fail() {
  snprintf(_message, sizeof(_message), "The update failed: %s", Update.errorString());
  abort();
  return _message;
}

const char* OtaUpdate::start(const uint8_t* first, size_t length, size_t uploadBytes) {
  if (_active) {
    return "Another update is already in progress";
  }
  const char* problem = eye::firmware::check(first, length, uploadBytes, slotBytes());
  if (problem != nullptr) {
    return problem;
  }
  // The exact image size is not known (the upload's size includes a little
  // wrapping), so the updater is left to size it as it goes.
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    return fail();
  }
  _active = true;
  _written = 0;
  _animation.setIndicator(AnimationTask::Indicator::Updating);
  Serial.println(F("[update] Firmware upload started."));
  return nullptr;
}

const char* OtaUpdate::write(const uint8_t* data, size_t length) {
  if (!_active) {
    return "No update is in progress";
  }
  // The updater takes a non-const pointer but does not change the data.
  if (Update.write(const_cast<uint8_t*>(data), length) != length) {
    return fail();
  }
  _written += length;
  return nullptr;
}

const char* OtaUpdate::finish() {
  if (!_active) {
    return "No update is in progress";
  }
  // true: accept the size written so far as the size of the image. This is
  // where the image's own checksum and hash are verified.
  if (!Update.end(true)) {
    return fail();
  }
  _active = false;
  Serial.printf("[update] Firmware accepted (%u bytes). It will run after the restart.\n",
                static_cast<unsigned>(_written));
  // The pulse is left on: the restart follows.
  return nullptr;
}

void OtaUpdate::abort() {
  if (_active) {
    Update.abort();
    _active = false;
    Serial.println(F("[update] Firmware upload abandoned; the running firmware is unchanged."));
  }
  _animation.setIndicator(AnimationTask::Indicator::None);
}
