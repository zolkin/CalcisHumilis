#include <array>
#include <bitset>

#include "./FakePins.h"
#include "hw/io/QuadManagerIO.h"
#include "platform/test.h"

using namespace zlkm::hw::io;
using namespace zlkm::test;

namespace quad_tests {

static void setAB(FakePins& pins, uint8_t aPin, uint8_t bPin, uint8_t ab) {
  pins.setPin(PinId{aPin}, (ab & 0b10) != 0);
  pins.setPin(PinId{bPin}, (ab & 0b01) != 0);
}

void test_single_cw_cycle_counts_positive() {
  FakePins fp;
  // Encoder 0 uses pins A=0, B=1
  using QM = QuadManagerIO<FakePins, 1>;
  QM::Cfg cfg{.pins{PinGroupId{0}, {0, 1}}};

  // Initialize to 00
  setAB(fp, 0, 1, 0b00);
  QM qm(fp, cfg);

  // CW sequence (00 -> 01 -> 11 -> 10 -> 00)
  const uint8_t seq[] = {0b00, 0b01, 0b11, 0b10, 0b00};
  for (uint8_t s : seq) {
    setAB(fp, 0, 1, s);
    qm.update();
  }
  int d = qm.consumeDeltaCounts(0);
  // Expect +4 (one per edge)
  TEST_ASSERT_EQUAL(4, d);
}

void test_ccw_cycle_counts_negative() {
  FakePins fp;
  using QM = QuadManagerIO<FakePins, 1>;
  QM::Cfg cfg{.pins{PinGroupId{0}, {2, 3}}};
  setAB(fp, 2, 3, 0b00);
  QM qm(fp, cfg);

  // CCW sequence (00 -> 10 -> 11 -> 01 -> 00)
  const uint8_t seq[] = {0b00, 0b10, 0b11, 0b01, 0b00};
  for (uint8_t s : seq) {
    setAB(fp, 2, 3, s);
    qm.update();
  }
  int d = qm.consumeDeltaCounts(0);
  TEST_ASSERT_EQUAL(-4, d);
}

void test_illegal_jump_ignored() {
  FakePins fp;
  using QM = QuadManagerIO<FakePins, 1>;
  QM::Cfg cfg{.pins{PinGroupId{0}, {4, 5}}};
  setAB(fp, 4, 5, 0b00);
  QM qm(fp, cfg);

  // Illegal jump 00 -> 11 -> 00 should not contribute
  setAB(fp, 4, 5, 0b11);
  qm.update();
  setAB(fp, 4, 5, 0b00);
  qm.update();
  int d = qm.consumeDeltaCounts(0);
  TEST_ASSERT_EQUAL(0, d);
}

}  // namespace quad_tests

void test_quad_manager() {
  using namespace quad_tests;
  RUN_TEST(test_single_cw_cycle_counts_positive);
  RUN_TEST(test_ccw_cycle_counts_negative);
  RUN_TEST(test_illegal_jump_ignored);
}
