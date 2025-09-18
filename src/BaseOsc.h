#pragma once
#include <Arduino.h>
#include <math.h>

#include <array>

#include "Helpers.h"  // expects pitch::hzToPitch / pitch::pitchToHz / pitch::semisToPitch

namespace zlkm {

template <int N, int SR>
class BaseOscillatorN {
  static constexpr float FREQ_TO_PHASE = TWO_PI / SR;

 public:
  // Call on trigger/note-on; latches pan & resets phase
  void reset(const bool randomPhase = false) {
    for (int i = 0; i < N; ++i) {
      phase[i] = randomPhase ? math::rand01() * TWO_PI_F : 0.0f;
      phaseInc[i] = 0.0f;
    }
  }

  std::array<float, N> freqNowHz = {0.0f};
  std::array<float, N> phase = {0.0f};
  std::array<float, N> phaseInc = {0.0f};

 protected:
  inline void smoothPhaseIncTowards(const float targetInc) {
    // set smoothingAlpha_ to 1.0f if you want immediate tracking
  }

  inline void advancePhase() {
    for (int i = 0; i < N; ++i) {
      phaseInc[i] +=
          (freqNowHz[i] * FREQ_TO_PHASE - phaseInc[i]) * SMOOTHING_FACTOR;
      phase[i] += phaseInc[i];
      phase[i] = fmodf(phase[i], TWO_PI_F);
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
      out[i] = static_cast<IMPL* const>(this)->sample(this->phase[i],
                                                      this->phaseInc[i]);
    }
  }
};

// -------- SINE --------
template <int N, int SR>
class SineOscN : public SimpleOscN<N, SR, SineOscN> {
 public:
  static float sample(float ph, float _ = 0) { return sinf(ph); }
};

// -------- TRIANGLE (naive, light CPU) --------
// tri = 1 - 2*abs(saw), where saw = (ph/PI) - 1  in [-1, +1]
template <int N, int SR>
class TriOscN : public SimpleOscN<N, SR, TriOscN> {
 public:
  static float sample(float ph, float _ = 0) {
    float saw = (ph / PI) - 1.0f;     // [-1..+1] with discontinuity at wrap
    return 1.0f - 2.0f * fabsf(saw);  // [-1..+1], peak at center
  }
};

// -------- SAWTOOTH (polyblep) --------
// saw = (ph/PI) - 1  in [-1, +1]
template <int N, int SR>
class SawOscN : public SimpleOscN<N, SR, SawOscN> {
 public:
  static float sample(float ph, float pi) {
    float t = ph * (1.0f / TWO_PI);
    t -= floorf(t);
    float dt = pi * (1.0f / TWO_PI);
    return (ph / PI) - 1.0f - dsp::polyblep(t, dt);
  }
};

// -------- SQUARE (50% duty, polyblep) --------
template <int N, int SR>
class SquareOscN : public BaseOscillatorN<N, SR> {
 public:
  std::array<float, N> pulseWidth = {0.5f};

  static float sample(const float ph, const float phInc,
                      const float pw = 0.5f) {
    // normalize
    float t = ph * (1.0f / TWO_PI);
    float dt = phInc * (1.0f / TWO_PI);

    // base hard square
    float s = (t < pw) ? +1.0f : -1.0f;

    // rising edge at t = pw
    float tr = t - pw;
    tr -= floorf(tr);  // wrap to [0,1)
    s += dsp::polyblep(tr, dt);

    // falling edge at t = 0 (a.k.a. 1)
    float tf = t;  // edge at 0
    s -= dsp::polyblep(tf, dt);

    return s;
  }

  inline void tick(std::array<float, N>& out) {
    this->advancePhase();
    for (int i = 0; i < N; ++i) {
      out[i] = sample(this->phase[i], this->phaseInc[i], this->pulseWidth[i]);
    }
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
  static constexpr float SAW_BOUND = 1.0;
  static constexpr float WAVES_COUNT = 3.0f;

  inline float sample(float ph, float pi, float pw, float m) {
    using namespace math;
    if (m <= TRIANGLE_BOUND) {
      const float t = (m - SINE_BOUND) * WAVES_COUNT;  // 0..1
      return interpolate(SineOscN<N, SR>::sample(ph),
                         TriOscN<N, SR>::sample(ph), t);
    }
    if (m <= SQUARE_BOUND) {
      const float t = (m - TRIANGLE_BOUND) * WAVES_COUNT;  // 0..1
      return interpolate(TriOscN<N, SR>::sample(ph),
                         SquareOscN<N, SR>::sample(ph, pi, pw), t);
    }
    const float t = (m - SQUARE_BOUND) * WAVES_COUNT;  // 0..1
    return interpolate(SquareOscN<N, SR>::sample(ph, pi, pw),
                       SawOscN<N, SR>::sample(ph, pi), t);
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