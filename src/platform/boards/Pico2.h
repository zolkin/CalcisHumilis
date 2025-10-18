#pragma once

// Board wiring and PinSource composition for Raspberry Pi Pico 2
#include <Wire.h>

#include <tuple>

#include "hw/io/GpioPins.h"
#include "hw/io/McpPins.h"
#include "hw/io/PinMux.h"

namespace zlkm::platform::boards::pico2 {

// Current board: Pico 2
// Compose a pin source for the board. On-board GPIOs are handled by GpioPins
// and the MCP23017 expander by McpPins, muxed by PinMux.

// Concrete typedefs for readability
struct Board {
  static constexpr int GPIO_PIN_COUNT = 30;
  using GpioPins =
      zlkm::hw::io::GpioPins<GPIO_PIN_COUNT>;     // identity map 0..29 to GPIO
  using ExpMcpPins = zlkm::hw::io::Mcp23017Pins;  // 16-pin expander
  using PinSource = zlkm::hw::io::PinMux<uint8_t, GpioPins, ExpMcpPins>;
  using SrcPinId = typename PinSource::PinIdT;
  using PinId = zlkm::hw::io::PinId;  // low-level raw pin id
  using PinGroupId = zlkm::hw::io::PinGroupId;

  template <size_t N>
  using GroupPinArray = PinSource::GroupPinArrayT<N>;

  static constexpr PinGroupId GROUP_GPIO = PinGroupId{0};  // default group
  static constexpr PinGroupId GROUP_EXPANDER = PinGroupId{1};

  template <size_t N>
  using SrcPinArray = std::array<SrcPinId, N>;

  // MCU GPIOs
  inline static const SrcPinId I2C_SDA{20};
  inline static const SrcPinId I2C_SCL{21};
  inline static const SrcPinId TRIG_IN{26};
  inline static const SrcPinId LED_TRIGGER{27};
  inline static const SrcPinId LED_CLIPPING{28};

  // OLED/SPI wiring (SPI0)
  inline static const SrcPinId OLED_SCK{6};   // SCK
  inline static const SrcPinId OLED_MOSI{7};  // TX
  inline static const SrcPinId OLED_DC{9};    // D/C
  inline static const SrcPinId OLED_RST{8};   // RESET

  // MCP23017 expander pins (0..15)
  inline static const GroupPinArray<4> LEDS{GROUP_EXPANDER, {0, 1, 2, 3}};
  inline static const GroupPinArray<4> TAB_BUTTONS{GROUP_EXPANDER,
                                                   {4, 5, 6, 7}};
  inline static const GroupPinArray<8> ENCODER{GROUP_EXPANDER,
                                               {8, 9, 10, 11, 12, 13, 14, 15}};

  static inline GpioPins& gpioPins() {
    using namespace zlkm::hw::io;
    static GpioPins s_gpio(PinMode::Input);
    return s_gpio;
  }

  static inline ExpMcpPins& expanderPins() {
    using namespace zlkm::hw::io;
    static ExpMcpPins s_exp(I2cCfg{
        .address = 0x20,
        .wire = &Wire,
        .clockHz = 0,
        .i2cSDA = I2C_SDA.pin(),
        .i2cSCL = I2C_SCL.pin(),
    });
    return s_exp;
  }

  static inline PinSource& pins() {
    static PinSource s_mux(gpioPins(), expanderPins());
    return s_mux;
  }
};

}  // namespace zlkm::platform::boards::pico2
