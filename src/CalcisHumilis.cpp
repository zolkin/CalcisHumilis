#include "CalcisHumilis.h"

#include <ArduinoLog.h>
#include <math.h>

using namespace audio_tools;

float CalcisHumilis::rateFromMs(float ms, int sr) {
  float samples = ms * (float)sr * 0.001f;
  if (samples < 1.0f) samples = 1.0f;
  return 1.0f / samples;  // per-sample step
}

float CalcisHumilis::softClip(float x) {
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

CalcisHumilis::CalcisHumilis(const CalcisConfig &cfg) : cfg_(cfg) {
  setConfig(cfg_);
}

void CalcisHumilis::setConfig(const CalcisConfig &cfg) {
  cfg_ = cfg;
  applyEnvelopeRates();
  gainSlew_.setTimeMs(cfg.gainSlewMs);
  osc.setConfig(cfg_.baseOsc);
  swarm.setConfig(cfg_.swarmOsc);
}

void CalcisHumilis::applyEnvelopeRates() {
  envAmp.setAttackRate(rateFromMs(cfg_.ampAttackMs, cfg_.getSampleRate()));
  envAmp.setDecayRate(rateFromMs(cfg_.ampMs, cfg_.getSampleRate()));
  envAmp.setSustainLevel(0.0f);
  envAmp.setReleaseRate(0.0f);

  envPitch.setAttackRate(rateFromMs(cfg_.pitchAttackMs, cfg_.getSampleRate()));
  envPitch.setDecayRate(rateFromMs(cfg_.pitchMs, cfg_.getSampleRate()));
  envPitch.setSustainLevel(0.0f);
  envPitch.setReleaseRate(0.0f);

  envClick.setAttackRate(rateFromMs(cfg_.clickAttackMs, cfg_.getSampleRate()));
  envClick.setDecayRate(rateFromMs(cfg_.clickMs, cfg_.getSampleRate()));
  envClick.setSustainLevel(0.0f);
  envClick.setReleaseRate(0.0f);
}

void CalcisHumilis::trigger() {
  envAmp.keyOn(1.0f);
  envPitch.keyOn(1.0f);
  envClick.keyOn(1.0f);

  triggerLED.FadeOff((uint32_t)cfg_.ampMs);
  osc.reset(cfg_.pan);
  swarm.reset();
}

void CalcisHumilis::tickLED() {
  triggerLED.Update();
  clippingLED.Update();
}

static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x;
}

void CalcisHumilis::fillBlock(int32_t *dstLR, size_t nFrames) {
  const float sr = cfg_.getSampleRate();
  gainSlew_.setTarget(cfg_.outGain);

  for (size_t i = 0; i < nFrames; ++i) {
    const float g = gainSlew_.tick();
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
        osc.tickStereo(pitch, l, r);
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