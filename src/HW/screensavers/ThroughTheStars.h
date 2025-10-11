#pragma once
#include <math.h>  // only for sinf/cosf in respawn
#include <stdint.h>

#include <array>

#include "platform/platform.h"

namespace zlkm::hw::ssaver {

template <typename G>
class ThroughTheStars {
 public:
  struct Cfg {
    // Display
    int width = 128;
    int height = 64;

    // Stars
    uint8_t starCount = 72;       // <= kMax
    uint16_t minSpawnRadius = 2;  // px
    uint16_t maxSpawnRadius = 8;  // px

    // Velocity (8.8 fixed px/ms)
    uint16_t vMin_8p8 = 10;  // ~0.04 px/ms
    uint16_t vMax_8p8 = 60;  // ~0.23 px/ms

    // Center drift
    uint16_t driftEveryMsMin = 900;
    uint16_t driftEveryMsMax = 2400;
    uint8_t maxDriftPX = 12;
    uint8_t centerEaseDiv = 16;

    // Occasional twitch
    uint16_t twitchEveryMsMin = 2500;
    uint16_t twitchEveryMsMax = 6000;
    uint8_t twitchPX = 3;
    uint16_t twitchDurationMs = 100;
  };

  explicit ThroughTheStars(const Cfg& cfg = {}) : cfg_(cfg) {
    // Initialize stars around current center (derived from cfg)
    for (uint8_t i = 0; i < activeCount(); ++i) respawn(i);
    const uint32_t now = millis();
    lastStepMs_ = now;
    scheduleNextDrift(now);
    scheduleNextTwitch(now);
  }

  inline void step(uint32_t now, G& g) {
    if (width() <= 0 || height() <= 0 || activeCount() == 0) return;

    uint32_t dt = now - lastStepMs_;
    if (dt == 0) return;
    if (dt > 100) dt = 100;
    lastStepMs_ = now;

    // Lazy-init centers if zero (covers default-constructed state nicely)
    if (!centersInited_) {
      cx_ = width() / 2;
      cy_ = height() / 2;
      tx_ = cx_;
      ty_ = cy_;
      trueCx_ = cx_;
      trueCy_ = cy_;
      centersInited_ = true;
    }

    // Eased drift
    cx_ += (int16_t)((int32_t)(tx_ - cx_) / cfg_.centerEaseDiv);
    cy_ += (int16_t)((int32_t)(ty_ - cy_) / cfg_.centerEaseDiv);

    if (now >= nextDriftMs_) {
      pickNewTargetCenter();
      scheduleNextDrift(now);
    }

    // Twitch (brief jitter)
    int16_t twX = 0, twY = 0;
    if (now >= nextTwitchMs_ && now < nextTwitchMs_ + cfg_.twitchDurationMs) {
      twX = (int16_t)random(-cfg_.twitchPX, cfg_.twitchPX + 1);
      twY = (int16_t)random(-cfg_.twitchPX, cfg_.twitchPX + 1);
    } else if (now >= nextTwitchMs_ + cfg_.twitchDurationMs) {
      scheduleNextTwitch(now);
    }

    const int16_t cxDraw = (int16_t)cx_ + twX;
    const int16_t cyDraw = (int16_t)cy_ + twY;
    const int16_t margin = 2;

    for (uint8_t i = 0; i < activeCount(); ++i) {
      stars_[i].x_8p8 += (int32_t)stars_[i].vx_8p8 * (int32_t)dt;
      stars_[i].y_8p8 += (int32_t)stars_[i].vy_8p8 * (int32_t)dt;

      const int16_t x = (int16_t)(stars_[i].x_8p8 >> 8);
      const int16_t y = (int16_t)(stars_[i].y_8p8 >> 8);

      if ((uint16_t)x < (uint16_t)width() && (uint16_t)y < (uint16_t)height()) {
        g.drawPixel(x, y);
        if (stars_[i].streak2 && (x + 1) < width()) g.drawPixel(x + 1, y);
      }

      if (x < -margin || y < -margin || x >= (width() + margin) ||
          y >= (height() + margin)) {
        respawn(i, cxDraw, cyDraw);
      }
    }
  }

 private:
  struct Star {
    int32_t x_8p8, y_8p8;
    int16_t vx_8p8, vy_8p8;
    bool streak2;
  };

  static constexpr uint8_t kMax = 96;

  inline int width() const { return cfg_.width; }
  inline int height() const { return cfg_.height; }
  inline uint8_t activeCount() const {
    return (cfg_.starCount <= kMax) ? cfg_.starCount : kMax;
  }

  inline void respawn(uint8_t i, int16_t cxSpawn, int16_t cySpawn) {
    const uint16_t rMin = cfg_.minSpawnRadius;
    const uint16_t rMax =
        (cfg_.maxSpawnRadius > rMin) ? cfg_.maxSpawnRadius : (rMin + 1);
    const float r = (float)random(rMin, rMax + 1);
    const float a = (float)random(0, 6283) * 0.001f;  // ~[0, 2π)
    const float sx = cosf(a), sy = sinf(a);

    const int16_t x0 = (int16_t)lrintf(cxSpawn + sx * r);
    const int16_t y0 = (int16_t)lrintf(cySpawn + sy * r);

    const uint16_t v = (uint16_t)random(cfg_.vMin_8p8, cfg_.vMax_8p8 + 1);
    stars_[i].x_8p8 = ((int32_t)x0) << 8;
    stars_[i].y_8p8 = ((int32_t)y0) << 8;
    stars_[i].vx_8p8 = (int16_t)lrintf(sx * (float)v);
    stars_[i].vy_8p8 = (int16_t)lrintf(sy * (float)v);
    stars_[i].streak2 = (v >= ((cfg_.vMin_8p8 + cfg_.vMax_8p8) / 2));
  }

  inline void respawn(uint8_t i) { respawn(i, width() / 2, height() / 2); }

  inline void pickNewTargetCenter() {
    trueCx_ = width() / 2;
    trueCy_ = height() / 2;
    const int dx = (int)random(-cfg_.maxDriftPX, cfg_.maxDriftPX + 1);
    const int dy = (int)random(-cfg_.maxDriftPX, cfg_.maxDriftPX + 1);
    tx_ = (int16_t)(trueCx_ + dx);
    ty_ = (int16_t)(trueCy_ + dy);
  }

  inline void scheduleNextDrift(uint32_t now) {
    nextDriftMs_ =
        now + (uint32_t)random(cfg_.driftEveryMsMin, cfg_.driftEveryMsMax + 1);
  }

  inline void scheduleNextTwitch(uint32_t now) {
    nextTwitchMs_ = now + (uint32_t)random(cfg_.twitchEveryMsMin,
                                           cfg_.twitchEveryMsMax + 1);
  }

  // --- Stored state (no copies of cfg values)
  Cfg cfg_{};
  std::array<Star, kMax> stars_{};
  bool centersInited_{false};

  int16_t cx_{0}, cy_{0};
  int16_t tx_{0}, ty_{0};
  int16_t trueCx_{0}, trueCy_{0};

  uint32_t lastStepMs_{0};
  uint32_t nextDriftMs_{0};
  uint32_t nextTwitchMs_{0};
};

}  // namespace zlkm::hw::ssaver