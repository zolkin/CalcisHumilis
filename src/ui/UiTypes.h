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

struct RotaryInputSpec {
  enum Response {
    RsLin,
    RsExp,
    RsRate,
    RsGCut,
    RsKDamp,
    RsMorph,
    RsDrive,
    RsInt,
    RsBool
  };
  float outMin = 0.0f;
  float outMax = 1.0f;
  Response response = RsLin;
  void* cfgValue = nullptr;
  int encSpanCounts = 0;
  int encOffset = 0;
  bool encWrap = false;
  bool encInvert = false;
  template <class T>
  inline void setCfgValue(T value) const {
    *reinterpret_cast<T*>(cfgValue) = value;
  }
};

}  // namespace zlkm::ch
