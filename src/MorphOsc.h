#pragma once
#include <Arduino.h>
#include <math.h>

#include <array>

#include "dsp/Blep.h"
#include "dsp/Dsp.h"
#include "math/Math.h"

namespace zlkm {

// -------- MORPH SINE->TRIANGLE->SQUARE->SAW --------
template <int N, int SR>
class MorphOscN {
  // Segment bounds
  static constexpr float SINE_BOUND = 0.0f;
  static constexpr float TRIANGLE_BOUND = 1.0f / 3.0f;
  static constexpr float SQUARE_BOUND = 2.0f / 3.0f;
  static constexpr float SAW_BOUND = 1.0f;

  static constexpr float INV_SINE_TRI_LEN = 1.f / (TRIANGLE_BOUND - SINE_BOUND);
  static constexpr float INV_TRI_SQ_LEN = 1.f / (SQUARE_BOUND - TRIANGLE_BOUND);
  static constexpr float INV_SQ_SAW_LEN = 1.f / (SAW_BOUND - SQUARE_BOUND);

  // For a ±1 triangle: slopes ±4 → slope jump magnitude 8
  static constexpr float TRI_JUMP = 8.0f;

  // per-sample normalized increment: dt = f / SR
  static constexpr float FREQ_TO_T = 1.0f / SR;

  struct alignas(16) State {
    float morph = 0.0f;           // 0..1
    float pulseWidth = 0.5f;      // 0..1
    blep::Injector2TapX2 blep{};  // now dt in [0,1) and the result in +-1
    float cyclesPerSample = 0.0f;
    float phase = 0.0f;  // t in [0,1)
  };

  // --- Primitive naive generators (only when needed) ---
  static inline float sine_naive(float t0) { return dsp::sin01_poly7(t0); }
  static inline float triangle_naive(float t0) {
    // 1 - 4*|t - 0.5|
    return 1.0f - 4.0f * fabsf(t0 - 0.5f);
  }
  static inline float square_naive(float t0, float pw) {
    //  +1 if t0<pw else -1  →  1 - 2*(t0>=pw)
    return 1.0f - 2.0f * (float)(t0 >= pw);
  }
  static inline float saw_naive(float t0) { return 2.0f * t0 - 1.0f; }

  inline float square_blep(State& s, float t0, float dt, float overshoot,
                           float pw, float amp) {
    if (amp <= 0.f) {
      return 0.f;
    }
    float s_sq = square_naive(t0, pw);
    [[likely]] if (overshoot <= 0.f) {
      [[likely]] if (t0 >= pw || (t0 + dt) < pw) { return s_sq; }
      const float frac = (pw - t0) / dt;
      s_sq += s.blep.discontinuity(frac, -amp);  // +1 -> -1
      return s_sq;
    }
    const float frac_rise = 1.0f - (overshoot / dt);
    s_sq += s.blep.discontinuity(frac_rise, amp);  // -1 -> +1

    [[likely]] if (overshoot <= pw) { return s_sq; }
    const float frac_fall = ((1.0f - t0) + pw) / dt;
    s_sq += s.blep.discontinuity(frac_fall, -amp);  // +1 -> -1
    return s_sq;
  }

  inline float saw_blep(State& s, float t0, float dt, float overshoot,
                        float amp) {
    if (amp <= 0.f) {
      return 0.f;
    }
    float saw = saw_naive(t0);
    [[likely]] if (overshoot <= 0)
      return saw;
    const float frac = 1.0f - (overshoot / dt);
    saw += s.blep.discontinuity(frac, amp);  // +1 -> -1 at wrap
    return saw;
  }

 public:
  std::array<State, N> state = {};  // per-voice state

  // Call on trigger/note-on; latches pan & resets phase
  void reset(const bool randomPhase = false) {
    for (int i = 0; i < N; ++i) {
      state[i].phase = randomPhase ? math::rand01() : 0.0f;  // store t in [0,1)
    }
  }

  inline void tick(std::array<float, N>& out) {
    for (int i = 0; i < N; ++i) {
      State& s = state[i];
      const float t0 = s.phase;
      const float dt = s.cyclesPerSample;  // s.phaseInc;
      const float sum = t0 + dt;
      const float overshoot = sum - 1.0f;
      const float pw = s.pulseWidth;
      const float m = s.morph;

      // Start with shared carry, once per sample
      float y = s.blep.apply();
      int seg = (int)(s.morph * 3.0f);

      switch (seg) {
        case 0: {
          const float wB = (m - SINE_BOUND) * INV_SINE_TRI_LEN;
          const float wA = 1.0f - wB;
          y += wA * sine_naive(t0) + wB * triangle_naive(t0);
        } break;
        case 1: {
          const float wB = (m - TRIANGLE_BOUND) * INV_TRI_SQ_LEN;
          const float wA = 1.0f - wB;
          const float s_tri = (wA > 0.0f) ? triangle_naive(t0) : 0.0f;
          const float s_sq = square_blep(s, t0, dt, overshoot, pw, wB);
          y += wA * s_tri + wB * s_sq;
        } break;
        default: {
          const float wB = (m - SQUARE_BOUND) * INV_SQ_SAW_LEN;
          const float wA = 1.0f - wB;
          const float s_sq = square_blep(s, t0, dt, overshoot, pw, wA);
          const float s_saw = saw_blep(s, t0, dt, overshoot, wB);
          y += wA * s_sq + wB * s_saw;
        }
      }

      out[i] = y;

      const float mask = (float)(overshoot > 0.f);  // 0.0 or 1.0
      s.phase = sum - mask;
      ;
    }
  }
};

}  // namespace zlkm