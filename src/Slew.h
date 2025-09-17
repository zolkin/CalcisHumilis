#pragma once
#include <array>
#include <cfloat>
#include <cmath>

// ==========================
// Exponential slew (one-pole)
// y += alpha * (t - y)
// ==========================
template <int N, int SR>
struct SlewOnePoleN {
  static constexpr int kN = N;
  static constexpr int kSR = SR;

  // State
  std::array<float, N> y{};      // current
  std::array<float, N> t{};      // target
  std::array<float, N> alpha{};  // [0..1], 1 = instant

  // --- Helpers ---
  static inline float alphaFromMs(float ms) {
    if (ms <= 0.0f) return 1.0f;  // instant
    const float tau_samp = ms * 0.001f * float(SR);
    return 1.0f - std::exp(-1.0f / tau_samp);
  }

  // --- Init / reset ---
  inline void resetAll(float v) {
    y.fill(v);
    t.fill(v);
  }
  inline void reset(int i, float v) {
    y[i] = v;
    t[i] = v;
  }

  // --- Configure alpha (all or per voice) ---
  inline void setTimeMsAll(float ms) {
    const float a = alphaFromMs(ms);
    alpha.fill(a);
  }
  inline void setTimeMs(int i, float ms) { alpha[i] = alphaFromMs(ms); }
  inline void setAlphaAll(float a) {
    alpha.fill(a);
  }  // direct control if desired
  inline void setAlpha(int i, float a) { alpha[i] = a; }

  // --- Targets ---
  inline void setTarget(int i, float v) { t[i] = v; }
  inline void setTargets(const std::array<float, N>& tv) { t = tv; }

  // --- Tick single voice ---
  inline float tick(int i) {
    const float a = alpha[i];
    if (a >= 1.0f) {
      y[i] = t[i];
      return y[i];
    }
    y[i] += a * (t[i] - y[i]);
    return y[i];
  }
  inline float tickTo(int i, float newTarget) {
    t[i] = newTarget;
    return tick(i);
  }

  // --- Tick all voices (optionally with new targets) ---
  inline void tickAll() {
    for (int i = 0; i < N; ++i) tick(i);
  }
  inline void tickAllTo(const std::array<float, N>& newTargets) {
    t = newTargets;
    tickAll();
  }
};

// ==========================
// Linear slew (rate-limited)
// moves at +/-step units/sample
// ==========================
template <int N, int SR>
struct SlewLinearN {
  static constexpr int kN = N;
  static constexpr int kSR = SR;

  std::array<float, N> y{};     // current
  std::array<float, N> t{};     // target
  std::array<float, N> step{};  // units/sample; INF => instant

  // --- Helpers ---
  static inline float stepFromUnitsPerSecond(float ups) {
    if (ups <= 0.0f) return INFINITY;  // instant
    return ups / float(SR);
  }

  // --- Init / reset ---
  inline void resetAll(float v) {
    y.fill(v);
    t.fill(v);
  }
  inline void reset(int i, float v) {
    y[i] = v;
    t[i] = v;
  }

  // --- Configure rate (all or per voice) ---
  inline void setRateAll(float unitsPerSecond) {
    const float s = stepFromUnitsPerSecond(unitsPerSecond);
    step.fill(s);
  }
  inline void setRate(int i, float unitsPerSecond) {
    step[i] = stepFromUnitsPerSecond(unitsPerSecond);
  }
  inline void setStepAll(float s) { step.fill(s); }  // direct per-sample step
  inline void setStep(int i, float s) { step[i] = s; }

  // --- Targets ---
  inline void setTarget(int i, float v) { t[i] = v; }
  inline void setTargets(const std::array<float, N>& tv) { t = tv; }

  // --- Tick ---
  inline float tick(int i) {
    const float s = step[i];
    if (!std::isfinite(s)) {
      y[i] = t[i];
      return y[i];
    }
    const float d = t[i] - y[i];
    if (std::fabs(d) <= s)
      y[i] = t[i];
    else
      y[i] += (d > 0.f ? s : -s);
    return y[i];
  }
  inline float tickTo(int i, float newTarget) {
    t[i] = newTarget;
    return tick(i);
  }
  inline void tickAll() {
    for (int i = 0; i < N; ++i) tick(i);
  }
  inline void tickAllTo(const std::array<float, N>& newTargets) {
    t = newTargets;
    tickAll();
  }
};