#pragma once

#include <cassert>

namespace zlkm::util {

template <typename Tag, typename T>
struct TaggedTypeT {
  using ValueType = T;

  constexpr TaggedTypeT() = default;

  // Single by-value constructor avoids overload ambiguity with brace-init
  constexpr explicit TaggedTypeT(T v) : value(v) {}

  operator bool() const { return static_cast<bool>(value); }
  explicit operator T() const { return value; }
  explicit operator T&() { return value; }

  bool operator==(const TaggedTypeT& other) const {
    return value == other.value;
  }
  bool operator!=(const TaggedTypeT& other) const {
    return value != other.value;
  }
  bool operator<(const TaggedTypeT& other) const { return value < other.value; }
  bool operator<=(const TaggedTypeT& other) const {
    return value <= other.value;
  }
  bool operator>(const TaggedTypeT& other) const { return value > other.value; }
  bool operator>=(const TaggedTypeT& other) const {
    return value >= other.value;
  }

  T value{};
};

// BitSplitT moved to util/BitSplit.h

}  // namespace zlkm::util

#define ZLKM_MAKE_TAGGED_TYPE(NAME, TYPE) \
  struct _##NAME##Tag {};                 \
  using NAME = zlkm::util::TaggedTypeT<_##NAME##Tag, TYPE>;

#define ZLKM_MAKE_BIT_SPLIT(NAME, TYPE, BITS) \
  struct _##NAME##Tag {};                     \
  using NAME = zlkm::util::BitSplitT<_##NAME##Tag, TYPE, BITS>;
