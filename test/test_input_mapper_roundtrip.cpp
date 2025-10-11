#include "platform/test.h"
// Needs to come first

#include "ui/InputMapper.h"

using namespace zlkm::ui;

namespace mappers_tests {
struct LinLim {
  static constexpr float outMin() { return 0.f; }
  static constexpr float outMax() { return 1.f; }
};
struct IntLim {
  static constexpr float outMin() { return 0.f; }
  static constexpr float outMax() { return 10.f; }
};
struct RateLim {
  static constexpr float outMin() { return 5.f; }
  static constexpr float outMax() { return 500.f; }
};
struct ExpPol {
  static constexpr float EXP = 2.0f;
};

void test_linear_roundtrip() {
  float v = 0.0f;
  auto im = LinearMapper<float, LinLim>::make(&v);
  for (int raw : {0, 512, 2048, 4095}) {
    im.mapAndSet(raw);
    int back = im.reverseMap();
    TEST_ASSERT_INT_WITHIN(32, raw, back);
  }
}

void test_exp_roundtrip() {
  float v = 0.0f;
  auto im = ExpPowMapper<float, LinLim, ExpPol>::make(&v);
  // Avoid raw values that collapse to 0 after pow(x, EXP) then sticky-ends.
  // For EXP=2.0, threshold ~ sqrt(0.05) * 4095 ≈ 916.
  for (int raw : {0, 1024, 2048, 3072, 4095}) {
    im.mapAndSet(raw);
    int back = im.reverseMap();
    TEST_ASSERT_INT_WITHIN(48, raw, back);
  }
}

void test_db_mapper() {
  float amp = 0.0f;
  struct DbLim {
    static constexpr float outMin() { return -60.f; }
    static constexpr float outMax() { return 0.f; }
  };
  auto im = DbMapper<DbLim>::make(&amp);
  im.mapAndSet(0);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, powf(10.f, -60.f * 0.05f), amp);
  im.mapAndSet(InputMapper::kMaxRawValue);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, amp);
}

void test_rate_roundtrip() {
  float rate = 0.0f;
  auto im = RateMapper<RateLim, 48000>::make(&rate);
  for (int raw : {0, 800, 1600, 2400, 4095}) {
    im.mapAndSet(raw);
    int back = im.reverseMap();
    TEST_ASSERT_INT_WITHIN(64, raw, back);
  }
}

void test_int_mapper() {
  int i = 0;
  auto im = IntMapper<IntLim>::make(&i);
  im.mapAndSet(0);
  TEST_ASSERT_EQUAL(0, i);
  im.mapAndSet(InputMapper::kMaxRawValue);
  TEST_ASSERT_EQUAL(10, i);
  im.mapAndSet(2048);
  TEST_ASSERT(i >= 5 && i <= 6);
  int back = im.reverseMap();
  TEST_ASSERT(back >= 0 && back <= InputMapper::kMaxRawValue);
}

void test_bool_mapper() {
  bool b = false;
  struct Thr {
    static constexpr float thresh() { return 0.5f; }
  };
  auto im = BoolMapper<Thr>::make(&b);
  im.mapAndSet(0);
  TEST_ASSERT_FALSE(b);
  im.mapAndSet(InputMapper::kMaxRawValue);
  TEST_ASSERT_TRUE(b);
}

}  // namespace mappers_tests

// Grouped test entry to be called from the harness
void test_mappers() {
  using namespace mappers_tests;
  RUN_TEST(test_linear_roundtrip);
  RUN_TEST(test_exp_roundtrip);
  RUN_TEST(test_db_mapper);
  RUN_TEST(test_rate_roundtrip);
  RUN_TEST(test_int_mapper);
  RUN_TEST(test_bool_mapper);
}
