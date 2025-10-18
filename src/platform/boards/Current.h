#pragma once

// Select the current board by aliasing to the desired boards::<board>::Board

#ifdef ARDUINO_RASPBERRY_PI_PICO_2
#include "platform/boards/pico2.h"
#endif

namespace zlkm::platform::boards::current {
#ifdef ARDUINO_RASPBERRY_PI_PICO_2
using Board = ::zlkm::platform::boards::pico2::Board;
using PinSource = typename Board::PinSource;
using PinDefs = typename Board::PinDefs;
using PinId = typename Board::PinId;
#endif
}  // namespace zlkm::platform::boards::current
