#include <unity.h>

#include <array>
#include <bitset>
#include <type_traits>

#include "hw/io/PinMux.h"

using namespace zlkm::hw::io;

// Minimal fake pin device for testing on native platform
template <size_t N>
struct FakePins {
  using Bits = std::bitset<N>;
  Bits state{};
  std::array<PinMode, N> modes{};

  // APIs expected by PinMux generic methods
  inline void setPinMode(PinId p, PinMode m) { modes[(size_t)p.value] = m; }
  inline void writePin(PinId p, bool high) { state.set((size_t)p.value, high); }
  template <size_t K>
  inline std::bitset<K> readPins(const PinIdArray<K>& pins) const {
    std::bitset<K> out;
    for (size_t i = 0; i < K; ++i)
      out.set(i, state.test((size_t)pins[i].value));
    return out;
  }
};

static void test_single_device_pinmux_basic() {
  FakePins<8> dev{};
  using MuxT = PinMux<uint8_t, FakePins<8>>;
  MuxT mux{dev};

  // Pin type should be transparent PinId in single-device configuration
  static_assert(std::is_same<typename MuxT::PinIdT, PinId>::value,
                "PinIdT must be PinId when kDeviceCount == 1");

  // setPinsMode with PinIdArray
  PinIdArray<2> pins01{PinId{0}, PinId{1}};
  mux.setPinsMode(pins01, PinMode::Output);
  TEST_ASSERT_EQUAL_INT((int)PinMode::Output, (int)dev.modes[0]);
  TEST_ASSERT_EQUAL_INT((int)PinMode::Output, (int)dev.modes[1]);

  // writePins with PinIdArray
  PinIdArray<2> pins23{PinId{2}, PinId{3}};
  mux.writePins(pins23, true);
  TEST_ASSERT_TRUE(dev.state.test(2));
  TEST_ASSERT_TRUE(dev.state.test(3));
  mux.writePins(pins23, false);
  TEST_ASSERT_FALSE(dev.state.test(2));
  TEST_ASSERT_FALSE(dev.state.test(3));

  // writeGroupPin (group is ignored = 0 for single device)
  mux.writeGroupPin(PinGroupId{0}, PinId{4}, true);
  TEST_ASSERT_TRUE(dev.state.test(4));

  // readGroupPins with PinIdArray
  mux.writeGroupPin(PinGroupId{0}, PinId{2}, true);
  mux.writeGroupPin(PinGroupId{0}, PinId{3}, false);
  mux.writeGroupPin(PinGroupId{0}, PinId{4}, true);
  PinIdArray<3> readPinsArr{PinId{2}, PinId{3}, PinId{4}};
  auto bitsA = mux.readGroupPins(readPinsArr);
  TEST_ASSERT_TRUE(bitsA.test(0));   // pin 2
  TEST_ASSERT_FALSE(bitsA.test(1));  // pin 3
  TEST_ASSERT_TRUE(bitsA.test(2));   // pin 4

  // readGroupPins with GroupPinArray (should also work and route to group 0)
  GroupPinArray<2> grp{PinGroupId{0}, std::array<PinId::ValueType, 2>{6, 7}};
  mux.writeGroupPin(PinGroupId{0}, PinId{6}, true);
  mux.writeGroupPin(PinGroupId{0}, PinId{7}, false);
  auto bitsB = mux.readGroupPins(grp);
  TEST_ASSERT_TRUE(bitsB.test(0));   // pin 6
  TEST_ASSERT_FALSE(bitsB.test(1));  // pin 7
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_single_device_pinmux_basic);
  return UNITY_END();
}
