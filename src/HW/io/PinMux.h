#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <tuple>
#include <utility>

#include "hw/io/Pin.h"
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

    constexpr GroupPinArray<1> toGroupArray() const {
      return GroupPinArray<1>(group(), PinIdArray<1>{pin()});
    }
  };

  using PinIdT = MuxPin;

  template <size_t N>
  using PinIdArrayT = zlkm::hw::io::PinIdArray<N>;
  template <size_t N>
  using GroupPinArrayT = zlkm::hw::io::GroupPinArray<N>;

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

  template <size_t K>
  inline void setPinsMode(GroupPinArrayT<K> const& pins, PinMode m) {
    dispatch_(pins.group().value, [&](auto& d) {
      for (size_t i = 0; i < K; ++i) {
        d.setPinMode(pins[i], m);
      }
    });
  }

  template <size_t K>
  inline void writePins(GroupPinArrayT<K> const& pins, bool high) {
    dispatch_(pins.group().value, [&](auto& d) {
      for (size_t i = 0; i < K; ++i) {
        d.writePin(pins[i], high);
      }
    });
  }

  // Write a single pin
  inline void writePin(PinIdT p, bool high) const {
    return writeGroupPin(p.group(), p.pin(), high);
  }

  inline void writeGroupPin(PinGroupId group, PinId pin, bool high) const {
    dispatch_(group.value, [&](auto& d) { d.writePin(pin, high); });
  }

  // Read a single pin
  inline bool readPin(PinIdT p) const {
    return readGroupPin(p.group(), p.pin());
  }

  inline bool readGroupPin(PinGroupId group, PinId pin) const {
    return dispatch_(group.value, [&](auto& d) { return d.readPin(pin); });
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

  template <size_t K>
  inline std::bitset<K> readGroupPins(const GroupPinArray<K>& pins) const {
    return dispatch_(pins.group().value,
                     [&](auto& d) { return d.template readPins<K>(pins); });
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
