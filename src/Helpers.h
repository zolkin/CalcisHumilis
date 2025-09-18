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

}  // namespace dsp

namespace math {

template <class T>
inline T clamp(T t, T min, T max) {
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