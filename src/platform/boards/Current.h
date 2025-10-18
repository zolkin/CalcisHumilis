#pragma once

// Select the current board by aliasing to the desired boards::<board>::Board

#ifdef ARDUINO_RASPBERRY_PI_PICO_2

#include "platform/boards/pico2.h"

namespace zlkm::platform::boards {
using Current = ::zlkm::platform::boards::pico2::Board;
}  // namespace zlkm::platform::boards

#endif  // ARDUINO_RASPBERRY_PI_PICO_2
