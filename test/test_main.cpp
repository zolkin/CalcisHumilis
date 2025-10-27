#include "platform/test.h"

// Declaration from test files
void test_mappers();
void test_ad_envelopes();
void test_interpolators();
void test_quad_manager();
void test_button_manager();
void test_idle_timer();

void setUp(void) {}
void tearDown(void) {}

TEST_MAIN() {
  PLATFORM_TEST_BEGIN();

  UNITY_BEGIN();
  test_mappers();
  test_ad_envelopes();
  test_interpolators();
  test_quad_manager();
  test_button_manager();
  test_idle_timer();
  UNITY_END();
}
