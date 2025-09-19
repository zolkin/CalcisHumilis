#pragma once
#include <math.h>

#include <array>

#include "BaseOsc.h"  // zlkm::MorphOscN<N,SR>

namespace zlkm {

template <int SR>
struct SwarmConfig {
  int voices = 3;             // 1..N
  float baseTuneHz = 65.0f;   // base tuning in Hz
  float detuneCents = 3.0f;   // spread per ring (±c, ±2c, …)
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

 public:
  explicit SwarmMorph(const SwarmConfig<SR>& c = SwarmConfig<SR>()) {
    setConfig(c);
  }

  void setConfig(const SwarmConfig<SR>& c) {
    cfg_ = c;
    cfg_.voices = zlkm::math::clamp(cfg_.voices, 1, N);
    osc_.morph.fill(cfg_.morph);
    osc_.pulseWidth.fill(cfg_.pulseWidth);
  }

  void reset() {
    const int VN = cfg_.voices;
    seedDetune(VN);
    seedPan(VN);
    seedGains(VN);
    osc_.reset(cfg_.randomPhase);
  }

  inline void tickStereo(const float totalPitchSemis, float& outL,
                         float& outR) {
    const int VN = cfg_.voices;

    // base freq in Hz
    const float baseMul = exp2f(totalPitchSemis * (1.f / 12.f));
    const float f0 = cfg_.baseTuneHz * baseMul;

    for (int i = 0; i < VN; ++i) {
      osc_.freqNowHz[i] = f0 * detuneMul_[i];
    }

    osc_.tickFirst(cfg_.voices, tmp_);

    float L = 0.f, R = 0.f;
    for (int i = 0; i < VN; ++i) {
      const float v = gains_[i] * tmp_[i];
      L += v * panL_[i];
      R += v * panR_[i];
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
    if (VN & 1) {
      detuneMul_[idx++] = 1.f;
    }  // center
    for (int k = 1; idx < VN; ++k) {
      const float semi = cfg_.detuneCents * k * cents2semi;
      const float mul = exp2f(semi * (1.f / 12.f));
      detuneMul_[idx++] = mul;
      if (idx < VN) detuneMul_[idx++] = 1.f / mul;
    }
    for (int i = VN; i < N; ++i) detuneMul_[i] = 0.f;
  }

  static inline int ringIndexFor(int i, int VN) {
    if (VN & 1) {
      if (i == 0) return 0;
      int k = (i + 1) / 2;
      return (i & 1) ? +k : -k;
    } else {
      int k = (i / 2) + 1;
      return (i & 1) ? -k : +k;
    }
  }

  void seedPan(int VN) {
    const int maxRing = (VN & 1) ? (VN - 1) / 2 : (VN / 2);
    for (int i = 0; i < VN; ++i) {
      const int ring = ringIndexFor(i, VN);
      float pNorm = (VN == 1) ? 0.f : (maxRing ? float(ring) / maxRing : 0.f);
      const float p = fminf(fmaxf(pNorm * cfg_.stereoSpread, -1.f), 1.f);
      panL_[i] = panGainL(p);
      panR_[i] = panGainR(p);
    }
    for (int i = VN; i < N; ++i) {
      panL_[i] = panR_[i] = 0.f;
    }
  }

  void seedGains(int VN) {
    float sum = 0.f;
    int idx = 0;
    if (VN & 1) {
      gains_[idx++] = 1.f;
      sum += 1.f;
    }
    for (int k = 1; idx < VN; ++k) {
      const float w = powf(cfg_.gainBase, float(k));
      gains_[idx++] = w;
      sum += w;
      if (idx < VN) {
        gains_[idx++] = w;
        sum += w;
      }
    }
    const float inv = (sum > 0.f) ? 1.f / sum : 1.f;
    const float norm = inv / sqrtf(float(VN));
    for (int i = 0; i < VN; ++i) gains_[i] *= norm;
    for (int i = VN; i < N; ++i) gains_[i] = 0.f;
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