#pragma once
#include <Arduino.h>
#include <math.h>

#include <array>

#include "Helpers.h"  // expects pitch::hzToPitch / pitch::pitchToHz / pitch::semisToPitch

namespace zlkm {

template <int N, int SR>
class BaseOscillatorN {
  // per-sample normalized increment: dt = f / SR
  static constexpr float FREQ_TO_T = 1.0f / SR;

 public:
  // Call on trigger/note-on; latches pan & resets phase
  void reset(const bool randomPhase = false) {
    for (int i = 0; i < N; ++i) {
      phase[i] = randomPhase ? math::rand01() : 0.0f;  // store t in [0,1)
      phaseInc[i] = 0.0f;                              // dt
    }
  }

  std::array<float, N> cyclesPerSample = {0.0f};
  std::array<float, N> phase = {0.0f};     // now t in [0,1)
  std::array<float, N> phaseInc = {0.0f};  // now dt in [0,1)

 protected:
  inline void smoothPhaseIncTowards(const float /*targetInc*/) {
    // set smoothingAlpha_ to 1.0f if you want immediate tracking
  }

  inline void advancePhase() {
    for (int i = 0; i < N; ++i) {
      // track dt towards target f/SR (keep your SMOOTHING_FACTOR as-is)
      phaseInc[i] += (cyclesPerSample[i] - phaseInc[i]) * SMOOTHING_FACTOR;

      // advance normalized phase and wrap with a cheap branch
      phase[i] += phaseInc[i];
      if (phase[i] >= 1.0f)
        phase[i] -= 1.0f;  // dt < 1, so one subtract is enough
      if (phase[i] < 0.0f)
        phase[i] += 1.0f;  // (defensive, in case dt ever goes negative)
    }
  }
};

template <int N, int SR, template <int, int> class IMPL_T>
class SimpleOscN : public BaseOscillatorN<N, SR> {
  using IMPL = IMPL_T<N, SR>;

 public:
  inline void tick(std::array<float, N>& out) {
    this->advancePhase();
    for (int i = 0; i < N; ++i) {
      out[i] = static_cast<IMPL* const>(this)->sample(this->phase[i],      // t
                                                      this->phaseInc[i]);  // dt
    }
  }
};

// -------- SINE --------
template <int N, int SR>
class SineOscN : public SimpleOscN<N, SR, SineOscN> {
  // static constexpr int kSineLUTBits = 9;
  // static constexpr int kLUTSize = 1 << kSineLUTBits;  // power of two
  // static constexpr float fLUTSize = float(kLUTSize);
  // static constexpr int kLUTMask = kLUTSize - 1;

  // static inline std::array<float, kLUTSize> calcTable() {
  //   std::array<float, kLUTSize> res;
  //   for (int i = 0; i < kLUTSize; ++i) {
  //     const float ang = TWO_PI * (float(i) / fLUTSize);
  //     res[i] = sinf(ang);
  //   }
  //   return res;
  // }

  // static inline std::array<float, kLUTSize> fLUTSine = calcTable();

 public:
  static float sample(float t, float /*dt*/ = 0.0f) {
    return sinf(t);
    // t in [0,1). Use power-of-two LUT with wrap via mask.
    // const float fpos = t * fLUTSize;
    // const int i0 = int(fpos) & kLUTMask;
    // const int i1 = (i0 + 1) & kLUTMask;
    // const float a = fpos - float(int(fpos));
    // return math::interpolate(fLUTSine[i0], fLUTSine[i1], a);
  }
};

// // -------- TRIANGLE (naive, light CPU) --------
// // tri = 1 - 2*abs(saw), where saw = 2*t - 1  in [-1, +1]
// template <int N, int SR>
// class TriOscN : public SimpleOscN<N, SR, TriOscN> {
//  public:
//   static float sample(float t, float /*dt*/ = 0.0f) {
//     const float saw = 2.0f * t - 1.0f;  // [-1..+1] with discontinuity at
//     wrap return 1.0f - 2.0f * fabsf(saw);    // [-1..+1], peak at center
//   }
// };

// -------- SAWTOOTH (polyblep) --------
// saw = 2*t - 1  in [-1, +1]
template <int N, int SR>
class SawOscN : public SimpleOscN<N, SR, SawOscN> {
 public:
  static float sample(const float t, const float dt) {
    return (2.0f * t - 1.0f) - dsp::polyblep(t, dt);
  }
};

// -------- SQUARE (variable PW, polyblep) --------
template <int N, int SR>
class SquareOscN : public BaseOscillatorN<N, SR> {
 public:
  std::array<float, N> pulseWidth = {0.5f};  // assumed clamped upstream

