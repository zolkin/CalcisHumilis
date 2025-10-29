#pragma once
#include "math/Util.h"

namespace zlkm::audio::proc {

// Simple post-filter drive/limiter FX
struct DriveFX {
  static inline float process(float x, float drive) {
    const float y = x * drive;
    return y / (1.0f + fabsf(y));
  }
};

}  // namespace zlkm::audio::proc
