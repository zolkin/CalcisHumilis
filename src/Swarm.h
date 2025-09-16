#pragma once
#include "BaseOsc.h"  // includes BaseOscillator, MorphOsc, math::clamp, etc.

static constexpr int kMaxSwarmVoices = 10;

struct SwarmConfig : public MorphConfig {
  int voices = 5;             // 1..kMaxSwarmVoices
  float detuneCents = 1.0f;   // spread per "ring" in cents (±c, ±2c, ...)
  float stereoSpread = 1.0f;  // 0..1 width (0=center, 1=full L/R)
  float gainBase = 0.7f;      // center weight curve: w = gainBase^(ring)
  bool randomPhase = true;    // optional: randomize per-voice phase on reset
};

class SwarmMorph {
 public:
  explicit SwarmMorph(const SwarmConfig& c = SwarmConfig()) { setConfig(c); }

  void setConfig(const SwarmConfig& c) {
    cfg_ = c;
    // clamp safe ranges
    cfg_.voices =
        (cfg_.voices < 1)
            ? 1
            : (cfg_.voices > kMaxSwarmVoices ? kMaxSwarmVoices : cfg_.voices);
    cfg_.stereoSpread = math::clamp(cfg_.stereoSpread, 0.0f, 1.0f);
    cfg_.pulseWidth = math::clamp(cfg_.pulseWidth, 0.01f, 0.99f);

    // push common OscConfig fields to children
    OscConfig oc;
    oc.sampleRate = cfg_.sampleRate;
    oc.baseTuneHz = cfg_.baseTuneHz;
    oc.initialPan = 0.0f;  // we set per-voice pan at reset()
    for (int i = 0; i < kMaxSwarmVoices; ++i) {
      voices_[i].setConfig(
          mergeToMorph(oc));  // morph inherits OscConfig fields
    }
  }

  // Reset: seed per-voice detune, pan, gains; latch phases/pans.
  void reset() {
    const int N = cfg_.voices;
    seedDetuneCents(N);
    seedPan(N);
    seedGains(N);

    for (int i = 0; i < N; ++i) {
      const float pan = pan_[i];
      voices_[i].reset(pan);
      // lock smoothing to parent’s preference if you want:
      voices_[i].smoothingAlpha_ = smoothingAlpha_;
      // optional random phase jitter:
      if (cfg_.randomPhase) {
        // tiny phase nudge via immediate advance (cheap & deterministic)
        voices_[i].phase = (rand01() * 2.0f * PI);
      }
    }
  }

  // One-sample stereo render.
  // totalPitchSemis: your summed pitch (knob + env + CV) in semitones.
  inline void tickStereo(float totalPitchSemis, float& outL, float& outR) {
    const int N = cfg_.voices;
    outL = 0.0f;
    outR = 0.0f;
    for (int i = 0; i < N; ++i) {
      float L, R;
      // pass morph via cfg_.morph stored inside each MorphOsc's config
      voices_[i].cfg_.morph = cfg_.morph;            // minimal: share one morph
      voices_[i].cfg_.pulseWidth = cfg_.pulseWidth;  // keep PW in sync
      voices_[i].tickStereo(totalPitchSemis + detuneSemis_[i], L, R);
      outL += gains_[i] * L;
      outR += gains_[i] * R;
    }
  }

  // public knobs you might tweak live
  float smoothingAlpha_ = 0.25f;  // forwarded to children on reset()
  SwarmConfig& cfg() { return cfg_; }

 private:
  SwarmConfig cfg_;
  MorphOsc voices_[kMaxSwarmVoices];

  float detuneSemis_[kMaxSwarmVoices] = {0};  // per-voice detune in semitones
  float gains_[kMaxSwarmVoices] = {0};        // per-voice linear gain (sum=1)
  float pan_[kMaxSwarmVoices] = {0};          // per-voice pan (-1..+1)

  // ---------- helpers ----------
  // Convert base OscConfig to MorphConfig fields for child oscs
  static MorphConfig mergeToMorph(const OscConfig& oc) {
    MorphConfig mc;
    static_cast<OscConfig&>(mc) = oc;  // slice assign common fields
    // mc.morph/pulseWidth left as-is; we set them before ticking
    return mc;
  }

  static inline float rand01() {
    static uint32_t rng = 0x6d5fca4b;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng >> 8) * (1.0f / 16777216.0f);
  }

  // symmetric cents table → semitone offsets: {0, +c, -c, +2c, -2c, ...}
  void seedDetuneCents(int N) {
    const float c2s = 0.01f;  // cents → semitones
    int idx = 0;
    if (N & 1) detuneSemis_[idx++] = 0.0f;  // center voice for odd N
    for (int k = 1; idx < N; ++k) {
      const float s = (cfg_.detuneCents * k) * c2s;
      detuneSemis_[idx++] = +s;
      if (idx < N) detuneSemis_[idx++] = -s;
    }
    // for even N, we started at +s/-s (no center) — fine for symmetry
  }

  // equal-power pan positions across [-spread, +spread] with center priority
  void seedPan(int N) {
    // map voice index order to ring order (0, +1, -1, +2, -2, ...)
    int order[kMaxSwarmVoices];
    int idx = 0;
    if (N & 1) order[idx++] = 0;
    for (int k = 1; idx < N; ++k) {
      order[idx++] = +k;
      if (idx < N) order[idx++] = -k;
    }

    // normalized pan slots from -1..+1 scaled by stereoSpread
    // center (0) = 0, ±1 rings spread outward linearly
    const float spread = cfg_.stereoSpread;
    for (int i = 0; i < N; ++i) {
      const int ring = order[i];
      float p = 0.0f;
      if (N == 1)
        p = 0.0f;
      else {
        // max ring index for this N
        const int maxRing = (N & 1) ? (N - 1) / 2 : (N / 2);
        p = (maxRing ? float(ring) / float(maxRing) : 0.0f);
      }
      pan_[i] = math::clamp(p * spread, -1.0f, 1.0f);
    }
  }

  // center-weighted gains: w = base^(ring), normalized sum=1
  void seedGains(int N) {
    float wSum = 0.0f;
    int idx = 0;
    if (N & 1) {
      gains_[idx++] = 1.0f;  // center ring=0 => base^0 = 1
      wSum += 1.0f;
    }
    for (int k = 1; idx < N; ++k) {
      const float w = powf(cfg_.gainBase, float(k));  // ring k weight
      // two voices per ring (±k)
      gains_[idx++] = w;
      wSum += w;
      if (idx < N) {
        gains_[idx++] = w;
        wSum += w;
      }
    }
    // normalize to sum=1
    const float inv = (wSum > 0.0f) ? (1.0f / wSum) : 1.0f;
    for (int i = 0; i < N; ++i) gains_[i] *= inv;
  }
};