  static float sample(const float t, const float dt, const float pw = 0.5f) {
    // Dynamic margin: keep the two edges at least ~2*dt apart
    const float margin =
        fminf(0.5f - 1e-4f, 2.0f * dt);  // cap so it never flips
    const float pwEff = math::clamp(pw, margin, 1.0f - margin);

    float s = (t < pwEff) ? 1.0f : -1.0f;

    // rising edge at t = pwEff (wrap without floorf)
    float tr = t - pwEff;
    if (tr < 0.0f) tr += 1.0f;

    // PolyBLEP corrections
    s += dsp::polyblep(tr, dt);  // cancel +edge at t=pwEff
    s -= dsp::polyblep(t, dt);   // cancel wrap edge at t=0/1

    return s;
  }
};

// -------- TRIANGLE from PolyBLEP Square (stateless) --------
template <int N, int SR>
class TriOscN : public BaseOscillatorN<N, SR> {
 public:
  static float sample(const float t, const float dt, const float pw = 0.5f) {
    // 1) Get bandlimited square (polyBLEP corrected)
    const float sq = SquareOscN<N, SR>::sample(t, dt, pw);

    // 2) Integrate the square analytically:
    // For a symmetric ±1 square at 50% duty, the integral is a saw ramp.
    // Normalized form (triangle) is just cumulative sum of sq * dt,
    // but stateless we can express it as: tri = 2*phase - 1 integrated &
    // folded. The trick: triangle is the *running average* of square → saw →
    // abs.

    float tri = 2.0f * t - 1.0f;     // basic saw
    tri = 1.0f - 2.0f * fabsf(tri);  // naive triangle core

    // 3) Blend in bandlimited square slope to soften the cusp.
    // This approximates BLAMP by mixing in the square.
    tri += (sq * dt);

    return tri;
  }
};

// -------- MORPH SINE->TRIANGLE->SQUARE->SAW --------
template <int N, int SR>
class MorphOscN : public SquareOscN<N, SR> {
 public:
  std::array<float, N> morph = {0.0f};

  static constexpr float SINE_BOUND = 0.0f;
  static constexpr float TRIANGLE_BOUND = 1.0f / 3.0f;
  static constexpr float SQUARE_BOUND = 2.0f / 3.0f;
  static constexpr float SAW_BOUND = 1.f;
  static constexpr float WAVES_COUNT = 3.0f;
  static constexpr float EPSILON = 1e-5;

  inline float sample(float t, float dt, float pw, float m) {
    using namespace math;
    if (m - SINE_BOUND < EPSILON) {
      return SineOscN<N, SR>::sample(t);
    }
    if (SAW_BOUND - m < EPSILON) {
      return SawOscN<N, SR>::sample(t, dt);
    }
    if (m <= TRIANGLE_BOUND) {
      const float a = (m - SINE_BOUND) * WAVES_COUNT;  // 0..1
      return interpolate(SineOscN<N, SR>::sample(t),
                         TriOscN<N, SR>::sample(t, dt, pw), a);
    }
    if (m <= SQUARE_BOUND) {
      const float a = (m - TRIANGLE_BOUND) * WAVES_COUNT;  // 0..1
      return interpolate(TriOscN<N, SR>::sample(t, dt, pw),
                         SquareOscN<N, SR>::sample(t, dt, pw), a);
    }
    const float a = (m - SQUARE_BOUND) * WAVES_COUNT;  // 0..1
    return interpolate(SquareOscN<N, SR>::sample(t, dt, pw),
                       SawOscN<N, SR>::sample(t, dt), a);
  }

  inline void tickAll(std::array<float, N>& out) {
    this->advancePhase();
    for (int i = 0; i < N; ++i) {
      out[i] = sample(this->phase[i], this->phaseInc[i], this->pulseWidth[i],
                      morph[i]);
    }
  }

  inline void tickFirst(const int k, std::array<float, N>& out) {
    this->advancePhase();
    for (int i = 0; i < k; ++i) {
      out[i] = sample(this->phase[i], this->phaseInc[i], this->pulseWidth[i],
                      morph[i]);
    }
  }
};

}  // namespace zlkm