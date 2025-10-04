#pragma once

namespace zlkm::math {

template <class T>
constexpr T clamp(const T t, const T min, const T max) {
  return t < min ? min : t > max ? max : t;
}

inline float interpolate(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

// Catmull–Rom cubic interpolation (uniform, tension=0)
static inline float lerp_cubic(const float y0, const float y1, const float y2,
                               const float y3, float a) {
  const float a2 = a * a;
  const float a3 = a2 * a;
  return 0.5f * ((2.0f * y1) + (-y0 + y2) * a +
                 (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * a2 +
                 (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * a3);
}

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

static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x;
}

inline float smoothstep(float a, float b, float x) {
  float t = (x - a) / (b - a);
  if (t < 0.f) t = 0.f;
  if (t > 1.f) t = 1.f;
  return t * t * (3.f - 2.f * t);
}

}  // namespace zlkm::math