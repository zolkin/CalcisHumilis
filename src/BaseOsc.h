#pragma once
#include <Arduino.h>
#include <math.h>

#include "Helpers.h"  // expects pitch::hzToPitch / pitch::pitchToHz / pitch::semisToPitch

struct OscConfig {
  int sampleRate = 48000;
  float baseTuneHz = 16.3516f;  // C0
  float initialPan = 0.0f;      // -1..+1 equal-power
};

template <typename Config>
class BaseOscillator {
 public:
  explicit BaseOscillator(const Config& c = Config()) { setConfig(c); }

  void setConfig(const Config& c) {
    cfg_ = c;
    baseTunePitch_ = pitch::hzToPitch(cfg_.baseTuneHz);  // log2 Hz
    sr_ = float(cfg_.sampleRate);
  }

  // Call on trigger/note-on; latches pan & resets phase
  void reset(float pan = NAN) {
    phase = 0.0f;
    phaseInc = 0.0f;
    currentPan_ = isnan(pan) ? cfg_.initialPan : pan;
    updatePanGains();
  }

  inline void setPan(float pan) {
    currentPan_ = pan;
    updatePanGains();
  }
  inline float pan() const { return currentPan_; }
  inline float phaseNow() const { return phase; }
  inline float freqNowHz() const { return pitch::pitchToHz(baseTunePitch_); }

  // required state you requested (kept here so all oscs share it)
  float baseTunePitch_ = 0.0f;  // log2(baseTuneHz)
  float phase = 0.0f, phaseInc = 0.0f;

  // per-osc smoothing factor for phaseInc (0..1). 0.25f matches your kick.
  float smoothingAlpha_ = 0.25f;

  Config cfg_;

 protected:
  float sr_ = 48000.0f;

  // pan
  float currentPan_ = 0.0f;
  float gainL_ = 0.7071f, gainR_ = 0.7071f;

  // helpers each osc will use inside tickStereo
  inline float pitchNowFromSemis(float pitchSemis) const {
    return baseTunePitch_ + pitch::semisToPitch(pitchSemis);
  }

  inline float targetIncFromPitch(float pitchNow) const {
    const float f = pitch::pitchToHz(pitchNow);
    return (2.0f * PI * f) / sr_;
  }

  inline void smoothPhaseIncTowards(float targetInc) {
    // set smoothingAlpha_ to 1.0f if you want immediate tracking
    phaseInc += (targetInc - phaseInc) * smoothingAlpha_;
  }

  inline float advancePhase(float pitchSemis) {
    const float pNow = pitchNowFromSemis(pitchSemis);
    const float trg = targetIncFromPitch(pNow);
    smoothPhaseIncTowards(trg);
    phase += phaseInc;
    if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    return phase;
  }

  inline void panOut(float s, float& outL, float& outR) const {
    outL = s * gainL_;
    outR = s * gainR_;
  }

  inline void updatePanGains() {
    // equal-power: -1..+1 → 0..pi/2
    const float t = (currentPan_ + 1.0f) * 0.5f * (PI * 0.5f);
    gainL_ = cosf(t);
    gainR_ = sinf(t);
  }
};

// -------- SINE --------
class SineOsc : public BaseOscillator<OscConfig> {
 public:
  static float sine(float ph) { return sinf(ph); }

  using BaseOscillator::BaseOscillator;
  inline void tickStereo(float pitchSemis, float& outL, float& outR) {
    panOut(sine(advancePhase(pitchSemis)), outL, outR);
  }
};

// -------- TRIANGLE (naive, light CPU) --------
// tri = 1 - 2*abs(saw), where saw = (ph/PI) - 1  in [-1, +1]
class TriOsc : public BaseOscillator<OscConfig> {
 public:
  using BaseOscillator::BaseOscillator;
  static float tri(float ph) {
    float saw = (ph / PI) - 1.0f;     // [-1..+1] with discontinuity at wrap
    return 1.0f - 2.0f * fabsf(saw);  // [-1..+1], peak at center
  }

  inline void tickStereo(float pitchSemis, float& outL, float& outR) {
    panOut(tri(advancePhase(pitchSemis)), outL, outR);
  }
};

// -------- SAWTOOTH (polyblep) --------
// saw = (ph/PI) - 1  in [-1, +1]
class SawOsc : public BaseOscillator<OscConfig> {
 public:
  using BaseOscillator::BaseOscillator;
  static float saw(float ph, float phInc) {
    const float t = ph / TWO_PI;
    const float dt = phInc / TWO_PI;
    return (ph / PI) - 1.0f - dsp::polyblep(t, dt);  // [-1..+1]
  }

  inline void tickStereo(float pitchSemis, float& outL, float& outR) {
    const float t = advancePhase(pitchSemis);
    panOut(saw(t, phaseInc), outL, outR);
  }
};

struct SquareConfig : OscConfig {
  float pulseWidth = 0.5f;
};
// -------- SQUARE (50% duty, polyblep) --------
class SquareOsc : public BaseOscillator<SquareConfig> {
 public:
  using BaseOscillator::BaseOscillator;

  static float square(float ph, float phInc, float pw = 0.5f) {
    return (ph < (2.0f * PI) * pw) ? +1.0f : -1.0f;
  }

  inline void tickStereo(float pitchSemis, float& outL, float& outR) {
    const float pw = math::clamp(cfg_.pulseWidth, 0.01f, 0.99f);
    const float t = advancePhase(pitchSemis);
    panOut(square(t, phaseInc, pw), outL, outR);
  }
};

struct MorphConfig : SquareConfig {
  float morph = 0.0f;
};

// -------- MORPH SINE->TRIANGLE->SQUARE->SAW --------
class MorphOsc : public BaseOscillator<MorphConfig> {
 public:
  using BaseOscillator::BaseOscillator;

  static constexpr float SINE_BOUND = 0.0f;
  static constexpr float TRIANGLE_BOUND = 1.0f / 3.0f;
  static constexpr float SQUARE_BOUND = 2.0f / 3.0f;
  static constexpr float SAW_BOUND = 1.0;
  static constexpr float WAVES_COUNT = 3.0f;

  inline void tickStereo(float pitchSemis, float& outL, float& outR) {
    using namespace math;

    const float ph = advancePhase(pitchSemis);
    const float pw = math::clamp(cfg_.pulseWidth, 0.01f, 0.99f);
    const float m = math::clamp(cfg_.morph, 0.f, 1.f);

    float s;
    if (m <= TRIANGLE_BOUND) {
      const float t = (m - SINE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(SineOsc::sine(ph), TriOsc::tri(ph), t);
    } else if (m <= SQUARE_BOUND) {
      const float t = (m - TRIANGLE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(TriOsc::tri(ph), SquareOsc::square(ph, phaseInc, pw), t);
    } else {
      const float t = (m - SQUARE_BOUND) * WAVES_COUNT;  // 0..1
      s = interpolate(SquareOsc::square(ph, pw), SawOsc::saw(ph, phaseInc), t);
    }

    panOut(s, outL, outR);
  }
};