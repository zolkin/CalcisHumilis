#pragma once
#include <Arduino.h>
#include <AudioTools.h>
#include <JLED.h>
#include <Stream.h>

#include "BaseOsc.h"
#include "FirOversample.h"  // the helper above
#include "Slew.h"
#include "Swarm.h"

enum class OscMode : uint8_t { Basic = 0, Swarm };

template <int SR, int OS>
struct CalcisConfig {
  OscMode oscMode = OscMode::Swarm;

  MorphConfigN<1, SR * OS> baseOsc;
  SwarmConfig<SR * OS> swarmOsc;

  float pitchSemis = 24.0f;
  float startMult = 4.0f;

  float ampMs = 220.0f;
  float pitchMs = 30.0f;
  float clickMs = 6.0f;
  float clickAmt = 0.2f;
  float outGain = 0.7f;
  float gainSlewMs = 3.0f;
  float pan = 0.f;

  float ampAttackMs = 0.001f;
  float pitchAttackMs = 0.01f;
  float clickAttackMs = 0.001f;

  bool kPack24In32 = false;
};

// -------------------- NEW: OS template param --------------------
// NTAPS is configurable; 63 is a decent default for 2–4x OS.
template <int SR, int OS = 1, int NTAPS = 63>
class CalcisHumilis {
 public:
  using Cfg = CalcisConfig<SR, OS>;

  explicit CalcisHumilis(const Cfg &cfg = Cfg());

  void setConfig(const Cfg &cfg);
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

  Cfg cfg_;
  audio_tools::ADSR envAmp, envPitch, envClick;
  audio_tools::ADSR envFilter;  // unused here but kept

  // Oscillators run at OS*SR so their phase math sees true step size
  MorphOscN<1, SR * OS> osc;
  SwarmMorph<10, SR * OS> swarm;

  SlewOnePoleN<1, SR> gainSlew_{};
  float currentPan = 0.5f;

  // LEDs
  JLed triggerLED{2};
  JLed clippingLED{3};

  // FIR at OS*SR, then decimate by OS
  OversampleDecimator<OS, NTAPS> osDecim_;
};

#include "CalcisHumilis_impl.hh"