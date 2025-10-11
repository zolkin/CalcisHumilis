#pragma once
#include <stdint.h>

#include <array>

#include "platform/platform.h"

namespace zlkm::hw::ssaver {

template <typename G>  // needs drawPixel(int,int)
class StarField {
 public:
  struct Cfg {
    int width = 128;
    int height = 64;
    uint8_t starCount = 64;  // <= kMax
  };

  explicit StarField(const Cfg& cfg = {}) : cfg_(cfg) {
    // Initialize stars across the width
    for (uint8_t i = 0; i < activeCount(); ++i)
      respawn(i, (int16_t)random(0, width()));
    lastFrameMs_ = millis();
  }

  inline void step(uint32_t now, G& g) {
    if (activeCount() == 0 || width() <= 0 || height() <= 0) return;

    uint32_t dt = now - lastFrameMs_;
    if (dt == 0) return;
    if (dt > 100) dt = 100;
    lastFrameMs_ = now;

    for (uint8_t i = 0; i < activeCount(); ++i) {
      // dx ≈ speed * 0.05 px/ms * dt  → integer approx: (speed * dt * 5)/100
      int dx = int(stars_[i].speed * (int(dt) * 5) / 100);
      if (dx < 1) dx = 1;
      stars_[i].x -= dx;

      if (stars_[i].x < -2)
        respawn(i, width() + (int16_t)random(0, width() / 2));

      const int16_t x = stars_[i].x;
      const int16_t y = stars_[i].y;

      if ((uint16_t)y < (uint16_t)height()) {
        g.drawPixel(x, y);
        if (stars_[i].speed >= 3 && x + 1 < width()) g.drawPixel(x + 1, y);
      }
    }
  }

 private:
  struct Star {
    int16_t x;
    uint8_t y;
    uint8_t speed;
  };
  static constexpr uint8_t kMax = 96;

  inline int width() const { return cfg_.width; }
  inline int height() const { return cfg_.height; }
  inline uint8_t activeCount() const {
    return (cfg_.starCount <= kMax) ? cfg_.starCount : kMax;
  }

  inline void respawn(uint8_t i, int16_t xStart) {
    stars_[i].x = xStart;
    stars_[i].y = (uint8_t)random(0, (height() > 0 ? height() : 1));
    stars_[i].speed = (uint8_t)(1 + (random(0, 1000) % 3));  // 1..3
  }

  // --- Stored state (no copies of cfg values)
  Cfg cfg_{};
  std::array<Star, kMax> stars_{};
  uint32_t lastFrameMs_{0};
};

}  // namespace zlkm::hw::ssaver