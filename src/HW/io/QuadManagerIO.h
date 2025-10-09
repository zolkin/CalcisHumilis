#pragma once
#include <Arduino.h>

#include <array>
#include <bitset>

#include "GpioPins.h"  // GpioPins<M>   has readPins<K>()
#include "McpPins.h"   // Mcp23017Pins has readPins<K>()
#include "Types.h"

namespace zlkm::hw::io {

template <typename PinGroupT, int N>
class QuadManagerIO {
 public:
  struct Cfg {
    std::array<uint8_t, N>
        pinsA{};  // Back-end native ids:
                  //  - Mcp23017Pins: MCP pins [0..15]
                  //  - GpioPins<M> : INDEXES into that GpioPins group [0..M-1]
    std::array<uint8_t, N> pinsB{};  // same domain as pinsA
    bool usePullUp = true;           // enable pull-ups on A/B
  };

  QuadManagerIO(const QuadManagerIO&) = delete;
  QuadManagerIO& operator=(const QuadManagerIO&) = delete;
  QuadManagerIO(QuadManagerIO&&) = default;
  QuadManagerIO& operator=(QuadManagerIO&&) = default;

  // Full mapping ctor
  QuadManagerIO(PinGroupT& dev, const Cfg& cfg) : dev_(dev), cfg_(cfg) {
    const PinMode pm = cfg_.usePullUp ? PinMode::InputPullUp : PinMode::Input;
    for (int i = 0; i < N; ++i) {
      dev_.setPinMode(cfg_.pinsA[i], pm);
      dev_.setPinMode(cfg_.pinsB[i], pm);
      readOrder_[i] = cfg_.pinsA[i];
      readOrder_[N + i] = cfg_.pinsB[i];
    }

    // Prime previous states from hardware
    auto bits = dev_.template readPins<2 * N>(readOrder_);  // 1=HIGH
    for (int i = 0; i < N; ++i) {
      const uint8_t a = bits.test(i) ? 1 : 0;
      const uint8_t b = bits.test(N + i) ? 1 : 0;
      enc_[i].prev = (uint8_t)((a << 1) | b);  // A as MSb, B as LSb
      enc_[i].delta = 0;
    }
  }

  // Convenience: B = A + 1 layout
  QuadManagerIO(PinGroupT& dev, const std::array<uint8_t, N>& pinsA,
                bool usePullUp = true)
      : QuadManagerIO(dev, makeCfgAPlus1_(pinsA, usePullUp)) {}

  // Poll once; apply deltas
  inline void update() {
    ZLKM_PERF_SCOPE("QuadManagerIO::update");
    auto bits =
        dev_.template readPins<2 * N>(readOrder_);  // A-block then B-block
    for (int i = 0; i < N; ++i) {
      Enc& e = enc_[i];
      const uint8_t a = bits.test(i) ? 1 : 0;
      const uint8_t b = bits.test(N + i) ? 1 : 0;
      const uint8_t curr = (uint8_t)((a << 1) | b);

      if (e.prev <= 3) {
        const uint8_t idx = (uint8_t)((e.prev << 2) | curr);
        const int8_t d = kDecodeLUT[idx];
        if (d) e.delta += e.invert ? -d : d;
      }
      e.prev = curr;
    }
  }

  // Invert encoder direction
  inline void invert(int idx, bool inv) { enc_[idx].invert = inv; }

  // Return delta since last consume for this encoder and update baseline
  inline int32_t consumeDeltaCounts(int idx) {
    int32_t d = enc_[idx].delta;
    enc_[idx].delta = 0;
    return d;
  }

  // Reset baselines to current counts (e.g., when changing page/tab)
  inline void resetBaselines() {
    auto bits = dev_.template readPins<2 * N>(readOrder_);
    for (int i = 0; i < N; ++i) {
      const uint8_t a = bits.test(i) ? 1 : 0;
      const uint8_t b = bits.test(N + i) ? 1 : 0;
      enc_[i].prev = (uint8_t)((a << 1) | b);
      enc_[i].delta = 0;
    }
  }

 private:
  struct Enc {
    uint8_t prev = 0xFF;  // previous 2-bit state (A<<1 | B)
    int32_t delta = 0;    // unread delta since last consume
    bool invert = false;
  };

  static constexpr int8_t kDecodeLUT[16] = {
      /*00->00*/ 0,  /*00->01*/ +1, /*00->10*/ -1, /*00->11*/ 0,
      /*01->00*/ -1, /*01->01*/ 0,  /*01->10*/ 0,  /*01->11*/ +1,
      /*10->00*/ +1, /*10->01*/ 0,  /*10->10*/ 0,  /*10->11*/ -1,
      /*11->00*/ 0,  /*11->01*/ -1, /*11->10*/ +1, /*11->11*/ 0};

  static inline Cfg makeCfgAPlus1_(const std::array<uint8_t, N>& pinsA,
                                   bool usePullUp) {
    Cfg c{};
    c.pinsA = pinsA;
    for (int i = 0; i < N; ++i) c.pinsB[i] = (uint8_t)(pinsA[i] + 1);
    c.usePullUp = usePullUp;
    return c;
  }

  PinGroupT& dev_;
  Cfg cfg_{};
  std::array<uint8_t, 2 * N> readOrder_{};  // [A0..A{N-1}, B0..B{N-1}]
  std::array<Enc, N> enc_{};
};

}  // namespace zlkm::hw::io