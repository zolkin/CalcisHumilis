#pragma once
#include <math.h>

namespace blep {

struct Injector2TapX2 {
  float carry = 0.0f;  // next-sample tap

  inline float apply() {
    float v = carry;
    carry = 0.0f;
    return v;
  }

  // 'amp' = blend weight; internally scaled to delta = 2*amp (±2 step)
  // 'frac' = edge position within the *current* sample, in [0,1)
  inline float discontinuity(float frac, float amp) {
    const float x = frac;
    // classic quadratic polyBLEP (support: 2 samples, current + next)
    const float h0 = (1.0f - x) * (1.0f - x);  // this sample
    const float h1 = x * x;                    // next sample
    carry += 0.75f * amp * h1;
    return 0.75f * amp * h0;
  }
};

}  // namespace blep