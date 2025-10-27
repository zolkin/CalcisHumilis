#pragma once

#include <math.h>

#include "math/Constants.h"

namespace zlkm::dsp {

namespace fast_sin_cos_poly_constants {

constexpr float s0 = -0.10132104963779f;      // x
constexpr float s1 = 0.00662060857089096f;    // x^3
constexpr float s2 = -0.000173351320734045f;  // x^5
constexpr float s3 = 2.48668816803878e-06f;   // x^7
constexpr float s4 = -1.97103310997063e-08f;  // x^9

constexpr float c0 = -0.405284410277645f;     // 1
constexpr float c1 = 0.0383849982168558f;     // x^2
constexpr float c2 = -0.00132798793179218f;   // x^4
constexpr float c3 = 2.37446117208029e-05f;   // x^6
constexpr float c4 = -2.23984068352572e-07f;  // x^8

constexpr float pi = math::PI_F;
constexpr float halfPi = math::HALF_PI_F;

};  // namespace fast_sin_cos_poly_constants

// x in [-pi, pi]
// chebishev polynomial approximation error: 1.32e-6 near 0
// taken from https://www.apulsoft.ch/blog/branchless-sincos/
constexpr float fast_sin_poly(float x) {
  using namespace fast_sin_cos_poly_constants;
  auto x2 = x * x;
  auto x4 = x2 * x2;
  auto x8 = x4 * x4;
  auto poly1 = fma(x8, s4, fma(x4, fma(s3, x2, s2), fma(s1, x2, s0)));
  return (x - pi) * (x + pi) * x * poly1;
}

constexpr float fast_cos_poly(float x) {
  using namespace fast_sin_cos_poly_constants;
  auto x2 = x * x;
  auto x4 = x2 * x2;
  auto x8 = x4 * x4;
  auto poly2 = fma(x8, c4, fma(x4, fma(c3, x2, c2), fma(c1, x2, c0)));
  return (x - halfPi) * (x + halfPi) * poly2;
}

struct SinCosResult {
  float sin;
  float cos;
};

// useful for tangens
SinCosResult fast_sin_cos_poly(float x) {
  using namespace fast_sin_cos_poly_constants;

  auto x2 = x * x;
  auto x4 = x2 * x2;
  auto x8 = x4 * x4;

  auto poly1 = fma(x8, s4, fma(x4, fma(s3, x2, s2), fma(s1, x2, s0)));
  auto poly2 = fma(x8, c4, fma(x4, fma(c3, x2, c2), fma(c1, x2, c0)));

  return {(x - pi) * (x + pi) * x * poly1, (x - halfPi) * (x + halfPi) * poly2};
}

}  // namespace zlkm::dsp