#pragma once

#include <AudioTools.h>

// need to include later

#include <Arduino.h>
#include <Stream.h>

#include "ADEnvelopes.h"
#include "DJFilter.h"
#include "MorphOsc.h"
#include "Swarm.h"

namespace zlkm {

// -------------------- NEW: OS template param --------------------
// NTAPS is configurable; 63 is a decent default for 2–4x OS.
template <class TR>
class CalcisHumilis {
  static constexpr int NTAPS = 63;

  static constexpr int SR = TR::SR;
  static constexpr int OS = TR::OS;
  using OutBuffer = typename TR::BufferT;
  using IntBuffer = std::array<float, TR::BLOCK_ELEMS>;

  static constexpr float INV_SR = 1.f / float(SR);

 public:
  static constexpr int MAX_SWARM_VOICES = 16;

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

    float outGain = .7f;
    float cyclesPerSample = cycles(65.f);

    std::array<EnvCfg, EnvCount> envs = {
        EnvCfg{rate(1.f), rate(330.f)},         // amp
        EnvCfg{rate(10.f), rate(20.f), 8.f},    // pitch
        EnvCfg{rate(1.f), rate(6.f), .2f},      // click
        EnvCfg{rate(200.f), rate(500.f), 1.f},  // swarm
        EnvCfg{rate(10.f), rate(200.f), 1.f},   // morph
        EnvCfg{rate(1.f), rate(60.f), 1.f},     // filter
    };

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
  void fillBlock(OutBuffer &destLR);

 private:
  float softClip(float x);

  static inline float hzToPitch(float hz) { return log2f(hz); }
  static inline float pitchToHz(float pit) { return exp2f(pit); }
  static inline float semisToPitch(float s) { return s / 12.0f; }

  const Cfg *cfg_;
  Feedback *fb_;

  Envelopes envelopes_;

  float outGain_;
  float cyclesPerSample_;

  // Oscillators run at OS*SR so their phase math sees true step size
  SwarmMorph<MAX_SWARM_VOICES, SR * OS> swarm;
  FilterCfg fCfg_;

  float currentPan = 0.5f;

  Filter filterL, filterR;

  int trigCounter_ = 0;
};

}  // namespace zlkm

#include "CalcisHumilis_impl.hh"
