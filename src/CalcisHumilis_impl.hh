#include <ArduinoLog.h>
#include <math.h>

#include "CalcisHumilis.h"

using namespace audio_tools;

template <int SR>
float CalcisHumilis<SR>::rateFromMs(float ms, int sr) {
  float samples = ms * (float)sr * 0.001f;
  if (samples < 1.0f) samples = 1.0f;
  return 1.0f / samples;  // per-sample step
}

template <int SR>
float CalcisHumilis<SR>::softClip(float x) {
  const float t = 0.95f;
  const bool isClip = (x > t) || (x < -t);
  if (isClip) {
    clippingLED.FadeOff(30);
  }
  if (x > t) {
    return t + (x - t) * 0.05f;
  }
  if (x < -t) {
    return -t + (x + t) * 0.05f;
  }
  return x;
}

static inline float softSign(float x) { return tanhf(8.0f * x); }

template <int SR>
CalcisHumilis<SR>::CalcisHumilis(const CalcisConfig<SR> &cfg) : cfg_(cfg) {
  setConfig(cfg_);
}

template <int SR>
void CalcisHumilis<SR>::setConfig(const CalcisConfig<SR> &cfg) {
  cfg_ = cfg;
  applyEnvelopeRates();
  gainSlew_.setTimeMsAll(cfg.gainSlewMs);
  osc.setConfig(cfg_.baseOsc);
  swarm.setConfig(cfg_.swarmOsc);
}

template <int SR>
void CalcisHumilis<SR>::applyEnvelopeRates() {
  envAmp.setAttackRate(rateFromMs(cfg_.ampAttackMs, SR));
  envAmp.setDecayRate(rateFromMs(cfg_.ampMs, SR));
  envAmp.setSustainLevel(0.0f);
  envAmp.setReleaseRate(0.0f);

  envPitch.setAttackRate(rateFromMs(cfg_.pitchAttackMs, SR));
  envPitch.setDecayRate(rateFromMs(cfg_.pitchMs, SR));
  envPitch.setSustainLevel(0.0f);
  envPitch.setReleaseRate(0.0f);

  envClick.setAttackRate(rateFromMs(cfg_.clickAttackMs, SR));
  envClick.setDecayRate(rateFromMs(cfg_.clickMs, SR));
  envClick.setSustainLevel(0.0f);
  envClick.setReleaseRate(0.0f);
}

template <int SR>
void CalcisHumilis<SR>::trigger() {
  envAmp.keyOn(1.0f);
  envPitch.keyOn(1.0f);
  envClick.keyOn(1.0f);

  triggerLED.FadeOff((uint32_t)cfg_.ampMs);
  osc.reset(cfg_.pan);
  swarm.reset();
}

template <int SR>
void CalcisHumilis<SR>::tickLED() {
  triggerLED.Update();
  clippingLED.Update();
}

static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x;
}

template <int SR>
void CalcisHumilis<SR>::fillBlock(int32_t *dstLR, size_t nFrames) {
  const float sr = SR;
  gainSlew_.setTarget(0, cfg_.outGain);

  for (size_t i = 0; i < nFrames; ++i) {
    const float g = gainSlew_.tick(0);
    const float a = envAmp.tick();
    const float p = envPitch.tick();
    const float c = envClick.tick();

    float s = 0.0f;

    float l, r;

    const float pitch =
        cfg_.pitchSemis + pitch::pitchToSemis(p * cfg_.startMult);

    switch (cfg_.oscMode) {
      case OscMode::Swarm:
        swarm.tickStereo(pitch, l, r);
        break;
      default:
        osc.tickStereo(0, pitch, l, r);
        break;
    }

    l = softClip((a * l) * g);
    r = softClip((a * r) * g);

    if (cfg_.kPack24In32) {
      dstLR[2 * i + 0] = (int32_t)lrintf(l * 8388607.0f) << 8;
      dstLR[2 * i + 1] = (int32_t)lrintf(r * 8388607.0f) << 8;
    } else {
      dstLR[2 * i + 0] = (int32_t)lrintf(l * 2147483647.0f);
      dstLR[2 * i + 1] = (int32_t)lrintf(r * 2147483647.0f);
    }
  }
}