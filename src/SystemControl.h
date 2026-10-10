// Restarting the board on request, a moment later, so the reply that
// announced the restart has time to reach the browser.
#pragma once

#include <Arduino.h>

namespace systemcontrol {

// Safe to call from any task. A later call does not postpone an earlier one.
void scheduleRestart(uint32_t delayMs);

bool restartPending();

// True once the delay has passed. main.cpp saves settings and restarts.
bool restartDue();

// Why the board last started, in plain words ("power on", "restart requested" …).
const char* resetReason();

}  // namespace systemcontrol
