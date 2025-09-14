#include "CalcisHumilis.h"

#include <ArduinoLog.h>
#include <math.h>

using namespace audio_tools;

static inline float sgn_soft(float x) { return tanhf(8.0f * x); }

float CalcisHumilis::rateFromMs(float ms, int sr) {
  float samples = ms * (float)sr * 0.001f;
  if (samples < 1.0f) samples = 1.0f;
  return 1.0f / samples;  // per-sample step
}

float CalcisHumilis::softClip(float x) {
  const float t = 0.95f;
  if (x > t) return t + (x - t) * 0.05f;
  if (x < -t) return -t + (x + t) * 0.05f;
  return x;
}

static inline float softSign(float x) { return tanhf(8.0f * x); }

CalcisHumilis::CalcisHumilis(const CalcisConfig &cfg) : cfg_(cfg) {
  setConfig(cfg_);
}

void CalcisHumilis::setConfig(const CalcisConfig &cfg) {
  cfg_ = cfg;
  updatePanGains();
  applyEnvelopeRates();

  //   Log.notice(
  //       F("[Calcis] setConfig sr=%d A=%.0fms P=%.0fms C=%.0fms pan=%.2f" CR),
  //       cfg_.sampleRate, cfg_.ampMs, cfg_.pitchMs, cfg_.clickMs, cfg_.pan);
}

void CalcisHumilis::applyEnvelopeRates() {
  envAmp.setAttackRate(rateFromMs(cfg_.ampAttackMs, cfg_.sampleRate));
  envAmp.setDecayRate(rateFromMs(cfg_.ampMs, cfg_.sampleRate));
  envAmp.setSustainLevel(0.0f);
  envAmp.setReleaseRate(0.0f);

  envPitch.setAttackRate(rateFromMs(cfg_.pitchAttackMs, cfg_.sampleRate));
  envPitch.setDecayRate(rateFromMs(cfg_.pitchMs, cfg_.sampleRate));
  envPitch.setSustainLevel(0.0f);
  envPitch.setReleaseRate(0.0f);

  envClick.setAttackRate(rateFromMs(cfg_.clickAttackMs, cfg_.sampleRate));
  envClick.setDecayRate(rateFromMs(cfg_.clickMs, cfg_.sampleRate));
  envClick.setSustainLevel(0.0f);
  envClick.setReleaseRate(0.0f);
}

void CalcisHumilis::updatePanGains() {
  // equal-power: -1..+1 -> 0..pi/2
  const float t = (currentPan + 1.0f) * 0.5f * (PI * 0.5f);
  gainL = cosf(t);
  gainR = sinf(t);
}

void CalcisHumilis::trigger() {
  envAmp.keyOn(1.0f);
  envPitch.keyOn(1.0f);
  envClick.keyOn(1.0f);
  phase = 0.0f;
  triggerLED.FadeOff((uint32_t)cfg_.ampMs);
  currentPan = cfg_.pan;
}

void CalcisHumilis::tickLED() { triggerLED.Update(); }

static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x;
}

void CalcisHumilis::fillBlock(int32_t *dstLR, size_t nFrames) {
  // tight, branchless-ish inner loop; no virtual calls
  const float sr = static_cast<float>(cfg_.sampleRate);

  for (size_t i = 0; i < nFrames; ++i) {
    const float a = envAmp.tick();
    const float p = envPitch.tick();
    const float c = envClick.tick();

    const float fNow = cfg_.baseHz * (1.0f + (cfg_.startMult - 1.0f) * p);
    const float targetInc = (2.0f * PI * fNow) / sr;
    phaseInc += (targetInc - phaseInc) * 0.25f;
    phase += phaseInc;
    if (phase >= 2.0f * PI) {
      phase -= 2.0f * PI;
    }

    const float s = sinf(phase);
    const float click = cfg_.clickAmt * c * softSign(s);
    const float y = softClip((a * s + click) * cfg_.outGain);

    const float l = clamp01(y * gainL);
    const float r = clamp01(y * gainR);

    if (cfg_.kPack24In32) {
      // 24-bit data left-justified in 32-bit word
      const int32_t Li = static_cast<int32_t>(lrintf(l * 8388607.0f)) << 8;
      const int32_t Ri = static_cast<int32_t>(lrintf(r * 8388607.0f)) << 8;
      dstLR[2 * i + 0] = Li;
      dstLR[2 * i + 1] = Ri;
    } else {
      // Full-range 32-bit
      const int32_t Li = static_cast<int32_t>(lrintf(l * 2147483647.0f));
      const int32_t Ri = static_cast<int32_t>(lrintf(r * 2147483647.0f));
      dstLR[2 * i + 0] = Li;
      dstLR[2 * i + 1] = Ri;
    }
  }
}