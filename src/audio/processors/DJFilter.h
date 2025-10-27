#pragma once

#include <assert.h>

#include "dsp/SinCosPoly9.h"
#include "dsp/Util.h"
#include "math/Constants.h"
#include "math/Util.h"
#include "platform/platform.h"

namespace zlkm::audio::proc {

// ------- Safe, UI-side param gate (0..1 inputs) -------
// TODO: Move to parameter definition header?
struct DJFilterLimitsDefault {
  static constexpr float kAlpha = 0.8f;   // base top-end vs Nyquist
  static constexpr float kGamma = 0.72f;  // how much max cutoff shrinks with Q
  static constexpr float kQmin = 0.707f;  // "no resonance" ≈ Butterworth
  static constexpr float kQmax = 16.f * kQmin;  // musical max Q
  static constexpr float kCurve = 2.0f;   // res 0..1 -> Q perceptual curve
  static constexpr float kMinHz = 25.0f;  // practical low cutoff
  static constexpr float kHardTopHz = 16000.f;  // hard cap (<=0 disables)
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
  static constexpr float kSR = float(SR);
  static constexpr float kInvSR = 1.f / kSR;
  static constexpr float PiOverSR = zlkm::math::PI_F * kInvSR;
  static constexpr float kLeakMul = 1.0f - (1.0f / (kSR * 60.0f));

  using DLim = DJFilterLimitsDefault;
  struct Cfg {
    float cutoffHz = DLim::kHardTopHz;
    float Q = DLim::kQmin;  // kDamp = dsp::res01ToKDamp_smooth(0.f);
    float morph = 0.f;      // 0..1 (0=LP -> 1=HP), pre-shaped/limited outside

    static constexpr int PCOUNT = 3;

    std::array<float, PCOUNT> const& asTarget() const {
      return *reinterpret_cast<std::array<float, PCOUNT> const*>(this);
    }
  };

  struct Mod {
    float cutoffHz = 0.f;
    float Q = 0.f;
    float morph = 0.f;
  };

  void reset() {
    ic1eq_ = 0.0f;  // band-pass related state
    ic2eq_ = 0.0f;  // low-pass related state
  }

  inline float process(float sample, Cfg const& cfg, Mod const& mod) {
    // TPT SVF core (no guards)
    using namespace zlkm::math;
    using namespace zlkm::dsp;

    const float cutoffRaw = cfg.cutoffHz + mod.cutoffHz;
    const float QRaw = cfg.Q + mod.Q;

    const float Q = clamp(QRaw, DLim::kQmin, DLim::kQmax);
    const float kDamp = 2.f / Q;
    const float maxCutoffHz = fMaxFromKDamp_fast(kDamp);
    const float cutoffClamped = clamp(cutoffRaw, DLim::kMinHz, maxCutoffHz);
    const float gCut = gCutFromHz(cutoffClamped);

    const float a1 = 1.0f / (1.0f + gCut * (gCut + kDamp));

    const float v1 = (sample - ic2eq_ - kDamp * ic1eq_) * a1;  // hp proto
    const float v2 = gCut * v1 + ic1eq_;                       // bp
    const float v3 = gCut * v2 + ic2eq_;                       // lp

    ic1eq_ = (2.0f * v2 - ic1eq_) * kLeakMul;
    ic2eq_ = (2.0f * v3 - ic2eq_) * kLeakMul;

    const float morph = clamp01(cfg.morph + mod.morph);

    const float lpWeight = (1.0f - morph);
    const float hpWeight = morph;

    return lpWeight * v3 + hpWeight * v1;
  }

 private:
  // Fast stability cap: maximum safe cutoff (Hz) for given Q so that g <= τ·k
  static inline float fMaxFromKDamp_fast(float kDamp) {
    constexpr float SROverPi = kSR / zlkm::math::PI_F;
    const float z = DLim::kStabTau * kDamp;
    const float a = z / (1.0f + 0.28f * z * z);  // ~atan(z)
    return SROverPi * a;
  }

  static constexpr float gCutFromHz(float hz) { return tanf(hz * PiOverSR); }

  static constexpr float kDampFromQ(float Q) { return 2.f / Q; }

  float ic1eq_ = 0.0f;  // "integrator capacitor" 1 (≈ band state)
  float ic2eq_ = 0.0f;  // "integrator capacitor" 2 (≈ low state)
};

// Forward-declare your filter type so we can return its Cfg
template <int SR>
class DJFilterTPT;

}  // namespace zlkm::audio::proc