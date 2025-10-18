#pragma once

// ==== Build-time config (override via build_flags) ====
#ifndef ZLKM_PROFILE_PERIOD_MS
#define ZLKM_PROFILE_PERIOD_MS 20000  // print snapshot every N ms
#endif
#ifndef ZLKM_PROFILE_EMIT_THREAD
#define ZLKM_PROFILE_EMIT_THREAD \
  0  // which thread prints (bind UI/core at runtime if you prefer)
#endif
#ifndef ZLKM_PROFILE_MAX_THREADS
#define ZLKM_PROFILE_MAX_THREADS 2
#endif
#ifndef ZLKM_PROFILE_MAX_COUNTERS
#define ZLKM_PROFILE_MAX_COUNTERS 32
#endif

#ifndef ZLKM_PROFILE_MAX_DEPTH
#define ZLKM_PROFILE_MAX_DEPTH 16
#endif

// Hot-path: use 32-bit sums by default (wrap-safe for window deltas)
#ifndef ZLKM_PROFILE_32BIT_SUMS
#define ZLKM_PROFILE_32BIT_SUMS 1
#endif

// Max tracking (double-buffered). Set to 0 to remove all max bookkeeping.
#ifndef ZLKM_PROFILE_ENABLE_MAX
#define ZLKM_PROFILE_ENABLE_MAX 1
#endif

#if defined(PROFILE)

#define ZLKM_PROFILE_ASCII_TREE 0  // 1 = ASCII (+- `- |), 0 = Unicode (├─ └─ │)

#if ZLKM_PROFILE_ASCII_TREE
#define T_VERT "|  "
#define T_SPACE "   "
#define T_TEE "+- "
#define T_ELB "`- "
#else  // ZLKM_PROFILE_ASCII_TREE
#define T_VERT "│  "
#define T_SPACE "   "
#define T_TEE "├─ "
#define T_ELB "└─ "
#endif  // ZLKM_PROFILE_ASCII_TREE

#define ZLKM_PROFILE_PREFIX_MAX (ZLKM_PROFILE_MAX_DEPTH * 6 + 8)

#include <array>
#include <cstdint>
#include <type_traits>

#include "platform/platform.h"

namespace zlkm::util {

// mask type auto-sizes to keep bit ops fast
#if (ZLKM_PROFILE_MAX_COUNTERS <= 32)
using mask_t = uint32_t;
#define ZLKM_MASK_ONE (uint32_t)1u
#define ZLKM_CTZ(x) __builtin_ctz(x)
#else
using mask_t = uint64_t;
#define ZLKM_MASK_ONE (uint64_t)1ull
#define ZLKM_CTZ(x) __builtin_ctzll(x)
#endif

#if ZLKM_PROFILE_32BIT_SUMS
using sum_t = uint32_t;
#else
using sum_t = uint64_t;
#endif

class Profiler {
 public:
  struct Config {
    uint32_t period_ms = ZLKM_PROFILE_PERIOD_MS;
    uint8_t emit_tid = ZLKM_PROFILE_EMIT_THREAD;  // only this thread prints
  };

  using ThreadIndexFn =
      uint8_t (*)();  // return [0..ZLKM_PROFILE_MAX_THREADS-1]

  static Profiler& instance() {
    static Profiler inst;
    return inst;
  }

  // ---- Setup / knobs ----
  void setup(const Config& cfg) {
    cfg_ = cfg;
    const uint32_t now = millis();
    for (auto& T : threads_) {
      T.lastLogMs = now;
      T.active = false;
      T.nextId = 0;
      for (auto& lp : T.lastPrintMs) lp = 0;
      for (auto& n : T.names) n = nullptr;
      for (auto& c : T.perCounter) c = ThreadCounter{};
      for (auto& m : T.childrenMask) m = 0;
      T.depth = 0;
    }
  }
  void set_thread_index_fn(ThreadIndexFn fn) { threadIndexFn_ = fn; }
  void set_emit_thread(uint8_t tid) {
    cfg_.emit_tid = (tid < ZLKM_PROFILE_MAX_THREADS) ? tid : 0;
  }

