// The LED settings a reset returns to: the built-in defaults from
// EyeConfig.h, with this build's led-defaults.json laid on top.
//
// led-defaults.json sits next to platformio.ini and is optional. It holds any
// subset of the settings, in the shape ConfigJson.h describes.
// scripts/led_defaults.py checks it before each build and compiles its text
// in; scripts/save_led_defaults.py writes it from the settings on a board.
#pragma once

#include "EyeConfig.h"

namespace eye {

// defaultConfig() if the build has no led-defaults.json.
EyeConfig factoryConfig();

// True if this build was made with a led-defaults.json.
bool factoryConfigIsCustom();

// The text of that file as compiled in, or "" if there was none.
const char* factoryConfigJson();

}  // namespace eye
