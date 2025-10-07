#pragma once

#include <pico/time.h>

#include <array>
#include <atomic>

#include "hw/io/QuadManagerIO.h"

namespace zlkm::ui {

// Schedules periodic sampling via a repeating hardware timer. The timer ISR
// only sets a flag; actual I2C reads are done on the caller thread by tick().
// Accumulates encoder count deltas atomically for consumption on another core.
template <typename PinExpanderT, int N>
class InputSampler {
 public:
  using Encoders = hw::io::QuadManagerIO<PinExpanderT, N>;

  struct Cfg {
    typename Encoders::Cfg encCfg{};  // pins and pullups
    uint32_t pollUs = 1000;           // desired poll period (microseconds)
  };

  InputSampler(PinExpanderT& pins, const Cfg& cfg)
      : pins_(pins), encs_(pins_, cfg.encCfg), pollUs_(cfg.pollUs) {
    // Prime baselines
    for (int i = 0; i < N; ++i) lastCounts_[i] = encs_.read(i);
    startTimer_();
  }

  ~InputSampler() { cancelTimer_(); }

  // Call on a non-ISR thread frequently (e.g., in the UI loop). If the timer
  // flagged a sampling moment, perform one encoder scan and accumulate deltas.
  inline void tick() {
    if (!due_.exchange(false, std::memory_order_acq_rel)) return;
    encs_.update();
    for (int i = 0; i < N; ++i) {
      const int32_t now = encs_.read(i);
      const int32_t d = now - lastCounts_[i];
      if (d != 0) {
        lastCounts_[i] = now;
        deltas_[i].fetch_add(d, std::memory_order_relaxed);
      }
    }
  }

  // Atomically fetch and reset the accumulated delta counts for encoder i.
  inline int32_t consumeDeltaCounts(int i) {
    return deltas_[i].exchange(0, std::memory_order_acq_rel);
  }

  // Reset baselines and clear pending deltas (e.g., when changing page/tab)
  inline void resetBaselines() {
    encs_.update();
    for (int i = 0; i < N; ++i) {
      lastCounts_[i] = encs_.read(i);
      deltas_[i].store(0, std::memory_order_relaxed);
    }
  }

  // Optional: update polling period
  inline void setPollUs(uint32_t us) {
    pollUs_ = us ? us : 1000;
    restartTimer_();
  }

 private:
  PinExpanderT& pins_;
  Encoders encs_;
  std::array<std::atomic<int32_t>, N> deltas_{};
  std::array<int32_t, N> lastCounts_{};

  repeating_timer_t timer_{};
  uint32_t pollUs_ = 1000;
  std::atomic<bool> due_{false};

  static bool onTimer_(repeating_timer_t* t) {
    auto* self = reinterpret_cast<InputSampler*>(t->user_data);
    self->due_.store(true, std::memory_order_release);
    return true;  // keep repeating
  }

  void startTimer_() {
    cancelTimer_();
    add_repeating_timer_us(-(int)pollUs_, &InputSampler::onTimer_, this,
                           &timer_);
  }

  void restartTimer_() { startTimer_(); }

  void cancelTimer_() {
    if (timer_.alarm_id != 0) cancel_repeating_timer(&timer_);
    timer_.alarm_id = 0;
  }
};

}  // namespace zlkm::ui
