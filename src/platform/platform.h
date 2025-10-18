#pragma once

// Unified platform header for Arduino/Pico and native builds.
// Include this instead of <Arduino.h> or Pico headers directly.

// Timing helpers (millis/micros) are defined for non-Arduino builds below.

#if defined(ARDUINO)

#include <Arduino.h>
#include <ArduinoLog.h>

#if defined(ARDUINO_ARCH_RP2350) || defined(PICO_RP2350) || defined(PICO_BOARD)
namespace zlkm::platform {
inline uint8_t get_core_num() { return ::get_core_num(); }
inline char* getSdkVersion() {
  static char versionStr[64];
  snprintf(versionStr, sizeof(versionStr), "PicoSdk: %d.%d.%d",
           PICO_SDK_VERSION_MAJOR, PICO_SDK_VERSION_MINOR,
           PICO_SDK_VERSION_REVISION);
  return versionStr;
}
}  // namespace zlkm::platform
#endif

#elif defined(ARDUINO_ARCH_RP2350) || defined(PICO_RP2350) || \
    defined(PICO_BOARD)
// Pico SDK / Arduino RP2350 variants
#include <hardware/timer.h>
#include <pico/platform.h>
#include <stdint.h>
// Provide Arduino-like millis()/micros() shims if not present
#ifndef millis
static inline uint32_t millis() {
  return to_ms_since_boot(get_absolute_time());
}
#endif
#ifndef micros
static inline uint32_t micros() {
  // Absolute time in us since boot
  return (uint32_t)to_us_since_boot(get_absolute_time());
}
#endif
#else
// Native / host build
#include <stdint.h>

#include <chrono>
static inline uint32_t millis() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<milliseconds>(
             steady_clock::now().time_since_epoch())
      .count();
}
static inline uint32_t micros() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<microseconds>(
             steady_clock::now().time_since_epoch())
      .count();
}
#endif
