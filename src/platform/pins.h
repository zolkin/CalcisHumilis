#pragma once

#include <array>
#include <cstdint>

#include "hw/io/Pin.h"

// Centralized board/expander pin map.
// Only static constexpr members; use from anywhere to avoid magic numbers.
namespace zlkm::platform {

struct Pins {
  using Pin = zlkm::hw::io::Pin;
  template <size_t N>
  using PinArray = zlkm::hw::io::PinArray<N>;
  // MCU GPIOs
  static constexpr Pin I2C_SDA{20};
  static constexpr Pin I2C_SCL{21};
  static constexpr Pin TRIG_IN{26};
  static constexpr Pin LED_TRIGGER{27};
  static constexpr Pin LED_CLIPPING{28};

  // OLED/SPI wiring (SPI0)
  static constexpr Pin OLED_SCK{6};   // SPI0 SCK
  static constexpr Pin OLED_MOSI{7};  // SPI0 TX (MOSI)
  static constexpr Pin OLED_DC{9};    // OLED D/C
  static constexpr Pin OLED_RST{8};   // OLED RESET

  // MCP23017 expander pins (0..15)
  inline static constexpr PinArray<4> LEDS{Pin{0}, Pin{1}, Pin{2}, Pin{3}};
  inline static constexpr PinArray<4> TAB_BUTTONS{Pin{4}, Pin{5}, Pin{6},
                                                  Pin{7}};
  inline static constexpr PinArray<4> ENCODER_A{Pin{8}, Pin{10}, Pin{12},
                                                Pin{14}};
  inline static constexpr PinArray<4> ENCODER_B{Pin{9}, Pin{11}, Pin{13},
                                                Pin{15}};
};

}  // namespace zlkm::platform
