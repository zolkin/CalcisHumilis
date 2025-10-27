#pragma once
#include <math.h>

#include <array>

#include "audio/source/MorphOsc.h"

namespace zlkm::audio::engine {

// ---------------- Swarm ----------------
template <int N, int SR>
class SwarmMorph {
  static constexpr int kMaxSwarmVoices = N;
  static constexpr float INV_SR_F = 1.f / float(SR);

 public:
  struct Cfg {
    // Some dirty tricks here!!
    static constexpr int INTERPOLATABLE_PARAMS = 6;
    float* i_begin() { return &cyclesPerSample; }
    std::array<float, INTERPOLATABLE_PARAMS> const& asTarget() const {
      return *reinterpret_cast<std::array<float, INTERPOLATABLE_PARAMS> const*>(
          &cyclesPerSample);
    }
    float cyclesPerSample = 200.0f / float(SR);  // base frequency
    float detuneMul = 1.2599f;  // spread per ring (p+-p*c, p+-2*p*c…)
    float stereoSpread = 0.6f;  // 0..1 width
    float gainBase = 0.6f;      // center weight: base^ring
    float morph = 0.166666f;    // 0=sine..1=saw morph
    float pulseWidth = 0.4f;    // square duty cycle

    // Set immediately
    int voices = 7;        // 1..N
    int morphMode;         // 0 -> Morph, 1 -> Switch between waveforms (debug)
    bool randomPhase = 1;  // randomize start phase, int
  };

  struct Mod {
    float cyclesPerSample = 0.f;  // base frequency
    float detuneMul = 0.f;        // multiply detune
    float stereoSpread = 0.f;     // add to 0..1
    float morph = 0.f;            // add to morph
    float pulseWidth = 0.f;       // add to duty cycle
  };

  explicit SwarmMorph(const Cfg& c) : cfg_(c) { cfgUpdated(); }

  void cfgUpdated() {
    for (int i = 0; i < cfg_.voices; ++i) {
      osc_.state[i].pulseWidth = cfg_.pulseWidth;
    }
  }

  void reset() {
    const int VN = cfg_.voices;
    seedDetune(VN);
    seedPanRing(VN);
    seedGains(VN);
    osc_.reset(cfg_.randomPhase);
  }

  // const float cyclesPerSample, const float swarmEnv,
  // const float morphEnv,
  inline void tickStereo(float& outL, float& outR) {
    ZLKM_PERF_SCOPE_SAMPLED("Swarm::tickStereo", 6);
    const int VN = cfg_.voices;
    const float c0 = cfg_.cyclesPerSample + mod_.cyclesPerSample;
    osc_.mode = (typename MorphOsc::Mode)cfg_.morphMode;
    std::array<float, N> oscOut{};

    {
      ZLKM_PERF_SCOPE_SAMPLED("detune", 6);
      for (int i = 0; i < VN; ++i) {
        osc_.state[i].cyclesPerSample = c0 * detuneMul_[i] * mod_.detuneMul;
        osc_.state[i].morph = math::clamp01(cfg_.morph + mod_.morph);
      }
    }

    {
      ZLKM_PERF_SCOPE_SAMPLED("oscillators", 6);
      osc_.tick(oscOut);
    }

    {
      ZLKM_PERF_SCOPE_SAMPLED("panning", 6);
      float L = 0.f, R = 0.f;

      updatePan(VN);

      for (int i = 0; i < VN; ++i) {
        L += gains_[i] * oscOut[i] * panL_[i];
        R += gains_[i] * oscOut[i] * panR_[i];
      }

      outL = L;
      outR = R;
    }
  }

  Cfg& cfg() { return cfg_; }
  Mod& mod() { return mod_; }

 private:
  // ---------------- helpers ----------------
  static inline float panGainL(float p) { return sqrtf(0.5f * (1.f - p)); }
  static inline float panGainR(float p) { return sqrtf(0.5f * (1.f + p)); }

  void seedDetune(int VN) {
    const float cents2semi = 0.01f;
    int idx = 0;
    float cur = VN & 1 ? 1.0f : cfg_.detuneMul;
    for (int k = 0; idx < VN; ++k) {
      detuneMul_[idx++] = cur;
      if (idx < VN) {
        detuneMul_[idx++] = 1.f / cur;
      }
      cur *= cfg_.detuneMul;
    }
  }

  static inline int ringIndexFor(int i, int VN) {
    if (VN & 1) {
      const int k = (i + 1) / 2;
      return (i & 1) ? +k : -k;
    }
    const int k = (i / 2) + 1;
    return (i & 1) ? -k : +k;
  }

  void seedPanRing(int VN) {
    if (VN == 1) {
      panL_[0] = panR_[0] = 0.f;
      return;
    }
    const int maxRing = (VN & 1) ? (VN - 1) / 2 : (VN / 2);
    const float fInvMaxRing = 1.f / float(maxRing);
    for (int i = 0; i < VN; ++i) {
      const int ring = ringIndexFor(i, VN);
      spreadRing_[i] = float(ring) * fInvMaxRing;
    }
  }

  void updatePan(int VN) {
    using namespace math;
    for (int i = 0; i < VN; ++i) {
      const float pNorm = spreadRing_[i];
      const float p = clamp01(pNorm * (cfg_.stereoSpread + mod_.stereoSpread));
      panL_[i] = panGainL(p);
      panR_[i] = panGainR(p);
    }
  }

  void seedGains(int VN) {
    float sum = 0.f;
    int idx = 0;
    if (VN & 1) {
      gains_[idx++] = 1.f;
      sum += 1.f;
    }
    float curGain = cfg_.gainBase;
    while (idx < VN) {
      gains_[idx++] = curGain;
      sum += curGain;
      if (idx < VN) {
        gains_[idx++] = curGain;
        sum += curGain;
      }
      curGain *= cfg_.gainBase;
    }
    const float inv = (sum > 0.f) ? 1.f / sum : 1.f;
    const float norm = inv / sqrtf(float(VN));
    for (int i = 0; i < VN; ++i) gains_[i] *= norm;
  }

 private:
  using MorphOsc = src::MorphOscN<N, SR>;

  Cfg cfg_;
  Mod mod_;
  MorphOsc osc_;

  std::array<float, N> spreadRing_{};

  std::array<float, N> detuneMul_{};
  std::array<float, N> gains_{};
  std::array<float, N> panL_{}, panR_{};
};

}  // namespace zlkm::audio::engine