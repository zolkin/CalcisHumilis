#pragma once

#include <array>

#include "CalcisHumilis.h"
#include "audio/AudioTraits.h"
#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"
#include "hw/io/McpPins.h"

namespace zlkm::ch {

using CalcisTR = audio::AudioTraits<48000, 1, 32, 64>;
using Calcis = ch::CalcisHumilis<CalcisTR>;
using ScreenSSD = hw::Screen<hw::ScreenController::SSD1306_128x64>;

// Convenience aliases used by UI components
using PinExpander = hw::io::Mcp23017Pins;
using TabButtons = hw::io::ButtonManager<4, PinExpander>;

}  // namespace zlkm::ch
