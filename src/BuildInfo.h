// What this firmware is: version, when it was built, and whether it is a
// development build.
#pragma once

namespace buildinfo {

const char* version();

// Local time on the PC that built it, "YYYY-MM-DD HH:MM".
const char* builtAt();

// True if the firmware was built with a platformio.local.ini, which compiles a
// Wi-Fi name and password into it. Such a build must not be handed out.
bool developmentBuild();

// True if the firmware was built with a led-defaults.json, so a reset returns
// to those LED settings and not to the built-in ones.
bool customLedDefaults();

}  // namespace buildinfo
