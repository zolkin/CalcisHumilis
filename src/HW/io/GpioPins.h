#pragma once

#include <array>
#include <bitset>

#include "hw/io/Pin.h"

namespace zlkm::hw::io {

template <size_t N>
class GpioPins {
 public:
  using PinIdT = PinId;
  using Pins = PinIdArray<N>;
  using Modes = std::array<PinMode, N>;
  using Bits = std::bitset<N>;

  // Single mode for all pins
  explicit GpioPins(PinMode mode_for_all) { setMode(mode_for_all); }

  void setMode(PinMode mode) {
    for (uint8_t i = 0; i < N; ++i) applyPinMode(PinId{i}, mode);
  }

  // Per-pin modes
  GpioPins(const Modes& modes) {
    for (uint8_t i = 0; i < N; ++i) applyPinMode(PinId{i}, modes[i]);
  }

  inline void setPinMode(PinId pin, PinMode m) { applyPinMode(pin, m); }

  inline void writeAll(const Bits& b) const {
    for (uint8_t i = 0; i < N; ++i) ::digitalWrite(i, b.test(i) ? HIGH : LOW);
  }

  inline void writePin(PinId pin, bool high) const {
    ::digitalWrite(pin.value, high ? HIGH : LOW);
  }

  inline Bits readAll() const {
    Bits out;
    for (uint8_t i = 0; i < N; ++i)
      if (::digitalRead(i)) out.set(i);
    return out;
  }

  template <size_t K>
  inline std::bitset<K> readPins(const PinIdArray<K>& pins) const {
    std::bitset<K> out;
    for (int i = 0; i < K; ++i) out.set(i, readPin(pins[i].value));
    return out;
  }

  inline bool readPin(uint8_t i) const { return ::digitalRead(i) != 0; }

 private:
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