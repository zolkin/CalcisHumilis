#pragma once
#include <Arduino.h>
#include <math.h>

// -----------------------------------------------------------------------------
// DJ-style morphable filter (LP <-> HP) with resonance + drive
// TPT SVF core (stable at high Q), zero checks/clamps in the audio path.
// Provide sanitized parameters from your UI/control thread.
// -----------------------------------------------------------------------------
template <int SR>
class DJFilterTPT {
 public:
  void reset(float low = 0.0f, float band = 0.0f) {
    ic1eq_ = low;   // band-pass related state
    ic2eq_ = band;  // low-pass related state
  }

  // sample: input sample
  // gCut:  cutoff prewarp coefficient (g = tan(pi * f / SR))
  // kDamp: damping term (k = 2 / Q)
  // morph: 0..1 (0=LP -> 1=HP), pre-shaped/limited outside
  // drive: post-filter gain before soft clip, pre-limited outside
  inline float process(float sample, float gCut, float kDamp, float morph,
                       float drive) {
    // TPT SVF core (no guards)
    const float a1 = 1.0f / (1.0f + gCut * (gCut + kDamp));

    const float v1 = (sample - ic2eq_ - kDamp * ic1eq_) * a1;  // hp proto
    const float v2 = gCut * v1 + ic1eq_;                       // bp
    const float v3 = gCut * v2 + ic2eq_;                       // lp

    ic1eq_ = 2.0f * v2 - ic1eq_;
    ic2eq_ = 2.0f * v3 - ic2eq_;

    float y = (1.0f - morph) * v3 + morph * v1;  // LP->HP morph
    y *= drive;

    // cheap soft clip (no guards)
    return y / (1.0f + fabsf(y));
  }

 private:
  float ic1eq_ = 0.0f;  // "integrator capacitor" 1 (≈ band state)
  float ic2eq_ = 0.0f;  // "integrator capacitor" 2 (≈ low state)
};