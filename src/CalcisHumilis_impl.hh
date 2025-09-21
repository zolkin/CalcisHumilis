#include <ArduinoLog.h>
#include <math.h>

#include "CalcisHumilis.h"

namespace zlkm {

template <class TR>
float CalcisHumilis<TR>::softClip(float x) {
  const float t = 0.95f;
  const bool isClip = (x > t) || (x < -t);
  if (isClip) ++fb_->saturationCounter;
  if (x > t) return t + (x - t) * 0.05f;
  if (x < -t) return -t + (x + t) * 0.05f;
  return x;
}

template <class TR>
CalcisHumilis<TR>::CalcisHumilis(const Cfg *cfg, Feedback *fb)
    : cfg_(cfg), fb_(fb) {
  if constexpr (OS > 1) osDecim_.setup();
}

template <class TR>
void CalcisHumilis<TR>::trigger() {
  envelopes_.triggerAll();
  swarm.reset();

  if constexpr (OS > 1) osDecim_.reset();
}

static inline float clamp01(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x;
}

template <class TR>
void CalcisHumilis<TR>::fillBlock(int32_t *dstLR, size_t nFrames) {
  if (cfg_->trigCounter > trigCounter_) {
    trigCounter_ = cfg_->trigCounter;
    trigger();
  }

  const float g = cfg_->outGain;
  swarm.setConfig(cfg_->swarmOsc);
  envelopes_.setEnvs(cfg_->envs);

  for (size_t i = 0; i < nFrames; ++i) {
    // Controls advance at base rate (one step per output frame)
    envelopes_.update();
    float outL = 0.0f, outR = 0.0f;

    // ---------- No oversampling path ----------
    const float a = envelopes_.value(EnvAmp);

    if (a < 1e-5) {
      filterL.reset();
      filterR.reset();
    }

    const float p = envelopes_.value(EnvPitch);
    const float c = envelopes_.value(EnvClick);
    const float sw = envelopes_.value(EnvSwarm);
    const float m = envelopes_.value(EnvMorph);
    const float f = envelopes_.value(EnvFilter);

    const float cyclesPerSample = cfg_->cyclesPerSample * (1.f + p);
    float l = 0.f, r = 0.f;
    switch (cfg_->oscMode) {
      case OscSwarm:
        swarm.tickStereo(cyclesPerSample, sw, m, l, r);
        break;
      default:
        swarm.tickStereo(cyclesPerSample, sw, m, l, r);
        break;
    }

    l = filterL.process(l, cfg_->filter);
    r = filterR.process(r, cfg_->filter);

    l = softClip(l * a * g);
    r = softClip(r * a * g);
    outL = l;
    outR = r;

    if (cfg_->kPack24In32) {
      dstLR[2 * i + 0] = (int32_t)lrintf(outL * 8388607.0f) << 8;
      dstLR[2 * i + 1] = (int32_t)lrintf(outR * 8388607.0f) << 8;
    } else {
      dstLR[2 * i + 0] = (int32_t)lrintf(outL * 2147483647.0f);
      dstLR[2 * i + 1] = (int32_t)lrintf(outR * 2147483647.0f);
    }
  }
}

}  // namespace zlkm