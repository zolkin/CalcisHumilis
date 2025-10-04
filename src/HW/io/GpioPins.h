#pragma once

#include <array>
#include <bitset>

#include "hw/io/Types.h"

namespace zlkm::hw::io {

template <size_t N>
class GpioPins {
 public:
  using Pins = std::array<uint8_t, N>;
  using Modes = std::array<PinMode, N>;
  using Bits = std::bitset<N>;

  // Single mode for all pins
  explicit GpioPins(const Pins& pins, PinMode mode_for_all) : pins_(pins) {
    setMode(mode_for_all);
  }

  // Per-pin modes
  GpioPins(const Pins& pins, const Modes& modes) : pins_(pins) {
    for (size_t i = 0; i < N; ++i) applyPinMode(pins_[i], modes[i]);
  }

  inline void setMode(PinMode m) {
    for (size_t i = 0; i < N; ++i) applyPinMode(pins_[i], m);
  }

  inline void setPinMode(size_t i, PinMode m) { applyPinMode(pins_[i], m); }

  inline void writeAll(const Bits& b) const {
    for (size_t i = 0; i < N; ++i)
      ::digitalWrite(pins_[i], b.test(i) ? HIGH : LOW);
  }

  inline void writePin(size_t i, bool high) const {
    ::digitalWrite(pins_[i], high ? HIGH : LOW);
  }

  inline Bits readAll() const {
    Bits out;
    for (size_t i = 0; i < N; ++i)
      if (::digitalRead(pins_[i])) out.set(i);
    return out;
  }

  template <size_t K>
  inline std::bitset<K> readPins(const std::array<uint8_t, K>& idxs) const {
    std::bitset<K> out;
    for (int i = 0; i < K; ++i) out.set(i, readPin(idxs[i]));
    return out;
  }

  inline bool readPin(size_t i) const { return ::digitalRead(pins_[i]) != 0; }

  inline uint8_t rawPin(size_t i) const { return pins_[i]; }

 private:
  Pins pins_;

  static inline void applyPinMode(uint8_t pin, PinMode m) {
    switch (m) {
      case PinMode::Input:
        ::pinMode(pin, INPUT);
        break;
      case PinMode::InputPullUp:
        ::pinMode(pin, INPUT_PULLUP);
        break;
      case PinMode::Output:
        ::pinMode(pin, OUTPUT);
        break;
    }
  }
};

}  // namespace zlkm::hw::io