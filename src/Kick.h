#pragma once
#include <Arduino.h>
#include <Debug.h>

class KickSynth {
public:
  void init(int sampleRate,
            float baseHz, float startMult,
            float ampMs, float pitchMs, float clickMs,
            float clickAmt, float outGain,
            int trigPeriodMs);
  void trigger();
  void fillBlock(int16_t* dstInterleaved, int nFrames, int sampleRate);

  bool onceIn(int32_t samples) {
    return (sampleCounter % samples) == 0;
  }

private:
  // params
  int   sr = 48000;
  float baseHz = 55.0f, startMult = 6.0f;
  float ampMs = 220.0f, pitchMs = 30.0f, clickMs = 6.0f;
  float clickAmt = 0.2f, outGain = 0.85f;
  int   trigPeriodSamples = 96000;

  // state
  float ampEnv=0, pitchEnv=0, clickEnv=0;
  float ampA=0, pitchA=0, clickA=0;
  float phase=0, phaseInc=0;
  uint32_t sampleCounter = 0;

#ifdef DEBUG
  uint32_t trigCounter = 0;
  uint32_t ledBlinkSamples = 1000;
#endif

  inline float softClip(float x) const {
    const float t=0.95f;
    if (x> t) return t + (x-t)*0.05f;
    if (x<-t) return -t + (x+t)*0.05f;
    return x;
  }
};