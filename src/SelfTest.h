// Power-on self-test: lights LED 1 to LED 8 in order, then fades the whole
// ring up and down once. Makes the wiring order visible at every start-up.
#pragma once

#include "LedDriver.h"

namespace selftest {

// Blocks for about three seconds. Called once from setup(), before anything
// else is running.
void run(LedDriver& leds);

}  // namespace selftest
