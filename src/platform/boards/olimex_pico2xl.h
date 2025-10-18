#pragma once

// Board wiring and PinSource composition for Olimex Pico 2-XL
#include <Wire.h>

#include <tuple>

#include "hw/io/GpioPins.h"
#include "hw/io/PinMux.h"

namespace zlkm::platform::boards::olimex_pico2xl {

// Compose a pin source for the board. Only on-board GPIOs are used
// via GpioPins and muxed by PinMux with a single device (transparent mode).
struct Board {
  static constexpr int GPIO_PIN_COUNT = 60;
  using PinId = zlkm::hw::io::PinId;  // low-level raw pin id
  using GpioPins = zlkm::hw::io::GpioPins<GPIO_PIN_COUNT>;
  using PinSource = zlkm::hw::io::PinMux<uint8_t, GpioPins>;  // single device
  using SrcPinId = typename PinSource::PinIdT;
  using PinGroupId = zlkm::hw::io::PinGroupId;
  template <size_t N>
  using GroupPinArray = PinSource::GroupPinArrayT<N>;

  static constexpr PinGroupId GROUP_GPIO = PinGroupId{0};  // only group

  template <size_t N>
  using SrcPinArray = std::array<SrcPinId, N>;

  // Assign a sensible default pin layout; adjust as needed later.
  // I2C wiring (default to GPIO20/21 similar to Pico 2)
  inline static const SrcPinId I2C_SDA{20};
  inline static const SrcPinId I2C_SCL{21};

  // Trigger and status LEDs
  inline static const SrcPinId TRIG_IN{26};
  inline static const SrcPinId LED_TRIGGER{27};
  inline static const SrcPinId LED_CLIPPING{28};

  // OLED/SPI wiring (SPI0): SCK/MOSI/DC/RST
  inline static const SrcPinId OLED_SCK{6};   // SCK
  inline static const SrcPinId OLED_MOSI{7};  // TX
  inline static const SrcPinId OLED_DC{9};    // D/C
  inline static const SrcPinId OLED_RST{8};   // RESET

  // Front-panel IO as plain GPIO groups (group 0)
  inline static const GroupPinArray<4> LEDS{PinId{0}, PinId{1}, PinId{2},
                                            PinId{3}};
  inline static const GroupPinArray<4> TAB_BUTTONS{PinId{4}, PinId{5}, PinId{6},
                                                   PinId{7}};
  inline static const GroupPinArray<8> ENCODER{PinId{8},  PinId{9},  PinId{10},
                                               PinId{11}, PinId{12}, PinId{13},
                                               PinId{14}, PinId{15}};

  static inline GpioPins& gpioPins() {
    using namespace zlkm::hw::io;
    static GpioPins s_gpio(PinMode::Input);
    return s_gpio;
  }

  static inline PinSource& pins() {
    static PinSource s_mux(gpioPins());
    return s_mux;
  }
};

}  // namespace zlkm::platform::boards::olimex_pico2xl
