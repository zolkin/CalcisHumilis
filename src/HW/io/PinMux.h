#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <tuple>
#include <utility>

#include "hw/io/Pin.h"
#include "hw/io/Types.h"
#include "util/BitSplit.h"

namespace zlkm::hw::io {

// Variadic multiplexer that composes multiple pin devices (e.g., McpPins,
// GpioPins) and exposes a unified Pin type that encodes (group, pin) using a
// BitSplit over RawPin = uint8_t by default.
// - High bits: device (group) index
// - Low bits:  device-local pin index/value (as expected by the underlying
// device)
template <typename RawPinT, typename... Devices>
class PinMux {
 private:
  using RawPin = RawPinT;
  static constexpr RawPin kDeviceCount = sizeof...(Devices);
  static_assert(kDeviceCount > 0 && kDeviceCount <= 64,
                "PinMux supports 1 to 64 devices");

  // Compute ceil(log2(n)) at compile-time
  static constexpr RawPin ceil_log2_raw(RawPin n) {
    return (n <= 1) ? 0 : (1 + ceil_log2_raw((RawPin)((n - 1) >> 1)));
  }

  // Number of bits assigned to select a device; remaining for pin id
  static constexpr RawPin kHighBits = ceil_log2_raw(kDeviceCount);
  static constexpr RawPin kLowBits = 8 - kHighBits;

  static_assert(kHighBits + kLowBits == 8, "BitSplit must fully cover RawPin");

  struct _MuxPinTag {};

 public:
  // Public-facing pin type encodes group and pin via BitSplitT over RawPin
  // - High bits: device (group) index
  // Defaults to group 0 if only pin is provided
  struct MuxPin : public ::zlkm::util::BitSplitT<_MuxPinTag, RawPin, kLowBits> {
    using Base = ::zlkm::util::BitSplitT<_MuxPinTag, RawPin, kLowBits>;
    using Base::Base;  // inherit constructors

    explicit MuxPin(PinId pin, PinGroupId group)
        : Base(pin.value, group.value) {}
    explicit MuxPin(RawPin pin, PinGroupId group) : Base(pin, group.value) {}
    explicit MuxPin(RawPin pin) : Base(pin, 0) {}
    explicit MuxPin(PinId pin) : Base(pin.value, 0) {}

    // Accessors per spec
    constexpr PinId pin() const { return PinId{this->low}; }
    constexpr PinGroupId group() const { return PinGroupId{this->high}; }
  };

  using PinIdT = MuxPin;

  template <size_t N>
  using PinIdArrayT = std::array<PinIdT, N>;

  explicit PinMux(Devices&... devs) : devs_(devs...) {}

  // Set the same mode on all devices
  inline void setMode(PinMode m) {
    applyAll_([&](auto& d) { d.setMode(m); });
  }

  // Per-pin mode
  inline void setPinMode(PinIdT p, PinMode m) {
    dispatch_(p.group().value,
              [&](auto& d) { d.setPinMode((int)p.pin().value, m); });
  }

  // Write a single pin
  inline void writePin(PinIdT p, bool high) const {
    dispatch_(p.group().value,
              [&](auto& d) { d.writePin((int)p.pin().value, high); });
  }

  // Read a single pin
  inline bool readPin(PinIdT p) const {
    return dispatch_(p.group().value,
                     [&](auto& d) { return d.readPin((int)p.pin().value); });
  }

  // Bulk read: if all pins are in the same group, use device's bulk read;
  // otherwise fall back to per-pin
  template <size_t K>
  inline std::bitset<K> readPins(const PinIdArrayT<K>& pins) const {
    std::bitset<K> out{};
    for (size_t i = 0; i < K; ++i) {
      out.set(i, readPin(pins[i]));
    }
    return out;
  }

  // Bulk read for a single device group; asserts all pins share the same group
  template <size_t K>
  inline std::bitset<K> readGroupPins(const PinIdArrayT<K>& pins) const {
    // Ensure non-empty and same group
    const auto g0 = pins[0].group();
    assert(std::all_of(pins.begin(), pins.end(),
                       [&](const PinIdT& p) { return p.group() == g0; }) &&
           "readGroupPins requires all pins to be from the same group");

    // Prepare low pin array for the target device
    std::array<PinId, K> lows{};
    for (size_t i = 0; i < K; ++i) lows[i] = pins[i].pin();
    auto devBits = dispatch_(
        g0.value, [&](auto& d) { return d.template readPins<K>(lows); });
    return devBits;
  }

  // Bulk read for a specific device group with low-level pin ids
  template <size_t K>
  inline std::bitset<K> readGroupPins(PinGroupId group,
                                      const std::array<PinId, K>& pins) const {
    return dispatch_(group.value,
                     [&](auto& d) { return d.template readPins<K>(pins); });
  }

  // Bulk read two pin sets (A and B) for a specific device group in a single
  // device transaction, by interleaving the read order as [A0, B0, A1, B1, ...]
  // to minimize timing skew between corresponding A/B signals.
  template <size_t K>
  inline std::pair<std::bitset<K>, std::bitset<K>> readGroupPinsInterleaved(
      PinGroupId group, const std::array<PinId, K>& pinsA,
      const std::array<PinId, K>& pinsB) const {
    std::array<PinId, 2 * K> pins{};
    for (size_t i = 0; i < K; ++i) {
      pins[2 * i] = pinsA[i];
      pins[2 * i + 1] = pinsB[i];
    }
    auto bits2 = dispatch_(group.value,
                           [&](auto& d) { return d.template readPins<2 * K>(pins); });
    std::bitset<K> outA, outB;
    for (size_t i = 0; i < K; ++i) {
      outA.set(i, bits2.test(2 * i));
      outB.set(i, bits2.test(2 * i + 1));
    }
    return {outA, outB};
  }

 private:
  std::tuple<Devices&...> devs_;

  template <typename F, size_t... I>
  inline void applyAll_(F&& f, std::index_sequence<I...>) {
    (f(std::get<I>(devs_)), ...);
  }
  template <typename F>
  inline void applyAll_(F&& f) {
    applyAll_(std::forward<F>(f),
              std::make_index_sequence<sizeof...(Devices)>{});
  }

  // Dispatch helper with minimal overhead (constexpr recursion over tuple
  // indices)
  template <size_t I = 0, typename F>
  inline auto dispatch_(RawPin idx, F&& f) const -> decltype(auto) {
    if constexpr (I >= sizeof...(Devices)) {
      // Out of range; undefined index - return default
      if constexpr (std::is_void_v<decltype(f(std::get<0>(devs_)))>) {
        return;
      } else {
        using Ret = decltype(f(std::get<0>(devs_)));
        return Ret{};
      }
    } else {
      if (idx == I) {
        return f(std::get<I>(devs_));
      } else {
        return dispatch_<I + 1>(idx, std::forward<F>(f));
      }
    }
  }
};

}  // namespace zlkm::hw::io
