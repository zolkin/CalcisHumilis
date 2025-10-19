#pragma once

// Board wiring and PinSource composition for Olimex Pico 2-XL
#include <Wire.h>
#include <stdint.h>

#include <tuple>

#include "hw/ScreenTypes.h"
#include "hw/io/GpioPins.h"
#include "hw/io/PinMux.h"

namespace zlkm::platform::boards::olimex_pico2xl {

// Compose a pin source for the board. Only on-board GPIOs are used
// via GpioPins and muxed by PinMux with a single device (transparent mode).
struct Board {
  // Platform-level selection of OLED controller used on this board
  using ScreenController = zlkm::hw::ScreenController;
  inline static constexpr auto SCREEN_CTRL = ScreenController::SSD1309_128x64;

  static constexpr size_t GPIO_PIN_COUNT = 48;
  using PinId = zlkm::hw::io::PinId;  // low-level raw pin id
  using GpioPins = zlkm::hw::io::GpioPins<GPIO_PIN_COUNT>;
  using PinSource = zlkm::hw::io::PinMux<uint8_t, GpioPins>;  // single device
  using SrcPinId = typename PinSource::PinIdT;
  using PinGroupId = zlkm::hw::io::PinGroupId;
  template <size_t N>
  using GroupPinArray = PinSource::GroupPinArrayT<N>;

  // Assign a sensible default pin layout; adjust as needed later.
  // I2C wiring (default to GPIO20/21 similar to Pico 2)
  inline static constexpr SrcPinId I2C_SDA{20};
  inline static constexpr SrcPinId I2C_SCL{21};

  // Trigger and status LEDs
  inline static constexpr SrcPinId TRIG_IN{13};
  inline static constexpr SrcPinId LED_TRIGGER{14};
  inline static constexpr SrcPinId LED_CLIPPING{15};

  inline static const SrcPinId PIN_BCK{10};
  inline static const SrcPinId PIN_LRCK{11};
  inline static const SrcPinId PIN_DATA{12};

  // SPI0 wiring on Olimex Pico2-XL header
  // RX(MISO)=GPIO4, CSn=GPIO5, SCK=GPIO6, TX(MOSI)=GPIO7
  inline static constexpr SrcPinId SPI0_MISO{4};
  inline static constexpr SrcPinId SPI0_CS{5};
  inline static constexpr SrcPinId SPI0_SCK{6};
  inline static constexpr SrcPinId SPI0_MOSI{7};

  // OLED pins mapped to SPI0 and control lines
  inline static constexpr SrcPinId OLED_CS{SPI0_CS};
  inline static constexpr SrcPinId OLED_SCK{SPI0_SCK};
  inline static constexpr SrcPinId OLED_MOSI{SPI0_MOSI};
  inline static constexpr SrcPinId OLED_DC{32};   // D/C on a free GPIO
  inline static constexpr SrcPinId OLED_RST{33};  // RESET

  // Front-panel IO mapped to avoid reserved/used ranges
  // Avoid: GPIO0-3,8-11,24-25,40-47 and OLED_DC/OLED_RST at 32/33
  inline static constexpr GroupPinArray<4> LEDS{PinId{26}, PinId{27}, PinId{28},
                                                PinId{29}};
  inline static constexpr GroupPinArray<4> TAB_BUTTONS{PinId{34}, PinId{35},
                                                       PinId{36}, PinId{37}};
  inline static constexpr GroupPinArray<8> ENCODER{
      PinId{16}, PinId{17}, PinId{18}, PinId{19},
      PinId{20}, PinId{21}, PinId{22}, PinId{23}};

  static inline GpioPins& gpioPins() {
    static GpioPins s_gpio(zlkm::hw::io::PinMode::Input);
    return s_gpio;
  }

  static inline PinSource& pins() {
    static PinSource s_mux(gpioPins());
    return s_mux;
  }
};

}  // namespace zlkm::platform::boards::olimex_pico2xl