  // Per-thread registration (fail-fast on capacity)
  uint16_t ensure_counter_for_thread(const char* name, uint8_t tid) {
    Thread& T = threads_[tid];
    // Reuse by pointer-equality (string literals)
    for (uint16_t i = 0; i < T.nextId; ++i)
      if (T.names[i] == name) return i;
    if (T.nextId >= ZLKM_PROFILE_MAX_COUNTERS)
      fatal_("counter capacity", name, tid);
    const uint16_t id = T.nextId++;
    T.names[id] = name;
    T.active = true;  // mark thread as active once on registration
    return id;
  }

  inline void on_scope_enter(uint16_t id, uint8_t tid) {
    Thread& T = threads_[tid];

    uint16_t parent = 0xFFFF;
    if (T.depth < ZLKM_PROFILE_MAX_DEPTH) {
      if (T.depth > 0) parent = T.stack[T.depth - 1];
      T.stack[T.depth++] = id;
    }
    if (parent != 0xFFFF) {
      const mask_t bit = (mask_t)(ZLKM_MASK_ONE << id);
      if ((T.childrenMask[parent] & bit) == 0) T.childrenMask[parent] |= bit;
    }
  }

  // Unconditional LIFO pop with debug-only check
  inline void on_scope_exit(uint16_t id, uint32_t dt_us, uint8_t tid) {
    Thread& T = threads_[tid];
#ifndef NDEBUG
    if (T.depth == 0 || T.stack[T.depth - 1] != id) {
      Log.warningln("[perf] non-LIFO exit (id=%u) on T%u", (unsigned)id,
                    (unsigned)tid);
      if (T.depth) --T.depth;  // attempt to keep stack moving in debug
    } else {
      --T.depth;
    }
#else
    --T.depth;
#endif
    ThreadCounter& tc = T.perCounter[id];
    ++tc.count;
    tc.sumUs += dt_us;
#if ZLKM_PROFILE_ENABLE_MAX
    const uint32_t slot = (tc.epoch & 1u);
    if (dt_us > tc.maxUs[slot]) tc.maxUs[slot] = dt_us;
#endif
  }

  inline void on_scope_exit_weighted(uint16_t id, uint32_t dt_us,
                                     uint32_t weight, uint8_t tid) {
    Thread& T = threads_[tid];
#ifndef NDEBUG
    if (T.depth == 0 || T.stack[T.depth - 1] != id) {
      Log.warningln("[perf] non-LIFO exit (weighted, id=%u) on T%u",
                    (unsigned)id, (unsigned)tid);
      if (T.depth) --T.depth;
    } else {
      --T.depth;
    }
#else
    --T.depth;
#endif
    ThreadCounter& tc = T.perCounter[id];
    tc.count += weight;
    tc.sumUs += (sum_t)(dt_us * weight);
#if ZLKM_PROFILE_ENABLE_MAX
    const uint32_t slot = (tc.epoch & 1u);
    if (dt_us > tc.maxUs[slot]) tc.maxUs[slot] = dt_us;
#endif
  }

