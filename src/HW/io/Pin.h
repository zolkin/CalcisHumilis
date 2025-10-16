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
inline constexpr PinIdArray<N> defaultPinSet() {
  PinIdArray<N> pins{};
  for (size_t i = 0; i < N; ++i) pins[i] = PinId{static_cast<uint8_t>(i)};
  return pins;
}

}  // namespace zlkm::hw::io