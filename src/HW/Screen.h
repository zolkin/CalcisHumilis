#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <stdint.h>

#include <array>

namespace zlkm::hw {

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
  // ---- Configuration (no heap, sane defaults)
  struct Cfg {
    // Pins (your wiring: GP6/7/9/8; CS is hard-wired to GND)
    uint8_t pinSCK = 6;   // GP6  -> SCL (SPI0 SCK)
    uint8_t pinMOSI = 7;  // GP7  -> SDA (SPI0 MOSI)
    uint8_t pinDC = 9;    // GP9  -> DC
    uint8_t pinRST = 8;   // GP8  -> RES

    // SPI & display
    uint32_t spiHz = 8'000'000;  // use u8g2.setBusClock()
    const u8g2_cb_t* rotation =
        U8G2_R0;  // For SH1107 use U8G2_R90 for 128x64 landscape
    const uint8_t* font = u8g2_font_6x12_tf;

    // Idle / screensaver
    bool screensaverStarfield = true;
    uint32_t idleTimeoutMs = 60'000;  // 1 minute
    uint8_t starCount = 64;           // <= kMaxStars
  };

  explicit Screen(const Cfg& cfg)
      : cfg_(cfg),
        u8g2_(cfg.rotation, U8X8_PIN_NONE, cfg.pinDC,
              cfg.pinRST)  // CS is tied to GND
  {
    // SPI0 on your pins (MISO not used)
    SPI.setSCK(cfg_.pinSCK);
    SPI.setTX(cfg_.pinMOSI);
    SPI.begin();

    u8g2_.begin();
    u8g2_.setBusClock(cfg_.spiHz);
    u8g2_.setPowerSave(0);
    u8g2_.clearBuffer();
    u8g2_.setFont(cfg_.font);
    u8g2_.drawStr(0, 12, "Screen ready");
    u8g2_.sendBuffer();

    lastActivityMs_ = millis();
    lastFrameMs_ = lastActivityMs_;
    initStars();
  }

  // Tell the screen something changed (encoder turn, button press, CV/gate
  // change, etc.)
  inline void noteActivity() { lastActivityMs_ = millis(); }

  // Zero-allocation render: templated callable (accepts lambdas/functors, no
  // std::function)
  template <typename Render>
  inline void update(Render&& render) {
    const uint32_t now = millis();
    const bool idle = cfg_.screensaverStarfield &&
                      (now - lastActivityMs_ >= cfg_.idleTimeoutMs);

    u8g2_.clearBuffer();
    if (idle)
      drawStarfieldStep(now);
    else
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
  inline U8G2& g() { return this->u8g2_; }

 private:
  // --- Starfield (fixed array, no heap) ---
  struct Star {
    int16_t x;
    uint8_t y;
    uint8_t speed;
  };
  static constexpr uint8_t kMaxStars = 96;
  std::array<Star, kMaxStars> stars_{};
  uint8_t activeStars_ = 0;

  void initStars() {
    activeStars_ = (cfg_.starCount <= kMaxStars) ? cfg_.starCount : kMaxStars;
    const int w = width();
    for (uint8_t i = 0; i < activeStars_; ++i) respawnStarAt(i, random(0, w));
  }

  void respawnStarAt(uint8_t i, int16_t xStart) {
    stars_[i].x = xStart;
    stars_[i].y = static_cast<uint8_t>(random(0, height()));
    stars_[i].speed = static_cast<uint8_t>(1 + (random(0, 1000) % 3));  // 1..3
  }

  void drawStarfieldStep(uint32_t now) {
    uint32_t dt = now - lastFrameMs_;
    if (dt > 100) dt = 100;
    lastFrameMs_ = now;
    const int w = width();
    for (uint8_t i = 0; i < activeStars_; ++i) {
      // dx ≈ speed * 0.05 px/ms * dt  (integer approx: (speed * dt * 5)/100)
      int dx = int(stars_[i].speed * (int(dt) * 5) / 100);
      if (dx < 1) dx = 1;
      stars_[i].x -= dx;
      if (stars_[i].x < -2) respawnStarAt(i, w + random(0, w / 2));
      u8g2_.drawPixel(stars_[i].x, stars_[i].y);
      if (stars_[i].speed >= 3 && stars_[i].x + 1 < w)
        u8g2_.drawPixel(stars_[i].x + 1, stars_[i].y);
    }
  }

 private:
  using DriverT = typename U8g2Driver<C>::type;
  Cfg cfg_;
  mutable DriverT u8g2_;

  // Idle timing
  uint32_t lastActivityMs_{0};
  uint32_t lastFrameMs_{0};
};

} // namespace zlkm::hw