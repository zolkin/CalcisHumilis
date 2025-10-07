#pragma once

#include <array>
#include <tuple>

#include "audio/DJFilter.h"
#include "dsp/Util.h"
#include "math/Util.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"  // For RotaryInputSpec, ParameterPage sizes, and Calcis types

namespace zlkm::ui {

// Forward-declared InputSampler interface we use
template <typename PinExpanderT, int N>
class InputSampler;

// Page class for processing: holds RotaryInputSpec array
struct ProcPage {
  std::array<zlkm::ch::RotaryInputSpec, zlkm::ch::ParameterPage::ROTARY_COUNT>
      spec{};
  bool enabled = true;
};

// Utility mapping helpers (copied from existing UI_impl.hh)
static inline float stickEnds(float f) {
  static constexpr float stickyEnds = 0.05f;
  static constexpr float reducedRangeScale = 1.f / (1.f - 2.f * stickyEnds);
  if (f < stickyEnds) return 0.f;
  if (f > 1.f - stickyEnds) return 1.f;
  return f * reducedRangeScale;
}

static inline float mapLin_(int raw, int rawMax, float outMin, float outMax,
                            bool invert = false) {
  raw = zlkm::math::clamp(raw, 0, rawMax);
  float x = static_cast<float>(raw) / static_cast<float>(rawMax);
  if (invert) x = 1.0f - x;
  x = stickEnds(x);
  return outMin + x * (outMax - outMin);
}

static inline float mapExp_(int raw, int rawMax, float outMin, float outMax,
                            bool invert = false) {
  if (rawMax <= 0) return outMin;
  float x = float(zlkm::math::clamp(raw, 0, rawMax)) / rawMax;
  if (outMin < 0 && outMax <= 0) {
    float dB = outMin + (invert ? 1 - x : x) * (outMax - outMin);
    return powf(10.f, dB * 0.05f);
  }
  float y = powf(x, 1.6f);
  if (invert) y = 1.0f - y;
  y = stickEnds(y);
  return outMin + (outMax - outMin) * y;
}

static inline float potToRate(int raw, int rawMax, float msMin, float msMax,
                              float SR) {
  raw = zlkm::math::clamp(raw, 0, rawMax);
  const float pot = float(raw) / float(rawMax);
  float ms = msMin + pot * (msMax - msMin);
  return zlkm::dsp::msToRate(ms, SR);
}

// Controller consumes encoder deltas and updates Calcis::Cfg
template <typename PinExpanderT, int N, class... Tabs>
class Controller {
 public:
  using TR = zlkm::ch::CalcisTR;
  using Calcis = zlkm::ch::Calcis;
  using Cfg = typename Calcis::Cfg;
  using Feedback = typename Calcis::Feedback;
  using Selection = TabControl<Tabs...>;

  static constexpr int kAdcMaxCode = 4095;

  struct StatePage {
    std::array<int, zlkm::ch::ParameterPage::ROTARY_COUNT> rawPos{0, 0, 0, 0};
  };
  struct StateTab {
    std::array<StatePage, zlkm::ch::ParameterTab::MAX_PAGE> pages{};
  };

  Controller(Cfg* cfg, Feedback* fb, Selection* sel,
             InputSampler<PinExpanderT, N>* sampler)
      : cfg_(cfg), fb_(fb), selection_(sel), sampler_(sampler) {}

