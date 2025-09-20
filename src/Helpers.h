#pragma once
#include <math.h>

#include "Constants.h"

namespace zlkm {

namespace arrays {
template <int N, typename T>
inline std::array<T, N> filled(T const& t) {
  std::array<T, N> res;
  res.fill(t);
  return res;
}
}  // namespace arrays

namespace pitch {

static inline float hzToPitch(float hz) { return log2f(hz); }  // log2 Hz
static inline float pitchToHz(float pit) { return exp2f(pit); }
static inline float semisToPitch(float s) { return s / 12.0f; }
static inline float pitchToSemis(float s) { return s * 12.0f; }

}  // namespace pitch

namespace dsp {

constexpr float msToRate(const float ms, const float sr) {
  return 1.f / (sr * ms * .001f > 1.f ? sr * ms * .001f : 1.f);
}

inline float polyblep(float t, float dt) {
  // t < dt
  if (t < dt) {
    float x = t / dt;
    return x + x - x * x - 1.0f;  // 2x - x^2 - 1
  }
  // t > 1 - dt
  if (t > 1.0f - dt) {
    float x = (t - 1.0f) / dt;
    return x * x + x + x + 1.0f;  // x^2 + 2x + 1
  }
  return 0.0f;
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
constexpr float res01ToKDamp(float res01, float Qmin = 0.5f,
                             float Qmax = 20.0f) {
  float r = (res01 < 0.0f) ? 0.0f : (res01 > 1.0f ? 1.0f : res01);
  const float Q = Qmin + r * (Qmax - Qmin);
  return 2.0f / Q;
}

}  // namespace dsp

namespace math {

template <class T>
constexpr T clamp(const T t, const T min, const T max) {
  return t < min ? min : t > max ? max : t;
}

inline float interpolate(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

static constexpr float INV_PI = 1.0f / PI;

template <int N>
constexpr std::array<int, N> fillRingIdx() {
  std::array<int, N> ring;
  for (int i = 0; i < N; ++i) {
    ring[i] = i % 2 ? (i + 1) / 2 : -i / 2;
  }
  return ring;
}

static inline float rand01() {
  static uint32_t rng = 0x6d5fca4b;
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng >> 8) * (1.0f / 16777216.0f);
}

}  // namespace math
}  // namespace zlkm