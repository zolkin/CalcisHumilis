#pragma once
#include <atomic>
#include <mutex>

namespace zlkm::util {

// Template on the atomic class (defaults to std::atomic).
// Pass std::atomic explicitly if you like: zlkm::spin_lock<std::atomic>
template <template <class> class Atomic = std::atomic>
class spin_lock_t {
  Atomic<bool> locked{false};

 public:
  spin_lock_t() = default;
  spin_lock_t(const spin_lock_t&) = delete;
  spin_lock_t& operator=(const spin_lock_t&) = delete;

  // BasicLockable
  void lock() {
    bool expected = false;
    while (!locked.compare_exchange_weak(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = false;  // reset for next attempt
    }
  }

  // Lockable (optional, but handy)
  bool try_lock() {
    bool expected = false;
    return locked.compare_exchange_strong(
        expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  }

  void unlock() {
    locked.store(false, std::memory_order_release);
    // no notify; pure spinner by design
  }
};

using spin_lock = spin_lock_t<>;
using sl_guard = std::lock_guard<spin_lock>;

}  // namespace zlkm