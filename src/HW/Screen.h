#pragma once
#include <U8g2lib.h>
#include <stdint.h>

#include "platform/boards/Current.h"
#include "platform/platform.h"

namespace zlkm::hw {

using PinDefs = typename ::zlkm::platform::boards::current::PinDefs;

// Pick your controller at compile time:
enum class ScreenController : uint8_t { SSD1306_128x64, SH1107_64x128 };

// Map controller -> U8g2 driver type
template <ScreenController C>
struct U8g2Driver;

template <>
struct U8g2Driver<ScreenController::SSD1306_128x64> {
  using type = U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI;
};

template <>
struct U8g2Driver<ScreenController::SH1107_64x128> {
  using type = U8G2_SH1107_64X128_F_4W_HW_SPI;
};

template <ScreenController C>
class Screen {
 public:
  struct Cfg {
  ::zlkm::hw::io::PinId pinSCK = PinDefs::OLED_SCK.pin();
  ::zlkm::hw::io::PinId pinMOSI = PinDefs::OLED_MOSI.pin();
  ::zlkm::hw::io::PinId pinDC = PinDefs::OLED_DC.pin();
  ::zlkm::hw::io::PinId pinRST = PinDefs::OLED_RST.pin();

    // SPI & display
    uint32_t spiHz = 8'000'000;           // use u8g2.setBusClock()
    const u8g2_cb_t* rotation = U8G2_R0;  // landscape
    const uint8_t* font = u8g2_font_6x12_tf;
  };

  explicit Screen(const Cfg& cfg)
      : cfg_(cfg),
        u8g2_(cfg.rotation, U8X8_PIN_NONE, (uint8_t)cfg.pinDC.value,
              (uint8_t)cfg.pinRST.value)  // CS tied to GND
  {
    // SPI0 on your pins (MISO not used)
    SPI.setSCK((uint8_t)cfg_.pinSCK.value);
    SPI.setTX((uint8_t)cfg_.pinMOSI.value);
    SPI.begin();

    u8g2_.begin();
    u8g2_.setBusClock(cfg_.spiHz);
    u8g2_.setPowerSave(0);
    u8g2_.clearBuffer();
    u8g2_.setFont(cfg_.font);
    u8g2_.drawStr(0, 12, "Screen ready");
    u8g2_.sendBuffer();
  }

  template <typename Render>
  inline void update(Render&& render) {
    u8g2_.clearBuffer();
    render(u8g2_);
    u8g2_.sendBuffer();
  }

  inline void updateLines(const char* l1, const char* l2 = nullptr) {
    u8g2_.clearBuffer();
    u8g2_.setFont(cfg_.font);
    if (l1) u8g2_.drawStr(0, 12, l1);
    if (l2) u8g2_.drawStr(0, 28, l2);
    u8g2_.drawFrame(0, 0, width(), height());
    u8g2_.sendBuffer();
  }

  inline int width() const { return u8g2_.getDisplayWidth(); }
  inline int height() const { return u8g2_.getDisplayHeight(); }
  inline U8G2& g() { return u8g2_; }

 private:
  using DriverT = typename U8g2Driver<C>::type;
  Cfg cfg_;
  mutable DriverT u8g2_;
};

}  // namespace zlkm::hw