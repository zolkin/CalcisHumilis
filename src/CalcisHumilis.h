#pragma once
#include <Arduino.h>
#include <AudioTools.h>
#include <JLED.h>  // or <jled.h> depending on your install
#include <Stream.h>

#include "Slew.h"

struct CalcisConfig {
  int sampleRate = 48000;
  float baseHz = 55.0f;
  float startMult = 6.0f;
  float ampMs = 220.0f;
  float pitchMs = 30.0f;
  float clickMs = 6.0f;
  float clickAmt = 0.2f;
  float outGain = 0.85f;
  float gainSlewMs = 3.0f;
  float pan = 0.0f;  // -1..+1 equal-power

  // small attacks (ms) for smooth starts
  float ampAttackMs = 0.001f;
  float pitchAttackMs = 0.01f;
  float clickAttackMs = 0.001f;

  bool kPack24In32 = false;
};

class CalcisHumilis {
 public:
  explicit CalcisHumilis(const CalcisConfig &cfg = CalcisConfig());

  // Update parameters & envelope rates WITHOUT resetting envelopes/phase
  void setConfig(const CalcisConfig &cfg);

  void trigger();  // retrigger envelopes + reset phase
  void tickLED();  // call from loop() if you want

  // Fill an interleaved stereo block (nFrames = stereo frames)
  void fillBlock(int32_t *dstLR, size_t nFrames);

 private:
  static float rateFromMs(float ms, int sr);
  static float softClip(float x);

  void applyEnvelopeRates();
  void updatePanGains();

  CalcisConfig cfg_;
  audio_tools::ADSR envAmp, envPitch, envClick;

  float phase = 0.0f, phaseInc = 0.0f;

  // pan gains
  float gainL = 0.7071f, gainR = 0.7071f;
  SlewOnePole gainSlew_;

  float currentPan = 0.5f;

  // LED
  JLed triggerLED{2};
};