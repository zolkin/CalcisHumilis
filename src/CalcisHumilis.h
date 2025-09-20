#pragma once

#include <AudioTools.h>

// need to include later

#include <Arduino.h>
#include <Stream.h>

#include "BaseOsc.h"
#include "DJFilter.h"
#include "FirOversample.h"  // the helper above
#include "Slew.h"
#include "Swarm.h"

namespace zlkm {

// -------------------- NEW: OS template param --------------------
// NTAPS is configurable; 63 is a decent default for 2–4x OS.
template <class TR>
class CalcisHumilis {
  static constexpr int NTAPS = 63;

  static constexpr int SR = TR::SR;
  static constexpr int OS = TR::OS;
  static constexpr int MAX_SWARM_VOICES = 16;

  static constexpr float INV_SR = 1.f / float(SR);

  static constexpr float SLEW_MS_ALL = 3.f;

 public:
  static constexpr float rate(float ms) { return dsp::msToRate(ms, SR); }
  static constexpr float cycles(float hz) { return hz * INV_SR; }

  enum class OscMode : uint8_t { Basic = 0, Swarm };

  struct Cfg {
    OscMode oscMode = OscMode::Swarm;

    SwarmConfig<SR * OS> swarmOsc;

    float cyclesPerSample = cycles(65.f);
    float pitchDepthMult = 8.0f;

    float ampAtt = rate(1.f);
    float ampDec = rate(330.f);

    float pitchAtt = rate(10.f);
    float pitchDec = rate(20.0f);

    float clickAtt = rate(1.f);
    float clickDec = rate(6.f);
    float clickAmt = .2f;

    float outGain = .7f;
    float gainSlew = 3.0f;
    float pan = 0.f;

    float swarmAtt = rate(200.f);
    float swarmDec = rate(500.f);

    float morphAtt = rate(10.f);
    float morphDec = rate(200.f);
    float morphAmt = .8f;

    float filterGCut = dsp::hzToGCut<SR>(2000.f);
    float filterKDamp = dsp::res01ToKDamp(0.f);
    float filterMorph = 0.f;
    float filterDrive = 0.f;

    int trigCounter = 0;

    bool kPack24In32 = false;
  };

  struct Feedback {
    int saturationCounter = 0;
  };

  explicit CalcisHumilis(const Cfg *cfg, Feedback *fb);

  void trigger();
  void tickLED();

  // Fill an interleaved stereo block (nFrames = stereo frames)
  void fillBlock(int32_t *dstLR, size_t nFrames);

 private:
  float softClip(float x);
  void applyEnvelopeRates();

  static inline float hzToPitch(float hz) { return log2f(hz); }
  static inline float pitchToHz(float pit) { return exp2f(pit); }
  static inline float semisToPitch(float s) { return s / 12.0f; }

  const Cfg *cfg_;
  Feedback *fb_;
  audio_tools::ADSR envAmp, envPitch, envClick;
  audio_tools::ADSR envFilter;  // unused here but kept

  audio_tools::ADSR envSwarm, envMorph;

  // Oscillators run at OS*SR so their phase math sees true step size
  SwarmMorph<MAX_SWARM_VOICES, SR * OS> swarm;

  SlewOnePoleN<1, SR> gainSlew_{};
  float currentPan = 0.5f;

  DJFilterTPT<SR * OS> filterL, filterR;

  // FIR at OS*SR, then decimate by OS
  OversampleDecimator<OS, NTAPS> osDecim_;

  int trigCounter_ = 0;
};

}  // namespace zlkm

#include "CalcisHumilis_impl.hh"
