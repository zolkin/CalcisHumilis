#pragma once
#include <Arduino.h>
#include <math.h>

#include "Helpers.h"  // expects pitch::hzToPitch / pitch::pitchToHz / pitch::semisToPitch

template <int N, int SR>
struct OscConfigN {
  static constexpr int SAMPLE_RATE = SR;
  static constexpr int COUNT = N;

  std::array<float, N> baseTuneHz = arrays::filled<N>(16.3516f);  // C0
  std::array<float, N> initialPan = arrays::filled<N>(0.0f);
};

template <int N, int SR, template <int, int> class Config>
class BaseOscillatorN {
 public:
  using ConfigT = Config<N, SR>;
  explicit BaseOscillatorN(const ConfigT& c = ConfigT()) { setConfig(c); }

  void setConfig(const ConfigT& c) {
    cfg_ = c;

    for (int i = 0; i < N; ++i) {
      baseTunePitch_[i] = pitch::hzToPitch(cfg_.baseTuneHz[i]);  // log2 Hz
    }
  }

  // Call on trigger/note-on; latches pan & resets phase
  void reset(const int i, const float pan = NAN, const float ph = 0.f) {
    phase[i] = ph;
    phaseInc[i] = 0.0f;
    currentPan_[i] = isnan(pan) ? cfg_.initialPan[i] : pan;
    updatePanGains(i);
  }

  inline void setPan(const int i, const float pan) {
    currentPan_[i] = pan;
    updatePanGains(i);
  }

  inline float pan(const int i) const { return currentPan_[i]; }
  inline float phaseNow(const int i) const { return phase[i]; }
  inline float freqNowHz(const int i) const {
    return pitch::pitchToHz(baseTunePitch_[i]);
  }

 protected:
  // required state you requested (kept here so all oscs share it)
  std::array<float, N> baseTunePitch_ = {0.0f};  // log2(baseTuneHz)
  std::array<float, N> phase = {0.0f};
  std::array<float, N> phaseInc = {0.0f};

  // per-osc smoothing factor for phaseInc (0..1). 0.25f matches your kick.
  std::array<float, N> smoothingAlpha_ = {0.25f};

  ConfigT cfg_;

  // pan
  std::array<float, N> currentPan_ = {0.0f};
  std::array<float, N> gainL_ = {0.7071f};
  std::array<float, N> gainR_ = {0.7071f};

  // helpers each osc will use inside tickStereo
  inline float pitchNowFromSemis(const int i, const float pitchSemis) const {
    return baseTunePitch_[i] + pitch::semisToPitch(pitchSemis);
  }

  inline float targetIncFromPitch(const int i, const float pitchNow) const {
    const float f = pitch::pitchToHz(pitchNow);
    return (TWO_PI * f) / SR;
  }

  inline void smoothPhaseIncTowards(const int i, const float targetInc) {
    // set smoothingAlpha_ to 1.0f if you want immediate tracking
    phaseInc[i] += (targetInc - phaseInc[i]) * smoothingAlpha_[i];
  }

  inline float advancePhase(const int i, float pitchSemis) {
    const float pNow = pitchNowFromSemis(i, pitchSemis);
    const float trg = targetIncFromPitch(i, pNow);
    smoothPhaseIncTowards(i, trg);
    phase[i] += phaseInc[i];
    if (phase[i] >= TWO_PI) phase[i] -= TWO_PI;
    return phase[i];
  }

  inline void updatePanGains(const int i) {
    // equal-power: -1..+1 → 0..pi/2
    const float t = (currentPan_[i] + 1.0f) * 0.5f * (PI * 0.5f);
    gainL_[i] = cosf(t);
    gainR_[i] = sinf(t);
  }

  inline void panOut(const int i, float s, float& outL, float& outR) const {
    outL = s * gainL_[i];
    outR = s * gainR_[i];
  }
};

// -------- SINE --------
template <int N, int SR>
class SineOscN : public BaseOscillatorN<N, SR, OscConfigN> {
 public:
  static float sine(float ph) { return sinf(ph); }

  using BaseOscillatorN<N, SR, OscConfigN>::BaseOscillatorN;

  inline void tickStereo(const int i, const float pitchSemis, float& outL,
                         float& outR) {
    this->panOut(i, sine(this->advancePhase(i, pitchSemis)), outL, outR);
  }
};

