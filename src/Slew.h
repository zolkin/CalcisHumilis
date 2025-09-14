#pragma once
#include <Arduino.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Exponential slew: y += alpha * (target - y)
// Configure with a time constant in milliseconds (independent of block size).
// -----------------------------------------------------------------------------
class SlewOnePole {
 public:
  SlewOnePole() = default;

  // Configure using a time constant (ms) and sample rate (Hz).
  // ms <= 0 makes it "instant" (alpha = 1).
  void configure(int sampleRate, float timeMs) {
    sr_ = sampleRate > 0 ? sampleRate : 48000;
    setTimeMs(timeMs);
  }

  // Change time constant without touching current value/target.
  void setTimeMs(float timeMs) {
    if (timeMs <= 0.0f) {
      alpha_ = 1.0f;
      return;
    }
    const float tauSamples = (timeMs * static_cast<float>(sr_)) * 0.001f;
    alpha_ = 1.0f - expf(-1.0f / tauSamples);
  }

  // Hard set current value and target (no transition).
  void reset(float v) {
    y_ = v;
    t_ = v;
  }

  // Set a new target; smoothing will move y toward it on subsequent tick().
  void setTarget(float v) { t_ = v; }

  // One sample step; returns the smoothed value.
  inline float tick() {
    // Fast path when alpha_ == 1: jump to target.
    if (alpha_ >= 1.0f) {
      y_ = t_;
      return y_;
    }
    y_ += alpha_ * (t_ - y_);
    return y_;
  }

  // Convenience: set target then step once.
  inline float tickTo(float newTarget) {
    t_ = newTarget;
    return tick();
  }

  float value() const { return y_; }
  float target() const { return t_; }
  float alpha() const { return alpha_; }

 private:
  int sr_ = 48000;
  float alpha_ = 1.0f;  // [0..1], 1 = instant
  float y_ = 0.0f;      // current smoothed value
  float t_ = 0.0f;      // target
};

// -----------------------------------------------------------------------------
// Linear slew: moves toward target at a maximum rate (units per second).
// Useful when you want strictly linear ramps rather than exponential.
// -----------------------------------------------------------------------------
class SlewLinear {
 public:
  SlewLinear() = default;

  void configure(int sampleRate, float maxUnitsPerSecond) {
    sr_ = sampleRate > 0 ? sampleRate : 48000;
    setRate(maxUnitsPerSecond);
  }

  // maxUnitsPerSecond is in "output units per second" (e.g. gain 0..1 / s).
  void setRate(float maxUnitsPerSecond) {
    if (maxUnitsPerSecond <= 0.0f) {
      step_ = INFINITY;  // instant
    } else {
      step_ = maxUnitsPerSecond / static_cast<float>(sr_);
    }
  }

  void reset(float v) {
    y_ = v;
    t_ = v;
  }

  void setTarget(float v) { t_ = v; }

  inline float tick() {
    if (!isfinite(step_)) {
      y_ = t_;
      return y_;
    }
    const float d = t_ - y_;
    if (fabsf(d) <= step_) {
      y_ = t_;
    } else {
      y_ += (d > 0.0f ? step_ : -step_);
    }
    return y_;
  }

  inline float tickTo(float newTarget) {
    t_ = newTarget;
    return tick();
  }

  float value() const { return y_; }
  float target() const { return t_; }
  float stepPerSample() const { return step_; }

 private:
  int sr_ = 48000;
  float step_ = INFINITY;  // units/sample; INFINITY = instant
  float y_ = 0.0f;
  float t_ = 0.0f;
};