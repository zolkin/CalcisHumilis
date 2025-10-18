#pragma once

#include <array>
#include <bitset>

#include "hw/io/Pin.h"
namespace zlkm::test {

using namespace zlkm::hw::io;

struct FakePins {
  std::array<bool, 16> level{};  // index by raw pin id

  void setPin(PinId id, bool high) { level[id.value] = high; }
  template <size_t K>
  void setGroupPins(GroupPinArray<K> group, bool high) {
    for (auto pin : group) {
      setPin(pin, high);
    }
  }
  template <size_t K>
  void setPinsMode(GroupPinArray<K>, PinMode /*m*/) {}

  template <size_t K>
  std::bitset<K> readGroupPins(const GroupPinArray<K>& group) const {
    std::bitset<K> out;
    for (size_t i = 0; i < K; ++i) out.set(i, level[group[i].value]);
    return out;
  }
};

}  // namespace zlkm::test