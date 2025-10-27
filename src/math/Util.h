#pragma once

#undef min
#undef max

namespace zlkm::math {

template <class T>
constexpr T clamp(const T t, const T min, const T max) {
  return t < min ? min : t > max ? max : t;
}

// Arm Cortex-M33 optimized version
// https://developer.arm.com/documentation/100235/0003/the-cortex-m33-instruction-set/cortex-m33-instructions
constexpr float clamp(float t, float min, float max) {
  return fmin(fmax(t, min), max);
}

constexpr float interpolate(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

inline float rand01() {
  static uint32_t rng = 0x6d5fca4b;
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng >> 8) * (1.0f / 16777216.0f);
}

// Clamp to [0,1]
constexpr float clamp01(float x) { return clamp(x, 0.0f, 1.0f); }

constexpr float smoothstep(float a, float b, float x) {
  float t = clamp01((x - a) / (b - a));
  return t * t * (3.f - 2.f * t);
}

}  // namespace zlkm::math