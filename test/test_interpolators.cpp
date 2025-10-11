#include "platform/test.h"
// Needs to come first

#include "mod/BlockInterpolator.h"

using namespace zlkm::mod;

namespace interp_tests {

void test_block_interpolator_n_progress() {
  constexpr int BS = 8;
  float src[2] = {0.0f, 1.0f};
  std::array<float, 2> targets = {1.0f, 0.0f};
  auto bi = makeBlockInterpolator<BS>(&src[0], targets);
  // One update should move both values by 1/BS
  bi.update();
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / BS, src[0]);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f - 1.0f / BS, src[1]);
}

void test_block_interpolator_n_final() {
  constexpr int BS = 16;
  float src[3] = {0.0f, 0.5f, -1.0f};
  std::array<float, 3> targets = {1.0f, -0.5f, 1.0f};
  auto bi = makeBlockInterpolator<BS>(&src[0], targets);
  for (int i = 0; i < BS; ++i) bi.update();
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, src[0]);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, src[1]);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, src[2]);
}

}  // namespace interp_tests

void test_interpolators() {
  using namespace interp_tests;
  RUN_TEST(test_block_interpolator_n_progress);
  RUN_TEST(test_block_interpolator_n_final);
}
