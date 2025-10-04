#pragma once
#include <Arduino.h>
#include <math.h>

#include "..\dsp\Util.h"
#include "..\math\Util.h"

namespace zlkm::audio {

// ------- Safe, UI-side param gate (0..1 inputs) -------
struct DJFilterLimitsDefault {
  static constexpr float kAlpha = 0.8f;   // base top-end vs Nyquist
  static constexpr float kGamma = 0.72f;  // how much max cutoff shrinks with Q
  static constexpr float kQmin = 0.707f;  // "no resonance" ≈ Butterworth
  static constexpr float kQmax = 10.0f;   // musical max Q
  static constexpr float kCurve = 2.0f;   // res 0..1 -> Q perceptual curve
  static constexpr float kMinHz = 20.0f;  // practical low cutoff
  static constexpr float kHardTopHz = 16000.f;  // hard cap (<=0 disables)
  static constexpr float kTrimStrength = 0.7f;  // auto-trim vs Q (0..~1.5)
  static constexpr float kDriveMax = 14.0f;     // UI drive top; UI min is unity
  static constexpr float kStabTau = 0.95f;  // stability margin τ (0.9..0.98)
  static constexpr float kBassMax = 2.f;    // up to 200% extra low-end
};

// -----------------------------------------------------------------------------
// DJ-style morphable filter (LP <-> HP) with resonance + drive
// TPT SVF core (stable at high Q), zero checks/clamps in the audio path.
// Provide sanitized parameters from your UI/control thread.
// -----------------------------------------------------------------------------
template <int SR>
class DJFilterTPT {
 public:
  struct Cfg {
    float gCut = dsp::hzToGCut<SR>(DJFilterLimitsDefault::kHardTopHz);
    float kDamp = dsp::res01ToKDamp_smooth(0.f);
    float lpWeight = 1.f;  // low-pass contribution (Q-compensated)
    float hpWeight = 0.f;  // high-pass contribution
    float drive = 1.f;

    static constexpr int PCOUNT = 5;

    std::array<float, PCOUNT> const& asTarget() const {
      return *reinterpret_cast<std::array<float, PCOUNT> const*>(this);
    }
  };

  void reset(float low = 0.0f, float band = 0.0f) {
    ic1eq_ = low;   // band-pass related state
    ic2eq_ = band;  // low-pass related state
  }

  // sample: input sample
  // gCut:  cutoff prewarp coefficient (g = tan(pi * f / SR))
  // kDamp: damping term (k = 2 / Q)
  // morph: 0..1 (0=LP -> 1=HP), pre-shaped/limited outside
  // drive: post-filter gain before soft clip, pre-limited outside
  inline float process(float sample, Cfg const& cfg) {
    // TPT SVF core (no guards)
    const float a1 = 1.0f / (1.0f + cfg.gCut * (cfg.gCut + cfg.kDamp));

    const float v1 = (sample - ic2eq_ - cfg.kDamp * ic1eq_) * a1;  // hp proto
    const float v2 = cfg.gCut * v1 + ic1eq_;                       // bp
    const float v3 = cfg.gCut * v2 + ic2eq_;                       // lp

    static constexpr float kLeakMul = 1.0f - (1.0f / (SR * 60.0f));
    ic1eq_ = (2.0f * v2 - ic1eq_) * kLeakMul;
    ic2eq_ = (2.0f * v3 - ic2eq_) * kLeakMul;

    float y = cfg.lpWeight * v3 + cfg.hpWeight * v1;

    y *= cfg.drive;

    // cheap soft clip (no guards)
    return y / (1.0f + fabsf(y));
  }

 private:
  float ic1eq_ = 0.0f;  // "integrator capacitor" 1 (≈ band state)
  float ic2eq_ = 0.0f;  // "integrator capacitor" 2 (≈ low state)
};

// Forward-declare your filter type so we can return its Cfg
template <int SR>
class DJFilterTPT;

// ---------- Safe, UI-side parameter gate (0..1 inputs) ----------
template <int SR, class Limits = DJFilterLimitsDefault>
class SafeFilterParams {
 public:
  static constexpr float kSR = float(SR);
  static constexpr float kNyq = 0.5f * kSR;

  // Init from 0..1 controls (drive01=0 => drive=1.0 by design)
  SafeFilterParams(float cutoff01 = 1.0f, float res01 = 0.f,
                   float drive01 = 0.f, float morph01 = 0.f) {
    setAll01(cutoff01, res01, drive01, morph01);
  }

  // --- individual 0..1 setters (mutual safety enforced) ---
  void setCutoff01(float v) { setAll01(v, res01_, drive01_, morph01_); }
  void setRes01(float v) { setAll01(cutoff01_, v, drive01_, morph01_); }
  void setDrive01(float v) { setAll01(cutoff01_, res01_, v, morph01_); }
  void setMorph01(float v) { setAll01(cutoff01_, res01_, drive01_, v); }

