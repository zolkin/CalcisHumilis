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
  envAmp.setSustainLevel(0.0f);
  envAmp.setReleaseRate(0.0f);
  envPitch.setSustainLevel(0.0f);
  envPitch.setReleaseRate(0.0f);
  envClick.setSustainLevel(0.0f);
  envClick.setReleaseRate(0.0f);
  envSwarm.setReleaseRate(0.0f);
  envMorph.setReleaseRate(0.0f);

  gainSlew_.setTimeMsAll(SLEW_MS_ALL);  // configure this once (!!!)
}

template <class TR>
void CalcisHumilis<TR>::applyEnvelopeRates() {
  envAmp.setAttackRate(cfg_->ampAtt);
  envAmp.setDecayRate(cfg_->ampDec);

  envPitch.setAttackRate(cfg_->pitchAtt);
  envPitch.setDecayRate(cfg_->pitchDec);

  envClick.setAttackRate(cfg_->clickAtt);
  envClick.setDecayRate(cfg_->clickDec);

  envSwarm.setAttackRate(cfg_->swarmAtt);
  envSwarm.setDecayRate(cfg_->swarmDec);

  envMorph.setAttackRate(cfg_->morphAtt);
  envMorph.setDecayRate(cfg_->morphDec);
}

template <class TR>
void CalcisHumilis<TR>::trigger() {
  envAmp.keyOn(1.f);
  envPitch.keyOn(1.f);
  envClick.keyOn(1.f);
  envSwarm.keyOn(1.f);
  envMorph.keyOn(1.f);
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

  gainSlew_.setTarget(0, cfg_->outGain);
  swarm.setConfig(cfg_->swarmOsc);
  applyEnvelopeRates();

  for (size_t i = 0; i < nFrames; ++i) {
    // Controls advance at base rate (one step per output frame)

    float outL = 0.0f, outR = 0.0f;

    if constexpr (OS == 1) {
      // ---------- No oversampling path ----------
      const float g = gainSlew_.tick(0);
      const float a = envAmp.tick();

      const float p = envPitch.tick();
      const float c = envClick.tick();
      const float sw = envSwarm.tick();
      const float m = cfg_->morphAmt * envMorph.tick();

      const float cyclesPerSample =
          cfg_->cyclesPerSample * (1.f + p * cfg_->pitchDepthMult);
      float l = 0.f, r = 0.f;
      switch (cfg_->oscMode) {
        case OscMode::Swarm:
          swarm.tickStereo(cyclesPerSample, sw, m, l, r);
          break;
        default:
          swarm.tickStereo(cyclesPerSample, sw, m, l, r);
          break;
      }
      l = filterL.process(l, cfg_->filterGCut, cfg_->filterKDamp,
                          cfg_->filterMorph, cfg_->filterDrive);
      r = filterR.process(r, cfg_->filterGCut, cfg_->filterKDamp,
                          cfg_->filterMorph, cfg_->filterDrive);
      l = softClip(l * a * g);
      r = softClip(r * a * g);
      outL = l;
      outR = r;

    } else {
      // ---------- Oversampled path: run osc at OS*SR, FIR at OS*SR, decimate
      // ----------
      float l_os = 0.f, r_os = 0.f;
      float l_f = 0.f, r_f = 0.f;

      // Zero-order hold for control values within the frame
      for (int k = 0; k < OS; ++k) {
        const float g = gainSlew_.tick(0);
        const float a = envAmp.tick();
        const float p = envPitch.tick();
        const float c = envClick.tick();  // currently unused
        const float sw = envSwarm.tick();
        const float m = envMorph.tick();

        const float pitchSemis =
            cfg_->pitchSemis + zlkm::pitch::pitchToSemis(p * cfg_->startMult);
        // Generate at OS*SR
        switch (cfg_->oscMode) {
          case OscMode::Swarm:
            swarm.tickStereo(pitchSemis, sw, m, l_os, r_os);
            break;
          default:
            swarm.tickStereo(pitchSemis, sw, m, l_os, r_os);
            break;
        }
        // Apply amplitude & output gain at OS-rate (still ZOH controls)
        l_os *= (a * g);
        r_os *= (a * g);

        // Soft clip pre/post filter? Prefer post-filter to avoid distorting
        // FIR. We'll do gentle soft clip *after* decimation; here feed FIR raw.
        float l_y, r_y;
        osDecim_.fir.tickStereo(l_os, r_os, l_y, r_y);

        // Take every OS-th filtered sample
        if (k == OS - 1) {
          outL = l_y;
          outR = r_y;
        }
      }
      // Final soft clip after anti-alias low-pass
      outL = softClip(outL);
      outR = softClip(outR);
    }

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