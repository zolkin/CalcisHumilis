#include "platform/test.h"
// Needs to come first

#include "mod/Parameters.h"

using namespace zlkm::mod;

namespace mappers_tests {
using LinLim = ZLKM_DEFINE_MIN_MAX_F(0.f, 1.f);
using IntLim = ZLKM_DEFINE_MIN_MAX_F(0.f, 10.f);
using RateLim = ZLKM_DEFINE_MIN_MAX_F(5.f, 500.f);

void test_linear_roundtrip() {
  float v = 0.0f;
  auto im = LinearMapMod<float, LinLim>::mapper(&v);
  for (int raw : {0, 512, 2048, 4095}) {
    im.mapAndSet(raw);
    int back = im.reverseMap();
    TEST_ASSERT_INT_WITHIN(32, raw, back);
  }
}

void test_db_mapper() {
  float amp = 0.0f;
  using DbLim = ZLKM_DEFINE_MIN_MAX_F(-60.f, 0.f);
  auto im = DbMapMod<DbLim>::mapper(&amp);
  im.mapAndSet(0);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, powf(10.f, -60.f * 0.05f), amp);
  im.mapAndSet(ParamInputMapper::kMaxRawValue);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.f, amp);
}

void test_rate_roundtrip() {
  float rate = 0.0f;
  auto im = RateMapMod<RateLim, 48000>::mapper(&rate);
  for (int raw : {0, 800, 1600, 2400, 4095}) {
    im.mapAndSet(raw);
    int back = im.reverseMap();
    TEST_ASSERT_INT_WITHIN(64, raw, back);
  }
}

void test_int_mapper() {
  int i = 0;
  auto im = IntMapMod<IntLim>::mapper(&i);
  im.mapAndSet(0);
  TEST_ASSERT_EQUAL(0, i);
  im.mapAndSet(ParamInputMapper::kMaxRawValue);
  TEST_ASSERT_EQUAL(10, i);
  im.mapAndSet(2048);
  TEST_ASSERT(i >= 5 && i <= 6);
  int back = im.reverseMap();
  TEST_ASSERT(back >= 0 && back <= ParamInputMapper::kMaxRawValue);
}

void test_bool_mapper() {
  bool b = false;
  struct Thr {
    static constexpr float thresh() { return 0.5f; }
  };
  auto im = BoolMapMod<Thr>::mapper(&b);
  im.mapAndSet(0);
  TEST_ASSERT_FALSE(b);
  im.mapAndSet(ParamInputMapper::kMaxRawValue);
  TEST_ASSERT_TRUE(b);
}

}  // namespace mappers_tests

// Grouped test entry to be called from the harness
void test_mappers() {
  using namespace mappers_tests;
  RUN_TEST(test_linear_roundtrip);
  RUN_TEST(test_db_mapper);
  RUN_TEST(test_rate_roundtrip);
  RUN_TEST(test_int_mapper);
  RUN_TEST(test_bool_mapper);
}
