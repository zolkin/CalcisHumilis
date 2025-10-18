#pragma once

#include <stdint.h>

#include <array>

#include "util/TaggedType.h"

namespace zlkm::hw::io {

ZLKM_MAKE_TAGGED_TYPE(PinId, uint8_t);
ZLKM_MAKE_TAGGED_TYPE(PinGroupId, uint8_t);

template <size_t N>
using PinIdArray = std::array<PinId, N>;

template <size_t N>
struct GroupPinArray : public PinIdArray<N> {
  using Base = PinIdArray<N>;

  GroupPinArray() = default;
  GroupPinArray(PinGroupId group, Base pins) : Base(pins), group_(group) {}

  GroupPinArray(PinGroupId group, std::array<PinId::ValueType, N> rawPins)
      : GroupPinArray{group, toPinList(rawPins)} {}

  constexpr PinGroupId group() const { return group_; }

 private:
  constexpr Base toPinList(std::array<PinId::ValueType, N> rawPins) {
    Base pins{};
    for (size_t i = 0; i < N; ++i) {
      pins[i] = PinId{rawPins[i]};
    }
    return pins;
  }

  PinGroupId group_ = PinGroupId{0};
};

template <class PinGroup>
struct PinArraySizeHelper;

template <size_t N>
struct PinArraySizeHelper<GroupPinArray<N>> {
  static constexpr size_t value = N;
};

template <size_t N>
struct PinArraySizeHelper<PinIdArray<N>> {
  static constexpr size_t value = N;
};

template <typename PinGroupT>
static constexpr size_t PinArraySizeV = PinArraySizeHelper<PinGroupT>::value;

enum class PinMode : uint8_t { Input, InputPullUp, Output };

}  // namespace zlkm::hw::io