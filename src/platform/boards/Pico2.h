#pragma once

// Board wiring and PinSource composition for Raspberry Pi Pico 2
#include <Wire.h>

#include <tuple>

#include "hw/io/GpioPins.h"
#include "hw/io/McpPins.h"
#include "hw/io/PinMux.h"
#include "platform/Board.h"

namespace zlkm::platform::boards::pico2 {

// BoardT is declared in platform/Board.h

// Current board: Pico 2
// Compose a pin source for the board. On-board GPIOs are handled by GpioPins
// and the MCP23017 expander by McpPins, muxed by PinMux.

// Concrete typedefs for readability
static constexpr int GPIO_PIN_COUNT = 30;
using GpioPins =
    zlkm::hw::io::GpioPins<GPIO_PIN_COUNT>;     // identity map 0..29 to GPIO
using ExpMcpPins = zlkm::hw::io::Mcp23017Pins;  // 16-pin expander
using PinSource = zlkm::hw::io::PinMux<uint8_t, GpioPins, ExpMcpPins>;

// Board-specific PinDefs constants now use the PinSource::PinIdT (MuxPin)
struct PinDefs {
  using SrcPin = typename PinSource::PinIdT;
  using PinId = zlkm::hw::io::PinId;  // low-level raw pin id
  using PinGroupId = zlkm::hw::io::PinGroupId;

  static constexpr PinGroupId GROUP_GPIO = PinGroupId{0};  // default group
  static constexpr PinGroupId GROUP_EXPANDER = PinGroupId{1};

  template <size_t N>
  using SrcPinArray = std::array<SrcPin, N>;

  // MCU GPIOs
  inline static const SrcPin I2C_SDA{20};
  inline static const SrcPin I2C_SCL{21};
  inline static const SrcPin TRIG_IN{26};
  inline static const SrcPin LED_TRIGGER{27};
  inline static const SrcPin LED_CLIPPING{28};

  // OLED/SPI wiring (SPI0)
  inline static const SrcPin OLED_SCK{6};   // SCK
  inline static const SrcPin OLED_MOSI{7};  // TX
  inline static const SrcPin OLED_DC{9};    // D/C
  inline static const SrcPin OLED_RST{8};   // RESET

  // MCP23017 expander pins (0..15)
  inline static const SrcPinArray<4> LEDS{
      SrcPin{0, GROUP_EXPANDER},
      SrcPin{1, GROUP_EXPANDER},
      SrcPin{2, GROUP_EXPANDER},
      SrcPin{3, GROUP_EXPANDER},
  };
  inline static const SrcPinArray<4> TAB_BUTTONS{
      SrcPin{4, GROUP_EXPANDER},
      SrcPin{5, GROUP_EXPANDER},
      SrcPin{6, GROUP_EXPANDER},
      SrcPin{7, GROUP_EXPANDER},
  };
  inline static const SrcPinArray<4> ENCODER_A{
      SrcPin{8, GROUP_EXPANDER},
      SrcPin{10, GROUP_EXPANDER},
      SrcPin{12, GROUP_EXPANDER},
      SrcPin{14, GROUP_EXPANDER},
  };
  inline static const SrcPinArray<4> ENCODER_B{
      SrcPin{9, GROUP_EXPANDER},
      SrcPin{11, GROUP_EXPANDER},
      SrcPin{13, GROUP_EXPANDER},
      SrcPin{15, GROUP_EXPANDER},
  };
};

inline GpioPins& gpioPins() {
  using namespace zlkm::hw::io;
  static GpioPins s_gpio(defaultPinSet<GPIO_PIN_COUNT>(), PinMode::Input);
  return s_gpio;
}

inline ExpMcpPins& expanderPins() {
  using namespace zlkm::hw::io;
  static ExpMcpPins s_exp(I2cCfg{
      .address = 0x20,
      .wire = &Wire,
      .clockHz = 0,
      .i2cSDA = PinDefs::I2C_SDA.pin(),
      .i2cSCL = PinDefs::I2C_SCL.pin(),
  });
  return s_exp;
}

inline PinSource& pinSource() {
  static PinSource s_mux(gpioPins(), expanderPins());
  return s_mux;
}

using Board = zlkm::platform::BoardT<PinSource, PinDefs>;

}  // namespace zlkm::platform::boards::pico2
