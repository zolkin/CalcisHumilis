#pragma once

#include <array>

#include "CalcisHumilis.h"
#include "audio/AudioTraits.h"
#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"

namespace zlkm::ch {

using CalcisTR = audio::AudioTraits<48000, 1, 32, 64>;
using Calcis = ch::CalcisHumilis<CalcisTR>;
using ScreenSSD = hw::Screen<hw::ScreenController::SSD1306_128x64>;

// Convenience aliases used by UI components (kept minimal here)

}  // namespace zlkm::ch
