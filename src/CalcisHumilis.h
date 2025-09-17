#pragma once
#include <Arduino.h>
#include <AudioTools.h>
#include <JLED.h>  // or <jled.h> depending on your install
#include <Stream.h>

#include "BaseOsc.h"
#include "Slew.h"
#include "Swarm.h"

enum class OscMode : uint8_t { Basic = 0, Swarm };

template <int SR>
struct CalcisConfig {
  // ...existing...
  OscMode oscMode = OscMode::Swarm;

  MorphConfigN<1, SR> baseOsc;
  SwarmConfig<SR> swarmOsc;

  float pitchSemis = 24.0f;  // knob: semitone offset from baseTuneHz
  float startMult = 4.0f;    // unchanged (ratio for pitch env)

  float ampMs = 220.0f;
  float pitchMs = 30.0f;
  float clickMs = 6.0f;
  float clickAmt = 0.2f;
  float outGain = 0.7f;
  float gainSlewMs = 3.0f;
  float pan = 0.f;

  // small attacks (ms) for smooth starts
  float ampAttackMs = 0.001f;
  float pitchAttackMs = 0.01f;
  float clickAttackMs = 0.001f;

  bool kPack24In32 = false;
};

template <int SR>
class CalcisHumilis {
 public:
  explicit CalcisHumilis(const CalcisConfig<SR> &cfg = CalcisConfig<SR>());

  // Update parameters & envelope rates WITHOUT resetting envelopes/phase
  void setConfig(const CalcisConfig<SR> &cfg);

  void trigger();  // retrigger envelopes + reset phase
  void tickLED();  // call from loop() if you want

  // Fill an interleaved stereo block (nFrames = stereo frames)
  void fillBlock(int32_t *dstLR, size_t nFrames);

 private:
  static float rateFromMs(float ms, int sr);
  float softClip(float x);

  float tickBasic(float p, float sr);

  void applyEnvelopeRates();
  void updatePanGains();

  CalcisConfig<SR> cfg_;
  audio_tools::ADSR envAmp, envPitch, envClick;
  audio_tools::ADSR envFilter;

  // Basic state
  MorphOscN<1, SR> osc;

  SwarmMorph<10, SR, 4> swarm;

  static inline float hzToPitch(float hz) { return log2f(hz); }  // log2 Hz
  static inline float pitchToHz(float pit) { return exp2f(pit); }
  static inline float semisToPitch(float s) { return s / 12.0f; }

  SlewOnePoleN<1, SR> gainSlew_;
  float currentPan = 0.5f;

  // LED
  JLed triggerLED{2};
  JLed clippingLED{3};
};

#include "CalcisHumilis_impl.hh"