#pragma once
#include <array>
#include <bitset>

#include "hw/io/Pin.h"

namespace zlkm::hw::io {

template <int N, typename PinGroupT>
class ButtonManager {
 public:
  using Bits = std::bitset<N>;
  using GroupArrayT = typename PinGroupT::GroupPinArrayT<N>;

  struct Cfg {
    GroupArrayT pins;  // backend-native ids (native to PinGroupT)
    bool activeLow = true;
    bool usePullUp = false;
    uint8_t debounceTicks = 5;  // consecutive equal samples to accept change
  };

  ButtonManager(const ButtonManager&) = delete;
  ButtonManager& operator=(const ButtonManager&) = delete;
  ButtonManager(ButtonManager&&) = default;
  ButtonManager& operator=(ButtonManager&&) = default;

  ButtonManager(PinGroupT& dev, const Cfg& cfg) : dev_(dev), cfg_(cfg) {
    const PinMode pm = (cfg_.activeLow && cfg_.usePullUp) ? PinMode::InputPullUp
                                                          : PinMode::Input;
    dev_.setPinsMode(cfg_.pins, pm);

    Bits now = dev_.readGroupPins(cfg_.pins);  // 1 = HIGH
    if (cfg_.activeLow) now = ~now;            // 1 = pressed
    stable_ = prev_ = lastSample_ = now;
    cnt_.fill(0);
  }

  struct Report {
    Bits pressed;
    Bits rising;
    Bits falling;
  };

  inline Report tick() {
    Bits raw = dev_.readGroupPins(cfg_.pins);  // 1 = HIGH
    if (cfg_.activeLow) raw = ~raw;            // 1 = pressed

    Bits rising, falling;
    for (int i = 0; i < N; ++i) {
      bool s = raw.test(i);
      if (s != lastSample_.test(i)) {
        lastSample_.set(i, s);
        cnt_[i] = 1;
      } else if (cnt_[i] < 0xFF) {
        ++cnt_[i];
      }

      if (cnt_[i] >= cfg_.debounceTicks && stable_.test(i) != s) {
        bool was = stable_.test(i);
        stable_.set(i, s);
        if (!was && s)
          rising.set(i);
        else if (was && !s)
          falling.set(i);
      }
    }
    prev_ = stable_;
    return {stable_, rising, falling};
  }

  inline Bits pressed() const { return stable_; }

 private:
  PinGroupT& dev_;
  Cfg cfg_;
  Bits lastSample_{};
  Bits stable_{};
  Bits prev_{};
  std::array<uint8_t, N> cnt_{};
};

}  // namespace zlkm::hw::io