  // Helper: visit tuple element by runtime index
  template <typename Tuple, typename F, size_t... Is>
  static inline bool visitTupleAt_(Tuple& t, size_t i, F&& f,
                                   std::index_sequence<Is...>) {
    return ((i == Is ? (f(std::get<Is>(t)), true) : false) || ...);
  }
  template <typename Tuple, typename F>
  static inline bool visitTupleAt(Tuple& t, size_t i, F&& f) {
    return visitTupleAt_(
        t, i, std::forward<F>(f),
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
  }

  // Called from the audio loop. Consume encoder deltas and convert to cfg.
  void process() {
    const uint8_t tabIdx = selection_->currentTab.load();
    visitTupleAt(selection_->tabs, tabIdx, [&](auto& tab) {
      const uint8_t pageIdx = tab.currentPage.load();
      visitTupleAt(tab.pages, pageIdx, [&](auto& page) {
        for (int i = 0; i < (int)zlkm::ch::ParameterPage::ROTARY_COUNT; ++i) {
          int32_t dCounts = sampler_->consumeDeltaCounts(i);
          if (!dCounts) continue;
          const auto& spec = page.spec[i];

          const int span = spec.encSpanCounts > 0
                               ? spec.encSpanCounts
                               : 2 * 24 * 4;  // default two turns
          const float scale =
              span > 0 ? (float(kAdcMaxCode) / float(span)) : 0.0f;
          int deltaRaw =
              int(lroundf(float(spec.encInvert ? -dCounts : dCounts) * scale));
          int& raw = tabs_[tabIdx].pages[pageIdx].rawPos[i];
          raw = zlkm::math::clamp(raw + deltaRaw, 0, kAdcMaxCode);

          // Apply mapping
          if (!spec.cfgValue) continue;
          switch (spec.response) {
            case zlkm::ch::RotaryInputSpec::RsLin:
              spec.setCfgValue(
                  mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              break;
            case zlkm::ch::RotaryInputSpec::RsExp:
              spec.setCfgValue(
                  mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              break;
            case zlkm::ch::RotaryInputSpec::RsRate:
              spec.setCfgValue(potToRate(raw, kAdcMaxCode, spec.outMin,
                                         spec.outMax, TR::SR));
              break;
            case zlkm::ch::RotaryInputSpec::RsGCut:
              filter_.setCutoff01(
                  mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              spec.setCfgValue(filter_.cfg);
              break;
            case zlkm::ch::RotaryInputSpec::RsKDamp:
              filter_.setRes01(
                  mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              spec.setCfgValue(filter_.cfg);
              break;
            case zlkm::ch::RotaryInputSpec::RsMorph:
              filter_.setMorph01(
                  mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              spec.setCfgValue(filter_.cfg);
              break;
            case zlkm::ch::RotaryInputSpec::RsDrive:
              filter_.setDrive01(
                  mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              spec.setCfgValue(filter_.cfg);
              break;
            case zlkm::ch::RotaryInputSpec::RsInt: {
              int v = int(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
              spec.setCfgValue(v);
            } break;
            case zlkm::ch::RotaryInputSpec::RsBool: {
              bool v = bool(int(mapLin_(raw, kAdcMaxCode, 0.f, 1.f)));
              spec.setCfgValue(v);
            } break;
            default:
              break;
          }
        }
      });
    });
  }

  // Seed raw positions from existing cfg values (optional)
  void seedFromCfg() { seedTabs_(selection_->tabs); }

 private:
  template <size_t TI, typename TabT>
  void seedTab_(TabT& tab) {
    using PagesTuple = typename TabT::PagesTuple;
    seedTabPages_<TI>(
        tab, std::make_index_sequence<std::tuple_size_v<PagesTuple>>{});
  }
  template <size_t TI, typename TabT, size_t... PIs>
  void seedTabPages_(TabT& tab, std::index_sequence<PIs...>) {
    (seedPage_<TI, PIs>(std::get<PIs>(tab.pages)), ...);
  }
  template <size_t TI, size_t PIdx, typename PageT>
  void seedPage_(PageT& page) {
    for (int i = 0; i < (int)zlkm::ch::ParameterPage::ROTARY_COUNT; ++i) {
      if (page.spec[i].cfgValue)
        tabs_[TI].pages[PIdx].rawPos[i] = kAdcMaxCode / 2;
    }
  }
  template <typename TabsTuple, size_t... Is>
  void seedTabs_(TabsTuple& t, std::index_sequence<Is...>) {
    (seedTab_<Is>(std::get<Is>(t)), ...);
  }
  template <typename TabsTuple>
  void seedTabs_(TabsTuple& t) {
    seedTabs_(
        t,
        std::make_index_sequence<std::tuple_size_v<std::decay_t<TabsTuple>>>{});
  }

  // Access current raw value (for UI display)
  int rawAt(uint8_t tab, uint8_t page, int ctl) const {
    return tabs_[tab].pages[page].rawPos[ctl];
  }

 private:
  Cfg* cfg_;
  Feedback* fb_;
  Selection* selection_;
  InputSampler<PinExpanderT, N>* sampler_;
  std::array<StateTab, sizeof...(Tabs)> tabs_{};
  zlkm::audio::SafeFilterParams<TR::SR> filter_;
};

}  // namespace zlkm::ui
