#pragma once
#include <unity.h>

#include "platform/platform.h"

#if defined(ARDUINO)
#define TEST_MAIN() \
  void loop() {}    \
  void setup()

// Waits for serial on embbedded platforms
#define PLATFORM_TEST_BEGIN()                      \
  do {                                             \
    Serial.begin(115200);                          \
    unsigned long start = millis();                \
    while (!Serial && (millis() - start) < 5000) { \
      delay(10);                                   \
    }                                              \
  } while (0)
#else  // native build

#define TEST_MAIN() int main(int, char**)

// does nothing on native builds
#define PLATFORM_TEST_BEGIN() void(0)

#endif  // ARDUINO