  // ---- Emit periodic snapshots (call from loops) ----
  void tick_and_log() {
    if (current_tid_() != cfg_.emit_tid) return;
    const uint32_t now = millis();

    for (uint8_t tid = 0; tid < ZLKM_PROFILE_MAX_THREADS; ++tid) {
      Thread& T = threads_[tid];
      if (!T.active) continue;
      if (cfg_.period_ms == 0 || (now - T.lastLogMs) < cfg_.period_ms) continue;

      // used = counters with activity this window
      mask_t used = 0;
      for (uint16_t id = 0; id < T.nextId; ++id) {
        const ThreadCounter& tc = T.perCounter[id];
        if ((tc.count - tc.lastCount) != 0)
          used |= (mask_t)(ZLKM_MASK_ONE << id);
      }

      // Promote parents of used nodes
      mask_t promote = 0, tmp = used;
      while (tmp) {
        const uint16_t c = (uint16_t)ZLKM_CTZ(tmp);
        tmp &= (tmp - 1);
        for (uint16_t p = 0; p < T.nextId; ++p) {
          if (T.childrenMask[p] & (mask_t)(ZLKM_MASK_ONE << c))
            promote |= (mask_t)(ZLKM_MASK_ONE << p);
        }
      }
      used |= promote;

      if (!used) {
        T.lastLogMs = now;
        continue;
      }

      // roots = used & ~union(children of used)
      mask_t childUnion = 0;
      tmp = used;
      while (tmp) {
        const uint16_t p = (uint16_t)ZLKM_CTZ(tmp);
        tmp &= (tmp - 1);
        childUnion |= (T.childrenMask[p] & used);
      }
      mask_t roots = used & ~childUnion;

      // Thread group header FIRST
      Log.infoln("[perf][T%u] (µs snapshot)", (unsigned)tid);

      bool printedAny = false;
      while (roots) {
        mask_t nextRoots = roots & (roots - 1);
        const uint16_t r = (uint16_t)ZLKM_CTZ(roots);
        const bool rootIsLast = (nextRoots == 0);
        printedAny |= print_node_rec_(T, r, /*depth=*/0, /*ancMore=*/0,
                                      rootIsLast, now, used,
                                      /*parent_dSum*/ (sum_t)0,
                                      /*parent_totalSum*/ (sum_t)0);
        roots = nextRoots;
      }

      (void)printedAny;  // spacing optional
      T.lastLogMs = now;
    }
  }

  // Public helper for PerfCounter
  static inline uint8_t current_tid() { return instance().current_tid_(); }

 private:
  Profiler() = default;

  struct ThreadCounter {
    uint32_t count{0};
    uint32_t epoch{0};
    sum_t sumUs{0};
#if ZLKM_PROFILE_ENABLE_MAX
    uint32_t maxUs[2]{0, 0};  // double-buffered max
#endif
    // snapshot baselines
    uint32_t lastCount{0};
    sum_t lastSum{0};
  };

  struct Thread {
    std::array<const char*, ZLKM_PROFILE_MAX_COUNTERS> names{};
    std::array<ThreadCounter, ZLKM_PROFILE_MAX_COUNTERS> perCounter{};
    std::array<uint32_t, ZLKM_PROFILE_MAX_COUNTERS> lastPrintMs{};
    std::array<mask_t, ZLKM_PROFILE_MAX_COUNTERS> childrenMask{};
    uint16_t stack[ZLKM_PROFILE_MAX_DEPTH]{};
    uint8_t depth{0};

    uint32_t lastLogMs{0};
    bool active{false};
    uint16_t nextId{0};
  };

  Config cfg_{};
  std::array<Thread, ZLKM_PROFILE_MAX_THREADS> threads_{};
  ThreadIndexFn threadIndexFn_ = nullptr;

  inline uint8_t current_tid_() const {
    uint8_t idx = 0;
    if (threadIndexFn_) idx = threadIndexFn_();
    return (idx < ZLKM_PROFILE_MAX_THREADS) ? idx : 0;
  }

  static inline void build_prefix_(char* out, size_t out_sz, uint8_t depth,
                                   mask_t ancMoreMask, bool isRoot,
                                   bool isLast) {
    char* p = out;
    char* end = out + (out_sz ? (out_sz - 1) : 0);
    auto append = [&](const char* s) {
      while (*s && p < end) *p++ = *s++;
    };

    for (uint8_t d = 0; d < depth; ++d) {
      append((ancMoreMask & (mask_t)(ZLKM_MASK_ONE << d)) ? T_VERT : T_SPACE);
    }
    if (!isRoot) append(isLast ? T_ELB : T_TEE);

    *p = '\0';
  }

