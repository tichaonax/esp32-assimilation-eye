#include "SystemControl.h"

#include <esp_system.h>

namespace systemcontrol {

namespace {
volatile bool pending = false;
volatile uint32_t dueAtMs = 0;
}  // namespace

void scheduleRestart(uint32_t delayMs) {
  if (pending) {
    return;
  }
  dueAtMs = millis() + delayMs;
  pending = true;
}

bool restartPending() {
  return pending;
}

bool restartDue() {
  return pending && static_cast<int32_t>(millis() - dueAtMs) >= 0;
}

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "power on";
    case ESP_RST_SW:
      return "restart requested";
    case ESP_RST_PANIC:
      return "firmware crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return "watchdog (firmware stopped responding)";
    case ESP_RST_BROWNOUT:
      return "supply voltage dropped too low";
    case ESP_RST_DEEPSLEEP:
      return "woke from sleep";
    case ESP_RST_EXT:
      return "reset pin";
    default:
      return "unknown";
  }
}

}  // namespace systemcontrol
