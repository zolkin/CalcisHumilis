#pragma once

#include <array>
#include <memory>

#include "CalcisHumilis.h"
#include "audio/DJFilter.h"
#include "dsp/Util.h"
#include "hw/io/ButtonManager.h"
#include "hw/io/GpioPins.h"
#include "hw/io/McpPins.h"
#include "math/Util.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"  // For RotaryInputSpec, ParameterPage sizes, and Calcis types
#include "util/Profiler.h"

namespace zlkm::ui {

// Sampler is any type that provides consumeDeltaCounts(int)

// No ProcPage: use zlkm::ch::ParameterPage directly for specs and state

// Utility mapping helpers (copied from existing UI_impl.hh)
static inline float stickEnds(float f) {
  static constexpr float stickyEnds = 0.05f;
  static constexpr float reducedRangeScale = 1.f / (1.f - 2.f * stickyEnds);
  if (f < stickyEnds) return 0.f;
  if (f > 1.f - stickyEnds) return 1.f;
  return f * reducedRangeScale;
}

static inline float mapLin_(int raw, int rawMax, float outMin, float outMax) {
  raw = zlkm::math::clamp(raw, 0, rawMax);
  float x = static_cast<float>(raw) / static_cast<float>(rawMax);
  x = stickEnds(x);
  return outMin + x * (outMax - outMin);
}

static inline float mapExp_(int raw, int rawMax, float outMin, float outMax) {
  if (rawMax <= 0) return outMin;
  float x = float(zlkm::math::clamp(raw, 0, rawMax)) / rawMax;
  if (outMin < 0 && outMax <= 0) {
    float dB = outMin + x * (outMax - outMin);
    return powf(10.f, dB * 0.05f);
  }
  float y = powf(x, 1.6f);
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
template <typename SamplerT, size_t N, size_t PAGE_COUNT, size_t ROTARY_COUNT>
class Controller {
 public:
  using TR = zlkm::ch::CalcisTR;
  using Calcis = zlkm::ch::Calcis;
  using Cfg = typename Calcis::Cfg;
  using Feedback = typename Calcis::Feedback;
  using Selection = ParameterTabControlT<N, PAGE_COUNT, ROTARY_COUNT>;
  using PPage = ::zlkm::ui::ParameterPageT<ROTARY_COUNT>;
  using PTab = ::zlkm::ui::ParameterTabT<PAGE_COUNT, ROTARY_COUNT>;
  using PinExpander = zlkm::hw::io::Mcp23017Pins;
  using TabButtons = zlkm::hw::io::ButtonManager<4, PinExpander>;
  using TriggerPort = zlkm::hw::io::GpioPins<1>;
  using TriggerBtnMgr = zlkm::hw::io::ButtonManager<1, TriggerPort>;

  static constexpr int kAdcMaxCode = 4095;

  Controller(Cfg& cfg, PinExpander& exp, const TabButtons::Cfg& buttonCfg,
             Feedback& fb, SamplerT& sampler, Selection& selection,
             uint8_t triggerPin,
             const typename TriggerBtnMgr::Cfg& triggerBtnCfg)
      : cfg_(cfg),
        fb_(fb),
        sampler_(sampler),
        selection_(selection),
        pinExp_(exp),
        tabBtns_(pinExp_, buttonCfg),
        triggerPort_({triggerPin}, zlkm::hw::io::PinMode::Input),
        triggerBtn_(triggerPort_, triggerBtnCfg) {}

  // Optional accessor to the externally-owned selection
  Selection& selection() { return selection_; }
  const Selection& selection() const { return selection_; }

  bool hasActivity() const { return activity_; }

  // Poll the trigger button; returns true on rising edge
  bool consumeTriggerRising() {
    auto rep = triggerBtn_.tick();
    return rep.rising.test(0);
  }

  void update() {
    ZLKM_PERF_SCOPE("Controller::update");
    activity_ = false;
    auto rep = tabBtns_.tick();
    for (int i = 0; i < ROTARY_COUNT; ++i) {
      if (!rep.rising.test(i)) continue;
      const uint8_t cur = selection_.currentTabIndex();
      if ((uint8_t)i == cur) {
        selection_.nextPageInCurrentTab();
        activity_ = true;
      } else {
        if (i < (int)Selection::count()) selection_.setCurrentTab((uint8_t)i);
        activity_ = true;
      }
    }

    if (consumeTriggerRising()) {
      ++(cfg_.trigCounter);
    }

    // Process encoders for the current page only
    PTab& t = selection_.tabAt(selection_.currentTabIndex());
    PPage& page = t.pages[t.currentPage];
    for (int i = 0; i < (int)PPage::ROTARY_COUNT; ++i) {
      int32_t dCounts = sampler_.consumeDeltaCounts(i);
      if (!dCounts) continue;
      activity_ = true;
      const auto& spec = page.rotary[i];

      const int span = spec.encSpanCounts > 0 ? spec.encSpanCounts : 2 * 24 * 4;
      const float scale = span > 0 ? (float(kAdcMaxCode) / float(span)) : 0.0f;
      int deltaRaw =
          int(lroundf(float(spec.encInvert ? -dCounts : dCounts) * scale));
      int& raw = page.rawPos[i];
      raw = zlkm::math::clamp(raw + deltaRaw, 0, kAdcMaxCode);

      if (!spec.cfgValue) continue;
      switch (spec.response) {
        case zlkm::ch::RotaryInputSpec::RsLin:
          spec.setCfgValue(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
          break;
        case zlkm::ch::RotaryInputSpec::RsExp:
          spec.setCfgValue(mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
          break;
        case zlkm::ch::RotaryInputSpec::RsRate:
          spec.setCfgValue(
              potToRate(raw, kAdcMaxCode, spec.outMin, spec.outMax, TR::SR));
          break;
        case zlkm::ch::RotaryInputSpec::RsGCut:
          filter_.setCutoff01(
              mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
          spec.setCfgValue(filter_.cfg);
          break;
        case zlkm::ch::RotaryInputSpec::RsKDamp:
          filter_.setRes01(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
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
  }

  // Seed raw positions from existing cfg values (optional)
  void seedFromCfg() {
    for (auto& t : selection_.tabs) {
      for (int pi = 0; pi < t.pageCount; ++pi) {
        auto& page = t.pages[pi];
        for (int i = 0; i < (int)PPage::ROTARY_COUNT; ++i) {
          if (page.rotary[i].cfgValue) {
            page.rawPos[i] = kAdcMaxCode / 2;
          }
        }
      }
    }
    // selection_ already initialized at defaults
  }

 private:
  // Access current raw value (for UI display)
  int rawAt(uint8_t tabIdx, uint8_t pageIdx, int ctl) const {
    if (tabIdx >= Selection::count()) return 0;
    const auto& t = selection_.tabs[tabIdx];
    if (pageIdx >= t.pageCount) return 0;
    return t.pages[pageIdx].rawPos[ctl];
  }

 private:
  Cfg& cfg_;
  Feedback& fb_;
  SamplerT& sampler_;
  Selection& selection_;
  // All per-page state lives in Selection's ParameterPage now
  zlkm::audio::SafeFilterParams<TR::SR> filter_;

  // Expander-backed IO
  PinExpander& pinExp_;
  TabButtons tabBtns_;
  // GPIO-backed trigger button
  TriggerPort triggerPort_;
  TriggerBtnMgr triggerBtn_;
  bool activity_ = false;
};

}  // namespace zlkm::ui
