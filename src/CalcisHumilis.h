#pragma once

#include <AudioTools.h>

// need to include later

#include <Arduino.h>
#include <Stream.h>

#include "ADEnvelopes.h"
#include "BaseOsc.h"
#include "DJFilter.h"
#include "FirOversample.h"  // the helper above
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

  using Filter = DJFilterTPT<SR * OS>;
  using FilterCfg = typename Filter::Cfg;

  enum OscMode { OscSwarm = 0, OscCount };

  enum Envs {
    EnvAmp = 0,
    EnvPitch,
    EnvClick,
    EnvFilter,
    EnvSwarm,
    EnvMorph,
    EnvCount
  };

  using Envelopes = ADEnvelopes<EnvCount>;
  using EnvCfg = typename Envelopes::EnvCfg;

  struct Cfg {
    OscMode oscMode = OscSwarm;

    SwarmConfig<SR * OS> swarmOsc;

    float cyclesPerSample = cycles(65.f);
    float pitchDepthMult = 8.0f;

    std::array<EnvCfg, EnvCount> envs = {
        EnvCfg{rate(1.f), rate(330.f)},       // amp
        EnvCfg{rate(10.f), rate(20.f), 8.f},  // pitch
        EnvCfg{rate(1.f), rate(6.f), .2f},    // click
        EnvCfg{rate(1.f), rate(6.f), .2f},    // swarm
        EnvCfg{rate(1.f), rate(6.f), .2f},    // morph
        EnvCfg{rate(1.f), rate(6.f), .2f},    // filter
    };

    float outGain = .7f;
    float gainSlew = 3.0f;
    float pan = 0.f;

    float swarmAtt = rate(200.f);
    float swarmDec = rate(500.f);

    float morphAtt = rate(10.f);
    float morphDec = rate(200.f);
    float morphAmt = .8f;

    FilterCfg filter;

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

  static inline float hzToPitch(float hz) { return log2f(hz); }
  static inline float pitchToHz(float pit) { return exp2f(pit); }
  static inline float semisToPitch(float s) { return s / 12.0f; }

  const Cfg *cfg_;
  Feedback *fb_;

  Envelopes envelopes_;

  // Oscillators run at OS*SR so their phase math sees true step size
  SwarmMorph<MAX_SWARM_VOICES, SR * OS> swarm;

  float currentPan = 0.5f;

  Filter filterL, filterR;

  // FIR at OS*SR, then decimate by OS
  OversampleDecimator<OS, NTAPS> osDecim_;

  int trigCounter_ = 0;
};

}  // namespace zlkm

#include "CalcisHumilis_impl.hh"
