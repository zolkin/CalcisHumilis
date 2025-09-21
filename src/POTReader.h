#pragma once
#include <array>
#include <cmath>

template <int N, int BITS>
class POTReader {
 public:
  static constexpr int kMaxValue = (1 << BITS) - 1;

  // Add a reader; returns its index or -1 if full.
  int addReader(int activityThresh, float emaAlpha = 0.2f) {
    if (count_ >= N) return -1;
    thresholds_[count_] = (activityThresh > 0) ? activityThresh : 1;
    alphas_[count_]     = (emaAlpha > 0.f && emaAlpha <= 1.f) ? emaAlpha : 1.f;
    seeded_[count_]     = false;
    lastOut_[count_]    = 0;
    ++count_;
    return count_ - 1;
  }

  // io: on input = raw ADC counts (0..kMaxValue), on output = filtered counts
  // Returns true if any filtered value moved by >= its activity threshold.
  bool update(std::array<int, N>& io) {
    bool anyChanged = false;

    // First ever update: seed everything we have, copy to io, report false.
    if (!everUpdated_) {
      for (int i = 0; i < count_; ++i) {
        const int raw = clamp(io[i], 0, kMaxValue);
        ema_[i]      = static_cast<float>(raw);
        lastOut_[i]  = raw;
        seeded_[i]   = true;
        io[i]        = raw;
      }
      everUpdated_ = true;
      return false;  // per your requirement
    }

    for (int i = 0; i < count_; ++i) {
      const int raw = clamp(io[i], 0, kMaxValue);

      if (!seeded_[i]) {
        // Late-added reader: seed silently, no "change" reported this pass.
        ema_[i]     = static_cast<float>(raw);
        lastOut_[i] = raw;
        seeded_[i]  = true;
        io[i]       = raw;
        continue;
      }

      // EMA smoothing
      const float a = alphas_[i];
      ema_[i] += a * (static_cast<float>(raw) - ema_[i]);

      const int v = static_cast<int>(lroundf(ema_[i]));
      io[i] = v;

      if (std::abs(v - lastOut_[i]) >= thresholds_[i]) {
        lastOut_[i] = v;
        anyChanged = true;
      }
    }
    return anyChanged;
  }

  int count() const { return count_; }

 private:
  int count_ = 0;
  bool everUpdated_ = false;

  std::array<int,   N>   thresholds_{};
  std::array<float, N>   alphas_{};
  std::array<float, N>   ema_{};
  std::array<int,   N>   lastOut_{};
  std::array<bool,  N>   seeded_{};
};