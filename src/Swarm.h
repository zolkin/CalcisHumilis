#pragma once
#include <math.h>

#include <array>

#include "BaseOsc.h"  // zlkm::MorphOscN<N,SR>

namespace zlkm {

template <int SR>
struct SwarmConfig {
  int voices = 9;             // 1..N
  float detuneMul = 1.2599f;  // spread per ring (p+-p*c, p+-2*p*c…)
  float stereoSpread = 0.6f;  // 0..1 width
  float gainBase = 0.6f;      // center weight: base^ring
  bool randomPhase = true;    // randomize start phase
  float morph = 0.166666f;    // 0=sine..1=saw morph
  float pulseWidth = 0.4f;    // square duty cycle
};

// ---------------- Swarm ----------------
template <int N, int SR>
class SwarmMorph {
  static constexpr int kMaxSwarmVoices = N;
  static constexpr float INV_SR_F = 1.f / float(SR);

 public:
  explicit SwarmMorph(const SwarmConfig<SR>& c = SwarmConfig<SR>()) {
    setConfig(c);
  }

  void setConfig(const SwarmConfig<SR>& c) {
    cfg_ = c;
    cfg_.voices = zlkm::math::clamp(cfg_.voices, 1, N);
    osc_.pulseWidth.fill(cfg_.pulseWidth);
  }

  void reset() {
    const int VN = cfg_.voices;
    seedDetune(VN);
    seedPan(VN);
    seedGains(VN);
    osc_.reset(cfg_.randomPhase);
  }

  inline void tickStereo(const float cyclesPerSample, const float swarmEnv,
                         const float morphEnv, float& outL, float& outR) {
    const int VN = cfg_.voices;
    const float c0 = cyclesPerSample;

    for (int i = 0; i < VN; ++i) {
      osc_.cyclesPerSample[i] =
          c0 * math::interpolate(1.f, detuneMul_[i], swarmEnv);
      osc_.morph[i] = math::clamp(cfg_.morph + morphEnv, 0.f, 1.f);
    }

    osc_.tickFirst(cfg_.voices, tmp_);

    float L = 0.f, R = 0.f;
    // per sample (or control-rate), e in [0..1]
    const float kEqualPan = 0.70710678f;  // 1/sqrt(2)
    for (int i = 0; i < VN; ++i) {
      const float v = gains_[i] * tmp_[i];
      L += v * math::interpolate(kEqualPan, panL_[i], swarmEnv);
      R += v * math::interpolate(kEqualPan, panR_[i], swarmEnv);
    }
    outL = L;
    outR = R;
  }

  SwarmConfig<SR>& cfg() { return cfg_; }

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

  void seedPan(int VN) {
    if (VN == 1) {
      panL_[0] = panR_[0] = 0.f;
      return;
    }
    const int maxRing = (VN & 1) ? (VN - 1) / 2 : (VN / 2);
    const float fInvMaxRing = 1.f / float(maxRing);
    for (int i = 0; i < VN; ++i) {
      const int ring = ringIndexFor(i, VN);
      float pNorm = maxRing ? float(ring) * fInvMaxRing : 0.f;
      const float p = math::clamp(pNorm * cfg_.stereoSpread, -1.f, 1.f);
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
  SwarmConfig<SR> cfg_;
  zlkm::MorphOscN<N, SR> osc_;

  std::array<float, N> tmp_{};
  std::array<float, N> detuneMul_{};
  std::array<float, N> gains_{};
  std::array<float, N> panL_{}, panR_{};
};

}  // namespace zlkm