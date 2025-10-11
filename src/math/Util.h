#pragma once

namespace zlkm::math {

template <class T>
constexpr T clamp(const T t, const T min, const T max) {
  return t < min ? min : t > max ? max : t;
}

inline float interpolate(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

static inline float rand01() {
  static uint32_t rng = 0x6d5fca4b;
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng >> 8) * (1.0f / 16777216.0f);
}

// Clamp to [0,1]
static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < 0.0f) return 0.0f;
  return x;
}

inline float smoothstep(float a, float b, float x) {
  float t = (x - a) / (b - a);
  if (t < 0.f) t = 0.f;
  if (t > 1.f) t = 1.f;
  return t * t * (3.f - 2.f * t);
}

}  // namespace zlkm::math