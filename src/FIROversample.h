#pragma once
#include <array>
#include <cmath>
#include <cstdint>

template <int NTAPS>
struct FirLP {
  static_assert(NTAPS % 2 == 1, "Use odd tap count for linear-phase FIR.");
  std::array<float, NTAPS> h{};   // coefficients
  std::array<float, NTAPS> xL{};  // delay line L
  std::array<float, NTAPS> xR{};  // delay line R
  int idx = 0;

  // Kaiser I0
  static float i0(float x) {
    float y = (x * x) * 0.25f;
    float s = 1.0f, t = 1.0f;
    for (int k = 1; k <= 10; ++k) {
      t *= y / (k * k);
      s += t;
    }
    return s;
  }

  // Build windowed-sinc LPF: cutoff = fc (0..0.5), beta ~6..8 ~70–80 dB
  void design(float fc, float beta = 7.5f) {
    const int M = NTAPS - 1;
    const float i0b = i0(beta);
    for (int n = 0; n < NTAPS; ++n) {
      const int k = n - M / 2;
      const float sinc =
          (k == 0) ? 2.0f * fc
                   : sinf(2.0f * float(M_PI) * fc * k) / (float(M_PI) * k);
      const float r = float(n) / float(M);
      const float w =
          i0(beta * std::sqrtf(1.0f - (2.0f * r - 1.0f) * (2.0f * r - 1.0f))) /
          i0b;
      h[n] = sinc * w;
    }
    // Normalize DC gain to 1.0
    float sum = 0.0f;
    for (float c : h) sum += c;
    for (float &c : h) c /= sum;
  }

  inline float tickSample(float s, std::array<float, NTAPS> &ring) {
    ring[idx] = s;
    // linear-phase convolution centered around idx (circular buffer)
    float acc = 0.0f;
    int j = idx;
    for (int n = 0; n < NTAPS; ++n) {
      acc += h[n] * ring[j];
      if (--j < 0) j = NTAPS - 1;
    }
    if (++idx >= NTAPS) idx = 0;
    return acc;
  }

  inline void reset() {
    xL.fill(0.f);
    xR.fill(0.f);
    idx = 0;
  }

  inline void tickStereo(float inL, float inR, float &outL, float &outR) {
    outL = tickSample(inL, xL);
    outR = tickSample(inR, xR);
  }
};

// Oversampled synth → FIR @ OS*SR → decimate by OS
template <int OS, int NTAPS>
struct OversampleDecimator {
  FirLP<NTAPS> fir;
  static constexpr bool Enabled = (OS > 1);

  void setup() {
    if constexpr (Enabled) {
      // Cutoff at ~ 0.45 of base-rate Nyquist (i.e., 0.225 of OS-rate Nyquist)
      // Expressed in normalized OS-rate (Nyquist=0.5): fc = 0.5 / OS * 0.9
      const float fc = 0.45f / float(OS);  // conservative stopband for images
      fir.design(fc, 7.5f);
    }
  }
  void reset() {
    if constexpr (Enabled) fir.reset();
  }
};