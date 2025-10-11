#pragma once

#include <array>
#include <cstdint>

// Centralized board/expander pin map.
// Only static constexpr members; use from anywhere to avoid magic numbers.
namespace zlkm::platform {

struct Pins {
  // MCU GPIOs
  static constexpr uint8_t I2C_SDA = 20;
  static constexpr uint8_t I2C_SCL = 21;
  static constexpr uint8_t TRIG_IN = 26;
  static constexpr uint8_t LED_TRIGGER = 27;
  static constexpr uint8_t LED_CLIPPING = 28;

  // OLED/SPI wiring (SPI0)
  static constexpr uint8_t OLED_SCK = 6;   // SPI0 SCK
  static constexpr uint8_t OLED_MOSI = 7;  // SPI0 TX (MOSI)
  static constexpr uint8_t OLED_DC = 9;    // OLED D/C
  static constexpr uint8_t OLED_RST = 8;   // OLED RESET

  // MCP23017 expander pins (0..15)
  inline static constexpr std::array<uint8_t, 4> LEDS{0, 1, 2, 3};
  inline static constexpr std::array<uint8_t, 4> TAB_BUTTONS{4, 5, 6, 7};
  inline static constexpr std::array<uint8_t, 4> ENCODER_A{8, 10, 12, 14};
  inline static constexpr std::array<uint8_t, 4> ENCODER_B{9, 11, 13, 15};
};

}  // namespace zlkm::platform
