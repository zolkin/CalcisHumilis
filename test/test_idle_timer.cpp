#include <stdint.h>

#include "platform/test.h"
#include "util/IdleTimer.h"

using zlkm::util::IdleTimer;

namespace idle_tests {

// We can’t control millis() directly here, so we test the logic around
// isIdle(now) with synthetic 'now' values relative to construction.

void test_idle_false_before_threshold_true_after() {
  // 100 ms timeout
  IdleTimer idle(100);
  // Immediately after construction, using now=0 would be wrong; instead use
  // relative checks We simulate that 'last_' was set at construction time (t0).
  // For now < t0+100, isIdle should be false. We can only call isIdle(now); so
  // we assume now=last_ + delta.

  // Since we don't know last_ value, test monotonic behavior around a chosen
  // reference For correctness: isIdle(last_ + 99) == false, isIdle(last_ + 100)
  // == true. We'll probe with two deltas and rely on the inequality check. We
  // can't read last_, but the implementation compares (now - last_) >= timeout.
  // So using any now where (now - last_) = 99 should be false, and 100 should
  // be true.

  // We can’t construct such 'now' exactly without last_; however, calling
  // noteActivity() sets last_ = millis(), and immediately calling
  // isIdle(millis()+k) gives delta≈k.

  idle.noteActivity();
  const uint32_t base = millis();
  TEST_ASSERT_FALSE(idle.isIdle(base + 99));
  TEST_ASSERT_TRUE(idle.isIdle(base + 100));
}

void test_note_activity_resets_timer() {
  IdleTimer idle(50);
  idle.noteActivity();
  uint32_t base = millis();
  TEST_ASSERT_FALSE(idle.isIdle(base + 49));
  TEST_ASSERT_TRUE(idle.isIdle(base + 50));
  // Reset activity, should become non-idle again for another window
  idle.noteActivity();
  base = millis();
  TEST_ASSERT_FALSE(idle.isIdle(base + 49));
}

}  // namespace idle_tests

void test_idle_timer() {
  using namespace idle_tests;
  RUN_TEST(test_idle_false_before_threshold_true_after);
  RUN_TEST(test_note_activity_resets_timer);
}
