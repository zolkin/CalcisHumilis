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
    : cfg_(cfg), fb_(fb) {}

template <class TR>
void CalcisHumilis<TR>::trigger() {
  envelopes_.triggerAll();
  swarm.reset();
}

template <size_t N>
static inline void array_float_to_int32(const std::array<float, N> &src,
                                        std::array<int32_t, N> &dst) {
  for (size_t i = 0; i < N; ++i) {
    float x = src[i] * 2147483647.0f;
    x = fmaxf(-2147483648.0f, fminf(2147483647.0f, x));
    dst[i] = (int32_t)x;
  }
}

template <class TR>
void CalcisHumilis<TR>::fillBlock(OutBuffer &destLR) {
  if (cfg_->trigCounter > trigCounter_) {
    trigCounter_ = cfg_->trigCounter;
    trigger();
  }

  const float g = cfg_->outGain;
  swarm.setConfig(cfg_->swarmOsc);
  envelopes_.setEnvs(cfg_->envs);

  IntBuffer buffer;
  for (size_t i = 0; i < TR::BLOCK_FRAMES; ++i) {
    envelopes_.update();

    // float outL = 0.0f, outR = 0.0f;

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

    float &l = buffer[2 * i + 0];
    float &r = buffer[2 * i + 1];
    l = r = 0.f;

    swarm.tickStereo(cyclesPerSample, sw, m, l, r);

    l = filterL.process(l, cfg_->filter);
    r = filterR.process(r, cfg_->filter);

    l = softClip(l * a * g);
    r = softClip(r * a * g);
  }

  if constexpr (TR::BITS == 24) {
    // dstLR[2 * i + 0] = (int32_t)lrintf(outL * 8388607.0f) << 8;
    // dstLR[2 * i + 1] = (int32_t)lrintf(outR * 8388607.0f) << 8;
  } else if constexpr (TR::BITS == 32) {
    array_float_to_int32(buffer, destLR);
  }
}

}  // namespace zlkm