#pragma once
#include "BaseOsc.h"  // includes BaseOscillator, MorphOsc, math::clamp, etc.

template <int SR>
struct SwarmConfig : public MorphConfigN<1, SR> {
  int voices = 2;             // 1..kMaxSwarmVoices
  float detuneCents = 1.0f;   // spread per "ring" in cents (±c, ±2c, ...)
  float stereoSpread = 1.0f;  // 0..1 width (0=center, 1=full L/R)
  float gainBase = 0.7f;      // center weight curve: w = gainBase^(ring)
  bool randomPhase = true;    // optional: randomize per-voice phase on reset
};

template <int SR>
struct OnePoleLP {
  float zL = 0, zR = 0;
  // fc in Hz
  void set(float fc) { a = expf(-2.0f * PI * fc / SR); }

  float a = 0.0f;

  inline void process(float& L, float& R) {
    zL += (1.0f - a) * (L - zL);
    zR += (1.0f - a) * (R - zR);
    L = zL;
    R = zR;
  }
};

template <int N, int SR, int OS>
class SwarmMorph {
  static constexpr int kMaxSwarmVoices = N;
  static constexpr int kEffectiveSR = SR * OS;

 public:
  explicit SwarmMorph(const SwarmConfig<SR>& c = SwarmConfig<SR>()) {
    lp_.set(0.45f * 0.5f * SR);
    setConfig(c);
  }

  void setConfig(const SwarmConfig<SR>& c) {
    cfg_ = c;
    // clamp safe ranges
    cfg_.voices = math::clamp(cfg_.voices, 1, N);
    cfg_.stereoSpread = math::clamp(cfg_.stereoSpread, 0.0f, 1.0f);
    cfg_.pulseWidth[0] = math::clamp(cfg_.pulseWidth[0], 0.01f, 0.99f);

    // push common OscConfig fields to children
    MorphConfigN<N, kEffectiveSR> oc;
    oc.baseTuneHz.fill(cfg_.baseTuneHz[0]);
    oc.pulseWidth.fill(cfg_.pulseWidth[0]);  // ← key line
    oc.initialPan.fill(0.0f);                // we set per-voice pan at reset()
    voices_.setConfig(oc);
  }

  OnePoleLP<SR * OS> lp_;

  // Reset: seed per-voice detune, pan, gains; latch phases/pans.
  void reset() {
    const int VN = cfg_.voices;
    seedDetuneCents(VN);
    seedPan(VN);
    seedGains(VN);

    for (int i = 0; i < VN; ++i) {
      voices_.reset(i, voices_.pan(i),
                    !cfg_.randomPhase ? 0.f : rand01() * TWO_PI);
    }
  }

  // One-sample stereo render.
  // totalPitchSemis: your summed pitch (knob + env + CV) in semitones.
  inline void tickStereo(const float totalPitchSemis, float& outL,
                         float& outR) {
    const int VN = cfg_.voices;
    float L = 0.f, R = 0.f;
    // render and filter at OS rate
    for (int j = 0; j < OS; ++j) {
      float accL = 0.f, accR = 0.f;
      for (int i = 0; i < VN; ++i) {
        float vL, vR;
        voices_.tickStereo(i, totalPitchSemis + detuneSemis_[i], vL, vR);
        accL += gains_[i] * vL;
        accR += gains_[i] * vR;
      }
      lp_.process(accL, accR);  // LPF runs at SR*OS
      L = accL;
      R = accR;  // keep last filtered subsample (decimate-by-pick)
      // (Option: accumulate and average OS subsamples after filtering; see
      // below)
    }
    outL = L;
    outR = R;
  }

  // public knobs you might tweak live
  float smoothingAlpha_ = 0.25f;  // forwarded to children on reset()
  SwarmConfig<SR>& cfg() { return cfg_; }

 private:
  SwarmConfig<SR> cfg_;
  MorphOscN<kMaxSwarmVoices, SR * OS> voices_;

  std::array<float, N> detuneSemis_ = {0};  // per-voice detune in semitones
  std::array<float, N> gains_ = {0.7f};

  // ---------- helpers ----------

  static inline float rand01() {
    static uint32_t rng = 0x6d5fca4b;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng >> 8) * (1.0f / 16777216.0f);
  }

  // symmetric cents table → semitone offsets: {0, +c, -c, +2c, -2c, ...}
  void seedDetuneCents(int VN) {
    const float c2s = 0.01f;  // cents → semitones
    int idx = 0;
    if (VN & 1) detuneSemis_[idx++] = 0.0f;  // center voice for odd N
    for (int k = 1; idx < VN; ++k) {
      const float s = (cfg_.detuneCents * k) * c2s;
      detuneSemis_[idx++] = +s;
      if (idx < VN) detuneSemis_[idx++] = -s;
    }
    // for even N, we started at +s/-s (no center) — fine for symmetry
  }

  // equal-power pan positions across [-spread, +spread] with center priority
  void seedPan(int VN) {
    // map voice index order to ring order (0, +1, -1, +2, -2, ...)
    int order[kMaxSwarmVoices];
    int idx = 0;
    if (VN & 1) order[idx++] = 0;
    for (int k = 1; idx < VN; ++k) {
      order[idx++] = +k;
      if (idx < VN) order[idx++] = -k;
    }

    // normalized pan slots from -1..+1 scaled by stereoSpread
    // center (0) = 0, ±1 rings spread outward linearly
    const float spread = cfg_.stereoSpread;
    for (int i = 0; i < VN; ++i) {
      const int ring = order[i];
      float p = 0.0f;
      if (VN == 1)
        p = 0.0f;
      else {
        // max ring index for this N
        const int maxRing = (VN & 1) ? (VN - 1) / 2 : (VN / 2);
        p = (maxRing ? float(ring) / float(maxRing) : 0.0f);
      }
      voices_.setPan(i, math::clamp(p * spread, -1.0f, 1.0f));
    }
  }

  // center-weighted gains: w = base^(ring), normalized sum=1
  void seedGains(int VN) {
    float wSum = 0.0f;
    int idx = 0;
    if (VN & 1) {
      gains_[idx++] = 1.0f;  // center ring=0 => base^0 = 1
      wSum += 1.0f;
    }
    for (int k = 1; idx < VN; ++k) {
      const float w = powf(cfg_.gainBase, float(k));  // ring k weight
      // two voices per ring (±k)
      gains_[idx++] = w;
      wSum += w;
      if (idx < VN) {
        gains_[idx++] = w;
        wSum += w;
      }
    }
    // normalize to sum=1
    const float inv = (wSum > 0.0f) ? (1.0f / wSum) : 1.0f;
    for (int i = 0; i < VN; ++i) gains_[i] *= inv;
  }
};