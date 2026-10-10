#include "BuildInfo.h"

#include "FactoryConfig.h"
#include "generated/build_info.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

namespace buildinfo {

const char* version() {
  return FIRMWARE_VERSION;
}

const char* builtAt() {
  return BUILD_TIMESTAMP;
}

bool developmentBuild() {
#ifdef DEV_WIFI_SSID
  return true;
#else
  return false;
#endif
}

bool customLedDefaults() {
  return eye::factoryConfigIsCustom();
}

}  // namespace buildinfo
