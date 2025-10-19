#pragma once
#include <stdint.h>

namespace zlkm::hw {

// Display controller variants supported by this project
enum class ScreenController : uint8_t {
  SSD1306_128x64 = 0,
  SSD1309_128x64 = 1,
  SH1107_64x128 = 2,
};

}  // namespace zlkm::hw
