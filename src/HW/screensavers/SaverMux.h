#pragma once
#include <stdint.h>

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "platform/platform.h"

namespace zlkm::hw::ssaver {

enum class MuxMode : uint8_t { Single, Cycle, Random };

template <typename G, template <class> class... SaverTpls>
class SaverMux {
  static_assert(sizeof...(SaverTpls) > 0,
                "Provide at least one saver template.");

  // Helper aliases
  template <template <class> class Tpl>
  using SaverT = Tpl<G>;
  using ActiveVariant = std::variant<SaverT<SaverTpls>...>;
  using CfgTuple = std::tuple<typename SaverT<SaverTpls>::Cfg...>;

 public:
  struct Cfg {
    MuxMode mode = MuxMode::Cycle;
    uint32_t cycleMs = 10'000;  // used by Cycle/Random
    uint8_t startIndex = 0;     // initial saver (and fixed for Single)
  };

  // Defaults: default mux cfg + default saver cfgs
  explicit SaverMux(const Cfg& muxCfg = {}, CfgTuple saverCfgs = CfgTuple{})
      : muxCfg_(muxCfg), saverCfgs_(std::move(saverCfgs)) {
    const uint32_t now = millis();
    lastSwitchMs_ = now;
    currentIndex_ = clampIndex(muxCfg_.startIndex);
    emplaceByIndex(currentIndex_);
  }

  // Convenience: pass saver cfgs as separate args
  template <typename... Cfgs, typename = std::enable_if_t<
                                  (sizeof...(Cfgs) == sizeof...(SaverTpls))>>
  explicit SaverMux(const Cfg& muxCfg, Cfgs&&... cfgs)
      : SaverMux(muxCfg, CfgTuple{std::forward<Cfgs>(cfgs)...}) {}

  // Step with external idle boolean. Returns true if a saver drew.
  inline bool step(uint32_t now, bool isIdle, G& g) {
    return stepImpl(now, isIdle, g);
  }

  // Force which saver index to use on next switch (or immediately on idle
  // enter).
  inline void setIndex(uint8_t idx) { currentIndex_ = clampIndex(idx); }

  static constexpr uint8_t count() { return sizeof...(SaverTpls); }

 private:
  // --- core logic ---
  inline bool stepImpl(uint32_t now, bool idle, G& g) {
    if (idle) {
      if (!idleActive_) {
        idleActive_ = true;
        lastSwitchMs_ = now;
        currentIndex_ = clampIndex(muxCfg_.startIndex);
        if (muxCfg_.mode == MuxMode::Random && count() > 1) {
          currentIndex_ = randomIndexDifferentFrom(currentIndex_);
        }
        emplaceByIndex(currentIndex_);
      }

      std::visit([&](auto& s) { s.step(now, g); }, active_);

      if ((muxCfg_.mode == MuxMode::Cycle || muxCfg_.mode == MuxMode::Random) &&
          muxCfg_.cycleMs > 0 && (now - lastSwitchMs_) >= muxCfg_.cycleMs) {
        lastSwitchMs_ = now;
        uint8_t next = currentIndex_;
        if (muxCfg_.mode == MuxMode::Cycle) {
          next = (uint8_t)((currentIndex_ + 1) % count());
        } else {  // Random
          if (count() > 1) next = randomIndexDifferentFrom(currentIndex_);
        }
        if (next != currentIndex_) {
          currentIndex_ = next;
          emplaceByIndex(currentIndex_);
        }
      }
      return true;
    } else {
      idleActive_ = false;
      return false;
    }
  }

  // Construct `active_` as saver at runtime index using its stored Cfg
  inline void emplaceByIndex(uint8_t idx) {
    emplaceByIndexImpl(idx, std::make_index_sequence<sizeof...(SaverTpls)>{});
  }

  template <size_t... Is>
  inline void emplaceByIndexImpl(uint8_t idx, std::index_sequence<Is...>) {
    (void)((idx == Is ? (emplaceNth<Is>(), 1) : 0) + ...);
  }

  template <size_t N>
  inline void emplaceNth() {
    using SaverN = std::tuple_element_t<N, std::tuple<SaverT<SaverTpls>...>>;
    auto& cfgN = std::get<N>(saverCfgs_);
    active_.template emplace<SaverN>(cfgN);
  }

  static constexpr uint8_t clampIndex(uint8_t i) {
    return (count() == 0) ? 0 : (uint8_t)(i % count());
  }

  inline uint8_t randomIndexDifferentFrom(uint8_t prev) {
    if (count() <= 1) return 0;
    uint8_t idx;
    do {
      idx = (uint8_t)random(0, (long)count());
    } while (idx == prev);
    return idx;
  }

  // --- minimal state (no duplicated saver cfg fields) ---
  Cfg muxCfg_{};
  CfgTuple saverCfgs_{};
  ActiveVariant active_;

  uint32_t lastSwitchMs_{0};
  uint8_t currentIndex_{0};
  bool idleActive_{false};
};

}  // namespace zlkm::hw::ssaver
