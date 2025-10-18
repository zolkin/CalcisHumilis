#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <tuple>
#include <type_traits>
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

  static constexpr bool kSingleDevice = (kDeviceCount == 1);

 public:
  // Local aliases for pin arrays; GroupPinArrayT is conditional in
  // single-device mode to be a plain PinIdArray.
  template <size_t N>
  using PinIdArrayT = zlkm::hw::io::PinIdArray<N>;

  template <size_t N>
  using GroupPinArrayT = std::conditional_t<kSingleDevice, PinIdArrayT<N>,
                                            ::zlkm::hw::io::GroupPinArray<N>>;
  struct _MuxPinTag {};
  struct _MuxPin
      : public ::zlkm::util::BitSplitT<_MuxPinTag, RawPin, kLowBits> {
    using Base = ::zlkm::util::BitSplitT<_MuxPinTag, RawPin, kLowBits>;
    using Base::Base;  // inherit constructors

    explicit _MuxPin(PinId pin, PinGroupId group)
        : Base(pin.value, group.value) {}
    explicit _MuxPin(RawPin pin, PinGroupId group) : Base(pin, group.value) {}
    explicit _MuxPin(RawPin pin) : Base(pin, 0) {}
    explicit _MuxPin(PinId pin) : Base(pin.value, 0) {}

    constexpr operator PinId() const { return pin(); }
    constexpr operator PinGroupId() const { return group(); }

    constexpr PinId pin() const { return PinId{this->low}; }
    constexpr PinGroupId group() const { return PinGroupId{this->high}; }
  };

  using PinIdT = std::conditional_t<kSingleDevice, PinId, _MuxPin>;

  // (aliases moved above)

  explicit PinMux(Devices&... devs) : devs_(devs...) {}

  // Group operations (preferred APIs used across the codebase)

  // Generic APIs that delegate group selection to dispatchPins_
  template <class PinGroup>
  inline void setPinsMode(const PinGroup& pins, PinMode m) {
    dispatchPins_(pins, [&](auto& d) {
      for (const auto& p : pins) d.setPinMode(p, m);
    });
  }

  template <class PinGroup>
  inline void writePins(const PinGroup& pins, bool high) {
    dispatchPins_(pins, [&](auto& d) {
      for (const auto& p : pins) d.writePin(p, high);
    });
  }

  template <class PinGroup>
  inline void writeGroupPin(const PinGroup& pins, int index, bool high) const {
    dispatchPins_(pins, [&](auto& d) { d.writePin(pins[index], high); });
  }

  template <class PinGroup>
  inline auto readGroupPins(const PinGroup& pins) const
      -> std::bitset<PinArraySizeV<PinGroup>> {
    constexpr size_t N = PinArraySizeV<PinGroup>;
    return dispatchPins_(pins,
                         [&](auto& d) { return d.template readPins<N>(pins); });
  }

 private:
  std::tuple<Devices&...> devs_;

  template <typename PinsT, typename F>
  inline auto dispatchPins_(const PinsT& pins, F&& f) const -> decltype(auto) {
    if constexpr (kSingleDevice) {
      return f(std::get<0>(devs_));
    } else {
      return dispatch_(pins.group().value, std::forward<F>(f));
    }
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
