#include <array>

namespace zlkm {
template <class Reader>
class MultiInput {
 public:
  static constexpr int N = Reader::CHAN_COUNT;

  struct Cfg {
    std::array<uint8_t, N> chan = {};  // 0..Reader::CHAN_COUNT-1
    int maxCode = 4095;
    float emaAlpha = 0.2f;    // 0..1 (1=no smoothing)
    float activityLSB = 4.f;  // min integer step to report as change
  };

  MultiInput(Reader& reader, const Cfg& cfg) : reader_(reader), cfg_(cfg) {
    last_.fill(INT_MIN);
    ema_.fill(0.f);
    changed_.fill(false);
  }

  // Reads ALL channels per call.
  // - First call seeds baselines, returns false, and clears changed_.
  // - After that, returns true if ANY channel changed by >= activityLSB.
  bool update() {
    const float vref = reader_.vrefVolts();
    const float scale = static_cast<float>(cfg_.maxCode) / vref;
    const float a =
        (cfg_.emaAlpha > 0.f && cfg_.emaAlpha <= 1.f) ? cfg_.emaAlpha : 1.f;

    changed_.fill(false);
    bool anyChanged = false;

    for (int i = 0; i < N; ++i) {
      const float volts = reader_.readVolts(cfg_.chan[i]);
      const float code = volts * scale;

      if (!seededAll_) {
        ema_[i] = code;
        last_[i] = static_cast<int>(lroundf(code));
        continue;
      }

      ema_[i] += a * (code - ema_[i]);
      const int vi = static_cast<int>(lroundf(ema_[i]));
      if (abs(vi - last_[i]) >= cfg_.activityLSB) {
        last_[i] = vi;
        changed_[i] = true;
        anyChanged = true;
      }
    }

    if (!seededAll_) {
      seededAll_ = true;
      return false;  // initial seeding
    }
    return anyChanged;
  }

  // Accessors / tweaks
  int value(int i) const { return last_.at(i); }
  bool valueChanged(int i) const { return changed_.at(i); }  // NEW

  int maxCode() const { return cfg_.maxCode; }
  void setMaxCode(int mc) { cfg_.maxCode = mc; }

  void setSmoothing(float a) { cfg_.emaAlpha = a; }
  void setActivityLSB(float s) { cfg_.activityLSB = s; }

  void setChannels(const std::array<uint8_t, N>& ch) {
    cfg_.chan = ch;
    resetEMA();
  }
  void resetEMA() { seededAll_ = false; }

 private:
  Reader& reader_;
  Cfg cfg_;

  std::array<int, N> last_;
  std::array<float, N> ema_;
  std::array<bool, N> changed_;  // NEW: per-channel change flags
  bool seededAll_ = false;
};

}  // namespace zlkm