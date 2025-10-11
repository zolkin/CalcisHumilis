#include "platform/test.h"
// Needs to come first

#include "mod/ADEnvelopes.h"

using namespace zlkm::mod;

namespace ad_tests {

void test_trigger_and_attack() {
  ADEnvelopes<2> env;
  env.cfg().resetToZeroOnTrigger = true;
  env.setRates(0, 0.2f, 0.1f);
  env.trigger(0);
  // Attack should raise towards 1.0
  for (int i = 0; i < 3; ++i) env.update();
  TEST_ASSERT(env.valueRaw(0) > 0.0f);
}

void test_reaches_decay_and_finishes() {
  ADEnvelopes<1> env;
  env.cfg().peakThresh = 0.6f;
  env.cfg().floorThresh = 0.05f;
  env.setRates(0, 0.3f, 0.2f);
  env.trigger(0);
  // Few updates to cross peak, then decay
  for (int i = 0; i < 10; ++i) env.update();
  TEST_ASSERT_FALSE(env.isActive(0));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, env.valueRaw(0));
}

void test_depth_scaling() {
  ADEnvelopes<1> env;
  env.setRates(0, 1.0f, 1.0f);
  env.setDepth(0, 0.25f);
  env.trigger(0);
  env.update();
  TEST_ASSERT(env.value(0) <= 0.25f);
}

}  // namespace ad_tests

void test_ad_envelopes() {
  using namespace ad_tests;
  RUN_TEST(test_trigger_and_attack);
  RUN_TEST(test_reaches_decay_and_finishes);
  RUN_TEST(test_depth_scaling);
}
