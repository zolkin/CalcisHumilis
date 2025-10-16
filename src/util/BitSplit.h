#pragma once

#include <cassert>

namespace zlkm::util {

template <typename Tag, typename T, int BitSplit>
struct BitSplitT {
  using ValueType = T;

  static_assert(BitSplit > 0 && BitSplit < (sizeof(T) * 8),
                "BitSplit must be between 1 and sizeof(T)*8 - 1");

  static constexpr int HIGH_BITS = int(sizeof(T) * 8) - BitSplit;
  static constexpr int LOW_BITS = BitSplit;

  static constexpr T MAX_LOW = (T(1) << LOW_BITS) - 1;
  static constexpr T MAX_HIGH = (T(1) << HIGH_BITS) - 1;

  constexpr BitSplitT() = default;
  constexpr explicit BitSplitT(T v) : value(v) {}
  constexpr explicit BitSplitT(T l, T h) : value((h << LOW_BITS) | l) {
    assert(l <= MAX_LOW && h <= MAX_HIGH);
  }
  constexpr explicit operator T() const { return value; }

  operator bool() const { return static_cast<bool>(value); }
  explicit operator T&() { return value; }

  bool operator==(const BitSplitT& other) const { return value == other.value; }
  bool operator!=(const BitSplitT& other) const { return value != other.value; }
  bool operator<(const BitSplitT& other) const { return value < other.value; }
  bool operator<=(const BitSplitT& other) const { return value <= other.value; }
  bool operator>(const BitSplitT& other) const { return value > other.value; }
  bool operator>=(const BitSplitT& other) const { return value >= other.value; }

  union {
    T value{};
    struct {
      T low : LOW_BITS;
      T high : HIGH_BITS;
    };
  };
};

}  // namespace zlkm::util
