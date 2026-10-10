// Hardware-independent unit tests. The same files run on the PC
// (pio test -e native) and on the board (pio test -e esp32dev).

#include <unity.h>

#include "tests.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

void setUp() {}
void tearDown() {}

static int runAllTests() {
  UNITY_BEGIN();
  runHardwareTests();
  runConfigTests();
  runGammaTests();
  runCometTests();
  runFlashTests();
  runBlendTests();
  runAnimatorTests();
  runOnboardTests();
  runJsonTests();
  runRulesTests();
  return UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000);  // give the test runner time to open the serial port
  runAllTests();
}

void loop() {}
#else
int main() {
  return runAllTests();
}
#endif
