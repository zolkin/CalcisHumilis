#pragma once

#include <AudioTools.h>

// need to include later

#include <Arduino.h>
#include <Stream.h>

#include "BaseOsc.h"
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

 public:
  enum class OscMode : uint8_t { Basic = 0, Swarm };

  struct Cfg {
    OscMode oscMode = OscMode::Swarm;

    SwarmConfig<SR * OS> swarmOsc;

    float pitchSemis = 12.0f;
    float startMult = 8.0f;

    float ampMs = 330.0f;
    float pitchMs = 20.0f;
    float clickMs = 6.0f;
    float clickAmt = 0.2f;
    float outGain = 0.7f;
    float gainSlewMs = 3.0f;
    float pan = 0.f;

    float ampAttackMs = 0.001f;
    float pitchAttackMs = 0.01f;
    float clickAttackMs = 0.001f;

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
  static float rateFromMs(float ms, int sr);
  float softClip(float x);
  void applyEnvelopeRates();

  static inline float hzToPitch(float hz) { return log2f(hz); }
  static inline float pitchToHz(float pit) { return exp2f(pit); }
  static inline float semisToPitch(float s) { return s / 12.0f; }

  const Cfg *cfg_;
  Feedback *fb_;
  audio_tools::ADSR envAmp, envPitch, envClick;
  audio_tools::ADSR envFilter;  // unused here but kept

  // Oscillators run at OS*SR so their phase math sees true step size
  SwarmMorph<10, SR * OS> swarm;

  SlewOnePoleN<1, SR> gainSlew_{};
  float currentPan = 0.5f;

  // FIR at OS*SR, then decimate by OS
  OversampleDecimator<OS, NTAPS> osDecim_;

  int trigCounter_ = 0;
};

}  // namespace zlkm

#include "CalcisHumilis_impl.hh"
