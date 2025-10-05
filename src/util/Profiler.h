// Profiler.h
#pragma once
#include <Arduino.h>
#include <ArduinoLog.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace zlkm::util {

// ---------- Profiler singleton (avg + min + max) ----------
class Profiler {
  static constexpr uint16_t kMaxCounters = 32;

 public:
  static Profiler& instance() {
    static Profiler inst;
    return inst;
  }

  // Register a counter once (static at call site recommended)
  uint16_t ensure_counter(const char* name, uint32_t logNth) {
    const uint16_t id = nextId_.fetch_add(1, std::memory_order_relaxed);
    if (id >= kMaxCounters) return kInvalidId;
    Counter& c = counters_[id];
    c.name = name;
    c.logNth = logNth;
    return id;
  }

  // Producers (any core): record one duration in microseconds
  inline void record(uint16_t id, uint32_t dt_us) {
    if (id == kInvalidId) return;
    Counter& c = counters_[id];

    c.count.fetch_add(1, std::memory_order_relaxed);
    c.sumUs.fetch_add(dt_us, std::memory_order_relaxed);

    // Double-buffered min/max for interval stats
    const uint32_t slot = c.epoch.load(std::memory_order_relaxed) & 1u;

    // atomic max
    {
      auto& amax = c.maxUs[slot];
      uint32_t prev = amax.load(std::memory_order_relaxed);
      while (prev < dt_us && !amax.compare_exchange_weak(
                                 prev, dt_us, std::memory_order_relaxed)) {
      }
    }
    // atomic min
    {
      auto& amin = c.minUs[slot];
      uint32_t prev = amin.load(std::memory_order_relaxed);
      while (dt_us < prev && !amin.compare_exchange_weak(
                                 prev, dt_us, std::memory_order_relaxed)) {
      }
    }
  }

  // UI thread: roll up and log interval stats periodically
  void tick_and_log(uint32_t nowMs, uint32_t minPeriodMs = 2000) {
    const uint16_t n = nextId_.load(std::memory_order_relaxed);
    for (uint16_t i = 0; i < n && i < kMaxCounters; ++i) {
      Counter& c = counters_[i];

      const uint32_t count = c.count.load(std::memory_order_relaxed);
      const uint64_t sumUs = c.sumUs.load(std::memory_order_relaxed);

      const uint32_t dCount = count - c.lastCount;
      const uint64_t dSum = sumUs - c.lastSumUs;
      const bool timeDue = (nowMs - c.lastLogMs) >= minPeriodMs;

      if (dCount >= c.logNth || (timeDue && dCount > 0)) {
        // Flip epoch so producers switch slots, then read/reset the old slot.
        const uint32_t curEpoch = c.epoch.load(std::memory_order_relaxed);
        const uint32_t oldIdx = (curEpoch & 1u);
        c.epoch.store(curEpoch + 1, std::memory_order_release);

        const uint32_t intMax = c.maxUs[oldIdx].load(std::memory_order_relaxed);
        const uint32_t intMin = c.minUs[oldIdx].load(std::memory_order_relaxed);

        // Reset old slot for the next time it becomes "current"
        c.maxUs[oldIdx].store(0, std::memory_order_relaxed);
        c.minUs[oldIdx].store(kInitMin, std::memory_order_relaxed);

        const uint32_t avgUs = static_cast<uint32_t>(dSum / dCount);

        Log.infoln("[perf] %s avg=%uus  (min=%u us, max=%u us, N=%lu)",
                   c.name ? c.name : "(unnamed)", avgUs, intMin, intMax,
                   (unsigned long)dCount);

        // advance snapshots
        c.lastCount = count;
        c.lastSumUs = sumUs;
        c.lastLogMs = nowMs;
      }
    }
  }

  static constexpr uint16_t invalid_id() { return kInvalidId; }

 private:
  Profiler() = default;

  static constexpr uint16_t kInvalidId = 0xFFFF;
  static constexpr uint32_t kInitMin = 0xFFFFFFFFu;  // "no min yet"

  struct Counter {
    const char* name = nullptr;
    uint32_t logNth = 1000;

    // producers update these atomically
    std::atomic<uint32_t> count{0};
    std::atomic<uint64_t> sumUs{0};
    std::atomic<uint32_t> epoch{0};            // selects active min/max slot
    std::atomic<uint32_t> maxUs[2]{{0}, {0}};  // per-interval max
    std::atomic<uint32_t> minUs[2]{{kInitMin}, {kInitMin}};  // per-interval min

    // UI thread snapshot
    uint32_t lastCount = 0;
    uint64_t lastSumUs = 0;
    uint32_t lastLogMs = 0;
  };

  std::array<Counter, kMaxCounters> counters_{};
  std::atomic<uint16_t> nextId_{0};
};

// ---------- Template-scoped counter + RAII scope ----------
template <int LOG_NTH>
class PerfCounter {
 public:
  explicit PerfCounter(const char* name)
      : id_(Profiler::instance().ensure_counter(name, LOG_NTH)) {}

  class Scope {
   public:
    explicit Scope(uint16_t id) : id_(id), t0_(micros()) {}
    ~Scope() {
      if (id_ != Profiler::invalid_id()) {
        const uint32_t dt = static_cast<uint32_t>(micros() - t0_);  // wrap-safe
        Profiler::instance().record(id_, dt);
      }
    }

   private:
    uint16_t id_;
    uint32_t t0_;
  };

  inline Scope scope() const { return Scope(id_); }

 private:
  uint16_t id_;
};

}  // namespace zlkm::util


// ---------- Convenience macros ----------
#define ZLKM_PERF_SCOPE_N(NAME, N)                               \
  static ::zlkm::util::PerfCounter<N> _zlkm_pc_##__LINE__{NAME}; \
  auto _zlkm_scope_##__LINE__ = _zlkm_pc_##__LINE__.scope()

#define ZLKM_PERF_SCOPE(NAME) ZLKM_PERF_SCOPE_N(NAME, 1000)