  bool print_node_rec_(Thread& T, uint16_t id, uint8_t depth,
                       mask_t ancMoreMask, bool isLast, uint32_t nowMs,
                       mask_t usedMask, sum_t parent_dSum,
                       sum_t parent_totalSum) {
    ThreadCounter& tc = T.perCounter[id];

    // swap max buffer, bump epoch
#if ZLKM_PROFILE_ENABLE_MAX
    const uint32_t curEpoch = tc.epoch;
    const uint32_t oldIdx = (curEpoch & 1u);
    tc.epoch = curEpoch + 1u;

    const uint32_t maxUs = tc.maxUs[oldIdx];
    tc.maxUs[oldIdx] = 0;
#else
    tc.epoch++;  // still flip epoch for symmetry
    const uint32_t maxUs = 0;
#endif

    const uint32_t count = tc.count;
    const sum_t total = tc.sumUs;

    const uint32_t dCount = count - tc.lastCount;
    const sum_t dSum = total - tc.lastSum;

    bool printed = false;
    if (dCount) {
      const char* name = T.names[id] ? T.names[id] : "(unnamed)";
      const uint32_t avgUs = (uint32_t)(dSum / dCount);

      // Shares (integers)
      uint32_t winPct = 0;
      uint32_t totPct = 0;
      if (parent_dSum > 0)
        winPct = (uint32_t)(((uint64_t)dSum * 100ull) / parent_dSum);
      if (parent_totalSum > 0)
        totPct = (uint32_t)(((uint64_t)total * 100ull) / parent_totalSum);

      char prefix[ZLKM_PROFILE_PREFIX_MAX];
      build_prefix_(prefix, sizeof(prefix), depth, ancMoreMask,
                    /*isRoot=*/depth == 0, isLast);

      Log.infoln("%s%s avg=%uus (max=%u us, N=%lu)  [win=%u%%  total=%u%%]",
                 prefix, name, avgUs, maxUs, (unsigned long)dCount, winPct,
                 totPct);

      tc.lastCount = count;
      tc.lastSum = total;
      T.lastPrintMs[id] = nowMs;
      printed = true;
    }

    // active children only: pass our sums as their “parent”
    mask_t childMask = T.childrenMask[id] & usedMask;
    while (childMask) {
      mask_t nextMask = childMask & (childMask - 1);
      const uint16_t c = (uint16_t)ZLKM_CTZ(childMask);
      const bool childIsLast = (nextMask == 0);

      const mask_t childAncMore =
          ancMoreMask | (isLast ? (mask_t)0 : (mask_t)(ZLKM_MASK_ONE << depth));

      printed |= print_node_rec_(T, c, (uint8_t)(depth + 1), childAncMore,
                                 childIsLast, nowMs, usedMask,
                                 /*parent_dSum   =*/dSum,
                                 /*parent_totalSum=*/total);

      childMask = nextMask;
    }
    return printed;
  }

  static void fatal_(const char* tag, const char* name, uint8_t tid) {
    Log.fatalln("[perf] %s: capacity reached on T%u while adding '%s'",
                tag ? tag : "fatal", (unsigned)tid, name ? name : "(null)");
    noInterrupts();
    while (true) {
    }
  }
};

// ---------- RAII counter (per-thread id, fail-fast on overflow) ----------
class PerfCounter {
 public:
  static constexpr uint16_t uninit_id = 0xFFFFu;

  explicit PerfCounter(const char* name) : name_(name) {
    for (auto& s : slot_) s = uninit_id;
    for (auto& g : gate_) g = 0;
  }

  class Scope {
   public:
    explicit Scope(PerfCounter& owner) : owner_(owner) {
      tid_ = Profiler::current_tid();
      uint16_t& idref = owner_.slot_[tid_];
      if (idref == uninit_id) {
        idref =
            Profiler::instance().ensure_counter_for_thread(owner_.name_, tid_);
      }
      id_ = idref;
      Profiler::instance().on_scope_enter(id_, tid_);
      start_ = micros();
    }
    ~Scope() {
      Profiler::instance().on_scope_exit(id_, (uint32_t)(micros() - start_),
                                         tid_);
    }

   private:
    PerfCounter& owner_;
    uint32_t start_{0};
    uint16_t id_{uninit_id};
    uint8_t tid_{0};
  };

