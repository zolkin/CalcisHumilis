#pragma once

// Select the current board by aliasing to the desired boards::<board>::Board

#include "platform/boards/pico2.h"

namespace zlkm::platform::boards::current {

using Board = ::zlkm::platform::boards::pico2::Board;

using PinSource = typename Board::PinSource;
using PinDefs = typename Board::PinDefs;
using PinId = typename Board::PinId;

}  // namespace zlkm::platform::boards::current
