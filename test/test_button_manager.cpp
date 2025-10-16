#include <array>
#include <bitset>

#include "hw/io/ButtonManager.h"
#include "hw/io/Types.h"
#include "platform/test.h"

using namespace zlkm::hw::io;

namespace button_tests {

struct FakePins {
  std::array<bool, 16> level{};  // index by raw pin id

  void setPin(uint8_t id, bool high) { level[id] = high; }

  void setPinMode(PinId /*id*/, PinMode /*m*/) {}

  template <size_t K>
  std::bitset<K> readPins(const std::array<PinId, K>& idxs) const {
    std::bitset<K> out;
    for (size_t i = 0; i < K; ++i) out.set(i, level[idxs[i].value]);
    return out;
  }
};

void test_debounce_rising_after_threshold() {
  FakePins fp;
  using BM = ButtonManager<1, FakePins>;
  BM::Cfg cfg{};
  cfg.pins[0] = PinId{0};  // one button on raw pin 0
  cfg.activeLow = true;    // pressed = LOW on the wire
  cfg.usePullUp = false;   // irrelevant for fake
  cfg.debounceTicks = 3;   // require 3 stable samples

  // Idle high (not pressed)
  fp.setPin(0, true);
  BM bm(fp, cfg);

  // Noisy press: H L H L L L -> should only trigger once after 3 L's
  auto sample = [&](bool high) {
    fp.setPin(0, high);
    return bm.tick();
  };

  auto r1 = sample(true);   // H
  auto r2 = sample(false);  // L (cnt=1)
  auto r3 = sample(true);   // H (cnt resets)
  auto r4 = sample(false);  // L (cnt=1)
  auto r5 = sample(false);  // L (cnt=2)
  auto r6 = sample(false);  // L (cnt=3) -> stable change, rising

  TEST_ASSERT_FALSE(r1.rising.test(0));
  TEST_ASSERT_FALSE(r2.rising.test(0));
  TEST_ASSERT_FALSE(r3.rising.test(0));
  TEST_ASSERT_FALSE(r4.rising.test(0));
  TEST_ASSERT_FALSE(r5.rising.test(0));
  TEST_ASSERT_TRUE(r6.rising.test(0));
  TEST_ASSERT_TRUE(r6.pressed.test(0));
}

void test_debounce_falling_after_threshold() {
  FakePins fp;
  using BM = ButtonManager<1, FakePins>;
  BM::Cfg cfg{};
  cfg.pins[0] = PinId{1};
  cfg.activeLow = true;
  cfg.debounceTicks = 2;

  // Start pressed: wire LOW because activeLow=true
  fp.setPin(1, false);
  BM bm(fp, cfg);

  auto sample = [&](bool high) {
    fp.setPin(1, high);
    return bm.tick();
  };

  auto r1 = sample(true);  // H (cnt=1)
  auto r2 = sample(true);  // H (cnt=2) -> falling
  TEST_ASSERT_TRUE(r2.falling.test(0));
  TEST_ASSERT_FALSE(r2.pressed.test(0));
}

}  // namespace button_tests

void test_button_manager() {
  using namespace button_tests;
  RUN_TEST(test_debounce_rising_after_threshold);
  RUN_TEST(test_debounce_falling_after_threshold);
}
