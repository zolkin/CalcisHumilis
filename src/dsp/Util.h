#pragma once
#include <math.h>

#include "math/Constants.h"

namespace zlkm::dsp {

static inline float sin01_poly7(float t) {
  // Map directly to radians
  float x = math::TWO_PI_F * t;  // x ∈ [0, 2π)

  // Wrap to [-π, π] by subtracting 2π if needed (optional if you guarantee
  // t<1)
  if (x > 3.14159265f) x -= math::TWO_PI_F;

  // Now x ∈ [-π, π]. Reduce further to [-π/2, π/2] with a trig identity:
  float r = x;
  if (r > 1.57079633f) r = 3.14159265f - r;    // sin(x) = sin(π - x)
  if (r < -1.57079633f) r = -3.14159265f - r;  // sin(x) = sin(-π - x)

  // Estrin polynomial on r
  const float c2 = -1.6666666664e-01f;
  const float c4 = 8.3333154850e-03f;
  const float c6 = -1.9840782423e-04f;

  float r2 = r * r;
  float r4 = r2 * r2;

  float p2 = fmaf(c6, r2, c4);
  float p1 = fmaf(c2, r2, 1.0f);
  float p = fmaf(p2, r4, p1);

  return r * p;
}

constexpr float msToRate(const float ms, const float sr) {
  return 1.f / (sr * ms * .001f > 1.f ? sr * ms * .001f : 1.f);
}

constexpr float rateToMs(const float rt, const float sr) {
  return 1.f / rt / sr * 1000.f;
}

// Convenience: wrap a normalized phase "u" by an offset and keep it 0..1
inline float wrap01(float u) {
  u -= floorf(u);
  return u;
}

// Prewarp: Hz -> gCut  (g = tan(pi * f / SR))
template <int SR>
constexpr float hzToGCut(float hz) {
  float safeHz = (hz < 20.0f) ? 20.0f : hz;
  const float nyq = 0.5f * SR;
  const float maxHz = 0.45f * nyq * 2.0f;  // ~0.45*fs
  if (safeHz > maxHz) safeHz = maxHz;
  return tanf(float(M_PI) * safeHz / float(SR));
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

}  // namespace zlkm::dsp