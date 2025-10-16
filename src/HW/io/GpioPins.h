#pragma once

#include <array>
#include <bitset>

#include "hw/io/Pin.h"
#include "hw/io/Types.h"

namespace zlkm::hw::io {

template <size_t N>
class GpioPins {
 public:
  using PinIdT = PinId;
  using Pins = PinIdArray<N>;
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
      ::digitalWrite((uint8_t)pins_[i].value, b.test(i) ? HIGH : LOW);
  }

  inline void writePin(size_t i, bool high) const {
    ::digitalWrite((uint8_t)pins_[i].value, high ? HIGH : LOW);
  }

  inline Bits readAll() const {
    Bits out;
    for (size_t i = 0; i < N; ++i)
      if (::digitalRead((uint8_t)pins_[i].value)) out.set(i);
    return out;
  }

  template <size_t K>
  inline std::bitset<K> readPins(const std::array<PinId, K>& idxs) const {
    std::bitset<K> out;
    for (int i = 0; i < K; ++i) out.set(i, readPin((uint8_t)idxs[i].value));
    return out;
  }

  inline bool readPin(size_t i) const {
    return ::digitalRead((uint8_t)pins_[i].value) != 0;
  }

  inline uint8_t rawPin(size_t i) const { return pins_[i].value; }

 private:
  Pins pins_;

  static inline void applyPinMode(PinId pin, PinMode m) {
    switch (m) {
      case PinMode::Input:
        ::pinMode((uint8_t)pin.value, INPUT);
        break;
      case PinMode::InputPullUp:
        ::pinMode((uint8_t)pin.value, INPUT_PULLUP);
        break;
      case PinMode::Output:
        ::pinMode((uint8_t)pin.value, OUTPUT);
        break;
    }
  }
};

}  // namespace zlkm::hw::io