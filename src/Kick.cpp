#include "Kick.h"
#include <math.h>

static inline float decayCoeffMs(float ms, int sr) {
  float tau = (ms <= 0.1f) ? 1.0f : (ms * sr / 1000.0f);
  return expf(-1.0f / tau);
}

void KickSynth::init(int sampleRate,
                     float baseHz_, float startMult_,
                     float ampMs_, float pitchMs_, float clickMs_,
                     float clickAmt_, float outGain_,
                     int trigPeriodMs) {
  sr = sampleRate;
  baseHz = baseHz_; startMult = startMult_;
  ampMs = ampMs_; pitchMs = pitchMs_; clickMs = clickMs_;
  clickAmt = clickAmt_; outGain = outGain_;
  ampA = decayCoeffMs(ampMs, sr);
  pitchA = decayCoeffMs(pitchMs, sr);
  clickA = decayCoeffMs(clickMs, sr);
  trigPeriodSamples = (trigPeriodMs * sr) / 1000;
  sampleCounter = 0;
  ampEnv = pitchEnv = clickEnv = 0.0f;
  phase = phaseInc = 0.0f;
  DBG_PRINT("[Kick] sr=%d base=%.1fHz startX=%.1f A=%.0fms P=%.0fms C=%.0fms click=%.2f gain=%.2f period=%dms\n",
            sr, baseHz, startMult, ampMs, pitchMs, clickMs, clickAmt, outGain, trigPeriodMs);
}

void KickSynth::trigger() {
  ampEnv = 1.0f;
  pitchEnv = 1.0f;
  clickEnv = 1.0f;
  phase = 0.0f;

  DBG_PRINT("[Kick] TRIG #%lu @ %lu ms\n",
            (unsigned long)trigCounter++, (unsigned long)millis());
}

void KickSynth::fillBlock(int16_t* dst, int nFrames, int /*sampleRate*/) {
  for (int i = 0; i < nFrames; ++i) {
    if (onceIn(trigPeriodSamples)) {
      trigger();
      DBG_LED_GREEN_ON();
    }

    #ifdef DEBUG
    if (onceIn(trigPeriodSamples + ledBlinkSamples)) {
        DBG_LED_GREEN_OFF();
    }
    #endif

    // freq with decaying pitch
    float fNow = baseHz * (1.0f + (startMult - 1.0f) * pitchEnv);
    float targetInc = (2.0f * PI * fNow) / (float)sr;
    phaseInc += (targetInc - phaseInc) * 0.25f;
    phase += phaseInc;
    if (phase >= 2.0f * PI) phase -= 2.0f * PI;

    float s = sinf(phase);
    float click = clickAmt * clickEnv * (s >= 0 ? 1.0f : -1.0f);
    float y = softClip((ampEnv * s + click) * outGain);

    int16_t v = (int16_t)(y * 32767.0f);
    dst[2*i + 0] = v;  // L
    dst[2*i + 1] = v;  // R

    // envelopes
    ampEnv   *= ampA;
    pitchEnv *= pitchA;
    clickEnv *= clickA;
    if (ampEnv < 1e-6f) { 
        ampEnv = 0.0f; clickEnv = 0.0f; 
    }

    sampleCounter++;
  }
}