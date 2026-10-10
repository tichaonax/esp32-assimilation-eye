// A first look at an uploaded firmware file, before any of it is written to
// flash. Catches the common mistakes: the wrong file, a file for another chip,
// or one of the other files the build produces (firmware.factory.bin, which
// is for a USB flash of a blank board, or bootloader.bin).
//
// The updater still verifies the whole image (checksum and hash) at the end.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace eye {
namespace firmware {

// How much of the start of the file check() needs.
constexpr size_t HEADER_BYTES = 36;

constexpr uint8_t IMAGE_MAGIC = 0xE9;
constexpr size_t CHIP_ID_OFFSET = 12;  // 16-bit, little-endian
constexpr uint16_t CHIP_ID_ESP32 = 0x0000;
constexpr size_t APP_MAGIC_OFFSET = 32;  // 32-bit, little-endian
constexpr uint32_t APP_MAGIC = 0xABCD5432;

// nullptr if the start of the file looks like an application image for this
// chip that fits in slotBytes; otherwise a plain-language reason.
const char* check(const uint8_t* start, size_t length, size_t totalBytes, size_t slotBytes);

}  // namespace firmware
}  // namespace eye