  // Sampled scope: measure 1 in (2^shift) calls; scales count/sum by weight
  class SampledScope {
   public:
    SampledScope(PerfCounter& owner, uint8_t shift) : owner_(owner) {
      tid_ = Profiler::current_tid();
      const uint32_t mask = (shift < 31) ? ((1u << shift) - 1u) : 0xFFFFFFFFu;
      uint32_t& gate = owner_.gate_[tid_];
      if (mask && (gate++ & mask)) {
        active_ = false;
        return;
      }
      active_ = true;
      weight_ = (mask + 1u);
      uint16_t& idref = owner_.slot_[tid_];
      if (idref == uninit_id) {
        idref =
            Profiler::instance().ensure_counter_for_thread(owner_.name_, tid_);
      }
      id_ = idref;
      Profiler::instance().on_scope_enter(id_, tid_);
      start_ = micros();
    }
    ~SampledScope() {
      if (!active_) return;
      const uint32_t dt = (uint32_t)(micros() - start_);
      Profiler::instance().on_scope_exit_weighted(id_, dt, weight_, tid_);
    }

   private:
    PerfCounter& owner_;
    uint8_t tid_{0};
    uint16_t id_{uninit_id};
    uint32_t start_{0};
    uint32_t weight_{1};
    bool active_{false};
  };

  inline Scope scope() { return Scope(*this); }
  inline SampledScope sampled_scope(uint8_t sample_shift) {
    return SampledScope(*this, sample_shift);
  }

 private:
  const char* name_;
  std::array<uint16_t, ZLKM_PROFILE_MAX_THREADS> slot_{};
  std::array<uint32_t, ZLKM_PROFILE_MAX_THREADS> gate_{};
};

}  // namespace zlkm::util

// ==== Macros ====
#define ZLKM_PROFILE_INIT_DEFAULT()                 \
  do {                                              \
    ::zlkm::util::Profiler::Config _cfg;            \
    ::zlkm::util::Profiler::instance().setup(_cfg); \
  } while (0)

#define ZLKM_PROFILE_INIT(cfg) (::zlkm::util::Profiler::instance().setup((cfg)))

#define ZLKM_PROFILE_SET_THREAD_INDEX(fn) \
  (::zlkm::util::Profiler::instance().set_thread_index_fn((fn)))

#define ZLKM_PROFILE_SET_EMIT_THREAD(tid) \
  (::zlkm::util::Profiler::instance().set_emit_thread((tid)))

#define ZLKM_PROFILE_TICK() (::zlkm::util::Profiler::instance().tick_and_log())

#ifndef ZLKM_PERF_SCOPE
#define ZLKM_PERF_SCOPE(NAME)                                 \
  static ::zlkm::util::PerfCounter _zlkm_pc_##__LINE__{NAME}; \
  auto _zlkm_scope_##__LINE__ = _zlkm_pc_##__LINE__.scope()
#endif

#ifndef ZLKM_PERF_SCOPE_SAMPLED
#define ZLKM_PERF_SCOPE_SAMPLED(NAME, SHIFT)                  \
  static ::zlkm::util::PerfCounter _zlkm_pc_##__LINE__{NAME}; \
  auto _zlkm_scope_s_##__LINE__ = _zlkm_pc_##__LINE__.sampled_scope(SHIFT)
#endif

#else  // PROFILE off -> no-ops

#define ZLKM_PROFILE_INIT_DEFAULT() ((void)0)
#define ZLKM_PROFILE_INIT(cfg) ((void)0)
#define ZLKM_PROFILE_SET_THREAD_INDEX(fn) ((void)0)
#define ZLKM_PROFILE_SET_EMIT_THREAD(tid) ((void)0)
#define ZLKM_PROFILE_TICK() ((void)0)
#ifndef ZLKM_PERF_SCOPE
#define ZLKM_PERF_SCOPE(NAME) ((void)0)
#endif
#ifndef ZLKM_PERF_SCOPE_SAMPLED
#define ZLKM_PERF_SCOPE_SAMPLED(NAME, SHIFT) ((void)0)
#endif

#endif  // PROFILE
