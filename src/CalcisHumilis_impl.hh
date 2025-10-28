#include <ArduinoLog.h>
#include <math.h>

#include "CalcisHumilis.h"
#include "mod/BlockInterpolator.h"

namespace zlkm::ch {

template <class TR>
CalcisHumilis<TR>::CalcisHumilis(const Cfg* cfg, Feedback* fb)
    : cfg_(cfg), fb_(fb), outGain_(cfg_->outGain), swarm(cfg->swarmOsc) {}

template <class TR>
void CalcisHumilis<TR>::trigger() {
  envelopes_.triggerAll();
  swarm.reset();
}

template <size_t N>
static inline void array_float_to_int32(const std::array<float, N>& src,
                                        std::array<int32_t, N>& dst) {
  for (size_t i = 0; i < N; ++i) {
    dst[i] = int32_t(src[i] * 2147483647.0f);
  }
}

template <size_t N>
static inline void array_float_to_int24(const std::array<float, N>& src,
                                        std::array<int32_t, N>& dst) {
  for (size_t i = 0; i < N; ++i) {
    dst[i] = int32_t(src[i] * 8388607.0f) << 8;
  }
}

template <class TR>
void CalcisHumilis<TR>::fillBlock(OutBuffer& destLR) {
  ZLKM_PERF_SCOPE("CalcisHumilis<TR>::fillBlock");

  using namespace zlkm::mod;

  if (cfg_->trigCounter > trigCounter_) {
    trigCounter_ = cfg_->trigCounter;
    trigger();
  }
  envelopes_.cfg() = cfg_->envs;

  auto swarmCfgItp = makeBlockInterpolator<TR::BLOCK_FRAMES>(
      swarm.cfg().i_begin(), cfg_->swarmOsc.asTarget());
  auto calcisCfgItp =
      makeBlockInterpolator<TR::BLOCK_FRAMES, 1>(&outGain_, {cfg_->outGain});
  auto filterCfgItp = makeBlockInterpolator<TR::BLOCK_FRAMES>(
      &fCfg_.cutoffHz, cfg_->filter.asTarget());
  auto driveItp =
      makeBlockInterpolator<TR::BLOCK_FRAMES, 1>(&driveGain_, {cfg_->drive});

  IntBuffer buffer;
  for (size_t i = 0; i < TR::BLOCK_FRAMES; ++i) {
    {
      ZLKM_PERF_SCOPE("envelopes");
      envelopes_.update();
    }

    {
      ZLKM_PERF_SCOPE("interpolators");
      swarmCfgItp.update();
      calcisCfgItp.update();
      filterCfgItp.update();
      driveItp.update();
      swarm.cfgUpdated();
    }

    const float g = outGain_;

    // ---------- No oversampling path ----------
    const float a = envelopes_.value(EnvAmp);

    float& l = buffer[2 * i + 0];
    float& r = buffer[2 * i + 1];
    l = r = 0.f;

    if (a < 1e-5) {
      filterL.reset();
      filterR.reset();
      swarm.mod() = typename Swarm::Mod{};
      fMod_ = typename Filter::Mod{};
      envelopes_.resetAll();
      driveGain_ = 1.0f;
      continue;
    }

    const float p = envelopes_.value(EnvPitch);
    const float c = envelopes_.value(EnvClick);
    const float sw = envelopes_.value(EnvSwarm);
    const float m = envelopes_.value(EnvMorph);
    const float f = envelopes_.value(EnvFilter);

    // TODO: manual modulation move to matrix later
    swarm.mod().cyclesPerSample = p * cyclesPerSample_;
    swarm.mod().detuneMul = math::interpolate(1.f, swarm.cfg().detuneMul, sw);
    swarm.mod().stereoSpread = sw;
    swarm.mod().morph = m;
    swarm.mod().pulseWidth = m;

    // test filter mod
    fMod_.cutoffHz = fCfg_.cutoffHz * f;
    fMod_.Q = fCfg_.Q * sw * 0.5;

    swarm.tickStereo(l, r);

    {
      // TODO: make flexible Processing chain
      ZLKM_PERF_SCOPE_SAMPLED("Processors", 6);
      const float drive = driveGain_ + p;
      l = Drive::process(filterL.process(l, fCfg_, fMod_), drive) * a * g;
      r = Drive::process(filterR.process(r, fCfg_, fMod_), drive) * a * g;
    }
  }

  {
    ZLKM_PERF_SCOPE_SAMPLED("array_float_to_int", 6);
    if constexpr (TR::BITS == 24) {
      array_float_to_int24(buffer, destLR);
    } else if constexpr (TR::BITS == 32) {
      array_float_to_int32(buffer, destLR);
    }
  }
}

}  // namespace zlkm::ch