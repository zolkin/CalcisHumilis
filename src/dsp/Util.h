#pragma once
#include <math.h>
#include "math/Constants.h"

namespace zlkm::dsp {

constexpr float msToRate(const float ms, const float sr) {
  return 1.f / (sr * ms * .001f > 1.f ? sr * ms * .001f : 1.f);
}

constexpr float rateToMs(const float rt, const float sr) {
  return 1.f / rt / sr * 1000.f;
}

constexpr float nyqFromSR(const float sr) { return 0.5f * sr; }

// Prewarp: Hz -> gCut  (g = tan(pi * f / SR))
template <int SR>
constexpr float hzToGCut(float hz) {
  constexpr float nyq = nyqFromSR(SR);
  constexpr float maxHz = 0.45f * nyq * 2.0f;  // ~0.45*fs
  if (hz > maxHz) hz = maxHz;
  return tanf(float(zlkm::math::PI_F) * hz / float(SR));
}

// Map UI resonance [0..1] -> Q in [Qmin..Qmax] -> kDamp = 2/Q
constexpr float res01ToKDamp_smooth(float res01, float Qmin = 0.707f,
                                    float Qmax = 10.0f, float curve = 2.0f) {
  // perceptual curve
  const float r = (res01 < 0.f) ? 0.f : (res01 > 1.f ? 1.f : res01);
  const float t = powf(r, curve);
  const float Q = Qmin * powf(Qmax / Qmin, t);
  return 2.0f / Q;  // k = 2/Q
}

constexpr float res01ToKDamp_fast(float res01) {
  // perceptual curve
  const float t = res01 * res01;
  // assuming default limits: QMin = 0.707, QMax = 16 * QMin
  const float Q = 0.707f * std::exp2(t * 4.0f);
  return 2.0f / Q;  // k = 2/Q
}

}  // namespace zlkm::dsp