// -------- TRIANGLE (naive, light CPU) --------
// tri = 1 - 2*abs(saw), where saw = (ph/PI) - 1  in [-1, +1]
template <int N, int SR>
class TriOscN : public BaseOscillatorN<N, SR, OscConfigN> {
 public:
  using BaseOscillatorN<N, SR, OscConfigN>::BaseOscillatorN;
  static float tri(float ph) {
    float saw = (ph / PI) - 1.0f;     // [-1..+1] with discontinuity at wrap
    return 1.0f - 2.0f * fabsf(saw);  // [-1..+1], peak at center
  }

  inline void tickStereo(const int i, const float pitchSemis, float& outL,
                         float& outR) {
    this->panOut(i, tri(this->advancePhase(i, pitchSemis)), outL, outR);
  }
};

// -------- SAWTOOTH (polyblep) --------
// saw = (ph/PI) - 1  in [-1, +1]
template <int N, int SR>
class SawOscN : public BaseOscillatorN<N, SR, OscConfigN> {
 public:
  using BaseOscillatorN<N, SR, OscConfigN>::BaseOscillatorN;

  static float saw(float ph, float phInc) {
    float t = ph * (1.0f / TWO_PI);
    t -= floorf(t);
    float dt = phInc * (1.0f / TWO_PI);
    return (ph / PI) - 1.0f - dsp::polyblep(t, dt);
  }

  inline void tickStereo(const int i, const float pitchSemis, float& outL,
                         float& outR) {
    const float t = this->advancePhase(i, pitchSemis);
    this->panOut(i, saw(t, this->phaseInc[i]), outL, outR);
  }
};

template <int N, int SR>
struct SquareConfigN : OscConfigN<N, SR> {
  std::array<float, N> pulseWidth = {0.5f};
};
// -------- SQUARE (50% duty, polyblep) --------
template <int N, int SR>
class SquareOscN : public BaseOscillatorN<N, SR, SquareConfigN> {
 public:
  using BaseOscillatorN<N, SR, SquareConfigN>::BaseOscillatorN;

  static float square(const float ph, const float phInc,
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

  inline void tickStereo(const int i, const float pitchSemis, float& outL,
                         float& outR) {
    const float pw = math::clamp(this->cfg_.pulseWidth[i], 0.01f, 0.99f);
    const float ph = this->advancePhase(i, pitchSemis);
    const float inc = this->phaseInc[i];
    this->panOut(i, square(ph, inc, pw), outL, outR);
  }
};

template <int N, int SR>
struct MorphConfigN : SquareConfigN<N, SR> {
  std::array<float, N> morph = {0.0f};
};

// -------- MORPH SINE->TRIANGLE->SQUARE->SAW --------
template <int N, int SR>
class MorphOscN : public BaseOscillatorN<N, SR, MorphConfigN> {
 public:
  using BaseOscillatorN<N, SR, MorphConfigN>::BaseOscillatorN;

  static constexpr float SINE_BOUND = 0.0f;
  static constexpr float TRIANGLE_BOUND = 1.0f / 3.0f;
  static constexpr float SQUARE_BOUND = 2.0f / 3.0f;
  static constexpr float SAW_BOUND = 1.0;
  static constexpr float WAVES_COUNT = 3.0f;

  inline void tickStereo(const int i, float pitchSemis, float& outL,
                         float& outR) {
    using namespace math;

    const auto ph = this->advancePhase(i, pitchSemis);
    const auto pw = math::clamp(this->cfg_.pulseWidth[i], 0.01f, 0.99f);
    const auto m = math::clamp(this->cfg_.morph[i], 0.f, 1.f);

    float s;
    if (m <= TRIANGLE_BOUND) {
      const float t = (m - SINE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(SineOscN<N, SR>::sine(ph), TriOscN<N, SR>::tri(ph), t);
    } else if (m <= SQUARE_BOUND) {
      const float t = (m - TRIANGLE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(TriOscN<N, SR>::tri(ph),
                      SquareOscN<N, SR>::square(ph, this->phaseInc[i], pw), t);
    } else {
      const float t = (m - SQUARE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(SquareOscN<N, SR>::square(ph, this->phaseInc[i], pw),
                      SawOscN<N, SR>::saw(ph, this->phaseInc[i]), t);
    }

    this->panOut(i, s, outL, outR);
  }
};