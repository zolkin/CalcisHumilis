#include "platform/test.h"
// Needs to come first

#include "..\src\audio\processors\DJFilter.h"

using namespace zlkm::audio;

namespace filter_tests {

static constexpr int SR = 48000;
using Filter = DJFilterTPT<SR>;
using Safe = SafeFilterParams<SR>;

void test_morph_weights() {
  Filter::Cfg cfg{};
  Safe p(&cfg, 0.5f, 0.0f, 0.0f, 0.0f);

  p.setMorph01(0.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, cfg.hpWeight);
  TEST_ASSERT(cfg.lpWeight >= 0.0f);

  p.setMorph01(1.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, cfg.lpWeight);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, cfg.hpWeight);
}

void test_cutoff_monotonic() {
  Filter::Cfg cfg{};
  Safe p(&cfg, 0.1f, 0.0f, 0.0f, 0.0f);
  float g1 = cfg.gCut;
  p.setCutoff01(0.9f);
  float g2 = cfg.gCut;
  TEST_ASSERT(g2 > g1);
}

void test_resonance_effect() {
  Filter::Cfg cfg{};
  Safe p(&cfg, 0.2f, 0.0f, 0.0f, 0.0f);
  float k0 = cfg.kDamp;
  p.setRes01(1.0f);
  float k1 = cfg.kDamp;
  TEST_ASSERT(k1 < k0);
}

void test_drive_monotonic() {
  Filter::Cfg cfg{};
  Safe p(&cfg, 0.4f, 0.2f, 0.0f, 0.5f);
  float d0 = cfg.drive;
  p.setDrive01(1.0f);
  float d1 = cfg.drive;
  TEST_ASSERT(d1 > d0);
}

}  // namespace filter_tests

void test_filter_params() {
  using namespace filter_tests;
  RUN_TEST(test_morph_weights);
  RUN_TEST(test_cutoff_monotonic);
  RUN_TEST(test_resonance_effect);
  RUN_TEST(test_drive_monotonic);
}
