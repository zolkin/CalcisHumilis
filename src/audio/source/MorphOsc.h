#pragma once
#include <math.h>

#include <array>

#include "dsp/Blep.h"
#include "dsp/SinCosPoly9.h"
#include "math/Util.h"
#include "platform/platform.h"

namespace zlkm::audio::src {

// -------- MORPH SINE->TRIANGLE->SQUARE->SAW --------
template <int N, int SR>
class MorphOscN {
 public:
  // Segment bounds
  static constexpr float SEGMENT_COUNT = 3.f;
  static constexpr float WAVE_COUNT = 4.f;
  static constexpr float SINE_BOUND = 0.0f;
  static constexpr float TRIANGLE_BOUND = 1.0f / SEGMENT_COUNT;
  static constexpr float SQUARE_BOUND = 2.0f / SEGMENT_COUNT;
  static constexpr float SAW_BOUND = 1.0f;

  static constexpr float INV_SINE_TRI_LEN = 1.f / (TRIANGLE_BOUND - SINE_BOUND);
  static constexpr float INV_TRI_SQ_LEN = 1.f / (SQUARE_BOUND - TRIANGLE_BOUND);
  static constexpr float INV_SQ_SAW_LEN = 1.f / (SAW_BOUND - SQUARE_BOUND);

  // For a ±1 triangle: slopes ±4 → slope jump magnitude 8
  static constexpr float TRI_JUMP = 8.0f;

  // per-sample normalized increment: dt = f / SR
  static constexpr float FREQ_TO_T = 1.0f / SR;

  struct alignas(16) State {
    float morph = 0.0f;          // 0..1
    float pulseWidth = 0.5f;     // 0..1
    dsp::Injector2TapX2 blep{};  // now dt in [0,1) and the result in +-1
    float cyclesPerSample = 0.0f;
    float phase = 0.0f;  // t in [0,1)
  };

  // --- Primitive naive generators (only when needed) ---
  static inline float sine_naive(float t0) {
    return dsp::fast_sin_poly(t0 * math::PI_F);
  }
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

  float tickMorph(int i) {
    State& s = state[i];

    // Start with shared carry, once per sample
    float sample = s.blep.apply();

    const float dt = s.cyclesPerSample;  // s.phaseInc;
    const float sum = s.phase + dt;
    const float overshoot = sum - 1.0f;
    const float pw = s.pulseWidth;

    const int seg = (int)(s.morph * SEGMENT_COUNT);

    switch (seg) {
      case 0: {
        const float wB = (s.morph - SINE_BOUND) * INV_SINE_TRI_LEN;
        const float wA = 1.0f - wB;
        sample += wA * sine_naive(s.phase) + wB * triangle_naive(s.phase);
      } break;
      case 1: {
        const float wB = (s.morph - TRIANGLE_BOUND) * INV_TRI_SQ_LEN;
        const float wA = 1.0f - wB;
        const float s_tri = (wA > 0.0f) ? triangle_naive(s.phase) : 0.0f;
        const float s_sq = square_blep(s, s.phase, dt, overshoot, pw, wB);
        sample += wA * s_tri + wB * s_sq;
      } break;
      default: {
        const float wB = (s.morph - SQUARE_BOUND) * INV_SQ_SAW_LEN;
        const float wA = 1.0f - wB;
        const float s_sq = square_blep(s, s.phase, dt, overshoot, pw, wA);
        const float s_saw = saw_blep(s, s.phase, dt, overshoot, wB);
        sample += wA * s_sq + wB * s_saw;
      } break;
    }
    s.phase = sum - (float)(overshoot > 0.f);
    return sample;
  }

  // useful to debug just the waveforms
  float ticSwitch(int i) {
    State& s = state[i];

    // Start with shared carry, once per sample
    float sample = s.blep.apply();

    const float dt = s.cyclesPerSample;  // s.phaseInc;
    const float sum = s.phase + dt;
    const float overshoot = sum - 1.0f;
    const float pw = s.pulseWidth;

    const int seg = (int)(s.morph * WAVE_COUNT);

    switch (seg) {
      case 0: {
        sample += sine_naive(s.phase);
      } break;
      case 1:
        sample += triangle_naive(s.phase);
        break;
      case 2:
        sample += square_blep(s, s.phase, dt, overshoot, pw, 1.f);
        break;
      default:
        sample += saw_blep(s, s.phase, dt, overshoot, 1.f);
        break;
    }
    s.phase = sum - (float)(overshoot > 0.f);
    return sample;
  }

  enum Mode { ModeMorph = 0, ModeSwitch };

  std::array<State, N> state = {};  // per-voice state

  Mode mode;

  // Call on trigger/note-on; latches pan & resets phase
  void reset(const bool randomPhase = false) {
    for (int i = 0; i < N; ++i) {
      state[i].phase = randomPhase ? math::rand01() : 0.0f;  // store t in [0,1)
    }
  }

  inline void tick(std::array<float, N>& out) {
    for (int i = 0; i < N; ++i) {
      switch (mode) {
        case ModeSwitch:
          out[i] = ticSwitch(i);
          break;
        default:
          out[i] = tickMorph(i);
          break;
      }
    }
  }
};

}  // namespace zlkm::audio::src