  // --- batch update (also 0..1) ---
  void setAll01(float cutoff01, float res01, float drive01, float morph01) {
    cutoff01_ = clamp01(cutoff01);
    res01_ = clamp01(res01);
    drive01_ = clamp01(drive01);
    morph01_ = clamp01(morph01);

    // Map res01 -> target Q with perceptual curve
    float Qt = QFromRes01(res01_);

    // Cutoff max allowed by that Q; map cutoff01 into [kMinHz..fcap]
    const float fcap = fMaxFromQ(Qt);
    float cutoffHz = Limits::kMinHz + cutoff01_ * (fcap - Limits::kMinHz);

    // Given cutoff, cap Q if needed; reflect clamp back to res01 domain
    const float Qcap = QmaxFromHz(cutoffHz);
    if (Qt > Qcap) {
      Qt = Qcap;
      res01_ = res01FromQ(Qt);
    }
    float Q = clampf(Qt, Limits::kQmin, Limits::kQmax);

    // Derived audio scalars
    cfg.gCut = tanf(float(M_PI) * cutoffHz / kSR);
    cfg.kDamp = 2.f / Q;

    const float Qnorm = (Q - Limits::kQmin) / (Limits::kQmax - Limits::kQmin);
    float cutoffFade = math::smoothstep(200.f, 600.f, cutoffHz);
    float bassBoost = 1.f + Limits::kBassMax * Qnorm * cutoffFade;
    cfg.lpWeight = (1.f - morph01_) * bassBoost;
    cfg.hpWeight = morph01_;
    float lpWeight = (1.0f - morph01) * bassBoost;
    float hpWeight = morph01;

    cfg.lpWeight = lpWeight;
    cfg.hpWeight = hpWeight;

    // Drive: UI 0..1 -> [1..kDriveMax], then auto-trim vs Q (may dip below 1)
    const float driveUI = 1.f + drive01_ * (Limits::kDriveMax - 1.f);
    const float trim = 1.f / (1.f + Limits::kTrimStrength * (Q - 1.f));
    cfg.drive = driveUI * trim;
  }

  typename DJFilterTPT<SR>::Cfg cfg;

 private:
  // --- helpers ---
  static inline float clamp01(float v) {
    return (v < 0.f) ? 0.f : ((v > 1.f) ? 1.f : v);
  }
  static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
  }

  static inline float fMaxFromQ_alphaGamma(float Q) {
    const float base =
        (Limits::kAlpha * kNyq) / (1.f + Limits::kGamma * (Q - 1.f));
    return (Limits::kHardTopHz > 0.f) ? fminf(base, Limits::kHardTopHz) : base;
  }

  // NEW: strict stability cutoff cap (enforces gCut <= τ·k)
  static inline float fMaxFromK_stable(float k) {
    // f_stab(k) = (SR/π) * atan(τ * k)
    const float t = Limits::kStabTau * k;
    const float f = (kSR / float(M_PI)) * atanf(t);
    // also respect practical floor:
    return fmaxf(f, Limits::kMinHz);
  }

  static inline float QFromRes01(float r) {
    const float t = powf(clamp01(r), Limits::kCurve);
    return Limits::kQmin * powf(Limits::kQmax / Limits::kQmin, t);
  }
  static inline float res01FromQ(float Q) {
    Q = clampf(Q, Limits::kQmin, Limits::kQmax);
    const float num = logf(Q / Limits::kQmin);
    const float den = logf(Limits::kQmax / Limits::kQmin);
    const float t = (den > 0.f) ? (num / den) : 0.f;
    return powf(clampf(t, 0.f, 1.f), 1.f / Limits::kCurve);
  }

  static inline float fMaxFromQ(float Q) {
    const float k = 2.f / Q;
    const float f_perc = fMaxFromQ_alphaGamma(Q);
    const float f_stab = fMaxFromK_stable(k);
    return fminf(f_perc, f_stab);
  }

  // Replace your QmaxFromHz with this:
  static inline float QmaxFromHz(float hz) {
    hz = (hz <= 0.f) ? Limits::kMinHz : hz;
    // Your existing UI/perceptual cap (keep your feel)
    const float Q_ui =
        clampf(1.f + (((Limits::kAlpha * kNyq) / hz) - 1.f) / Limits::kGamma,
               Limits::kQmin, Limits::kQmax);

    // Stability cap: Q_stab = 2τ / gCut
    const float g = tanf(float(M_PI) * hz / kSR);
    const float Q_stab = 2.f * Limits::kStabTau / fmaxf(g, 1e-20f);

    return fminf(Q_ui, clampf(Q_stab, Limits::kQmin, Limits::kQmax));
  }

  // --- stored normals (0..1) ---
  float cutoff01_ = 0.f, res01_ = 0.f, drive01_ = 0.f, morph01_ = 0.f;
};

}  // namespace zlkm