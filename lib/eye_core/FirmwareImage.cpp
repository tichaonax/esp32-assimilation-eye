#include "FirmwareImage.h"

namespace eye {
namespace firmware {

const char* check(const uint8_t* start, size_t length, size_t totalBytes, size_t slotBytes) {
  if (totalBytes > slotBytes) {
    return "The file is larger than the space reserved for firmware";
  }
  if (start == nullptr || length < HEADER_BYTES) {
    return "The file is too small to be firmware";
  }
  if (start[0] != IMAGE_MAGIC) {
    return "The file is not ESP32 firmware. Upload the firmware.bin produced by the build.";
  }
  const uint16_t chip = static_cast<uint16_t>(start[CHIP_ID_OFFSET] | (start[CHIP_ID_OFFSET + 1] << 8));
  if (chip != CHIP_ID_ESP32) {
    return "The firmware was built for a different chip";
  }
  const uint32_t magic = static_cast<uint32_t>(start[APP_MAGIC_OFFSET]) |
                         (static_cast<uint32_t>(start[APP_MAGIC_OFFSET + 1]) << 8) |
                         (static_cast<uint32_t>(start[APP_MAGIC_OFFSET + 2]) << 16) |
                         (static_cast<uint32_t>(start[APP_MAGIC_OFFSET + 3]) << 24);
  if (magic != APP_MAGIC) {
    // bootloader.bin, for one, passes the checks above.
    return "The file is ESP32 code but not the application. Upload firmware.bin.";
  }
  return nullptr;
}

}  // namespace firmware
}  // namespace eye
