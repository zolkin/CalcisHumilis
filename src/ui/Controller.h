#pragma once

#include <array>
#include <memory>

#include "CalcisHumilis.h"
#include "dsp/Util.h"
#include "hw/io/ButtonManager.h"
#include "math/Util.h"
#include "platform/boards/Current.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"
#include "util/IdleTimer.h"
#include "util/Profiler.h"

namespace zlkm::ui {

// Sampler is any type that provides consumeDeltaCounts(int)
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
  using PinDevice = typename SamplerT::DeviceT;
  using TabButtons = zlkm::hw::io::ButtonManager<4, PinDevice>;
  using TriggerBtnMgr = zlkm::hw::io::ButtonManager<1, PinDevice>;

  static constexpr int kAdcMaxCode = 4095;

  Controller(Cfg& cfg, PinDevice& dev,
             const typename TabButtons::Cfg& buttonCfg, Feedback& fb,
             SamplerT& sampler, Selection& selection,
             const typename TriggerBtnMgr::Cfg& triggerBtnCfg)
      : cfg_(cfg),
        fb_(fb),
        sampler_(sampler),
        selection_(selection),
        pinDev_(dev),
        tabBtns_(pinDev_, buttonCfg),
        triggerBtn_(pinDev_, triggerBtnCfg) {}

  // Optional accessor to the externally-owned selection
  Selection& selection() { return selection_; }
  const Selection& selection() const { return selection_; }

  bool hasActivity() const { return activity_; }

  // Poll the trigger button; returns true on rising edge
  bool consumeTriggerRising() {
    auto rep = triggerBtn_.tick();
    return rep.rising.test(0);
  }

  void update(zlkm::util::IdleTimer& idle) {
    ZLKM_PERF_SCOPE("Controller::update");
    activity_ = false;
    auto rep = tabBtns_.tick();
    for (int i = 0; i < ROTARY_COUNT; ++i) {
      if (!rep.rising.test(i)) continue;
      const uint8_t cur = selection_.currentTabIndex();
      if ((uint8_t)i == cur) {
        selection_.nextPageInCurrentTab();
        activity_ = true;
        idle.noteActivity();
      } else {
        if (i < (int)Selection::count()) selection_.setCurrentTab((uint8_t)i);
        activity_ = true;
        idle.noteActivity();
      }
    }

    if (consumeTriggerRising()) {
      idle.noteActivity();
      ++(cfg_.trigCounter);
    }

    // Process encoders for the current page only
    PTab& t = selection_.tabAt(selection_.currentTabIndex());
    PPage& page = t.pages[t.currentPage];
    for (int i = 0; i < (int)PPage::ROTARY_COUNT; ++i) {
      int32_t dCounts = sampler_.consumeDeltaCounts(i);
      if (!dCounts) continue;
      activity_ = true;
      idle.noteActivity();
      int16_t& raw = page.rawPos[i];
      // Use default span as before
      constexpr int defaultSpan = 2 * 24 * 4;
      const float scale = float(kAdcMaxCode) / float(defaultSpan);
      int deltaRaw = int(lroundf(float(dCounts) * scale));
      raw = zlkm::math::clamp(raw + deltaRaw, 0, kAdcMaxCode);
      page.mappers[i].mapAndSet(raw);
    }
  }

  // Seed raw positions from existing cfg values (optional)
  void seedFromCfg() {
    for (auto& t : selection_.tabs) {
      for (int pi = 0; pi < t.pageCount; ++pi) {
        auto& page = t.pages[pi];
        for (int i = 0; i < (int)PPage::ROTARY_COUNT; ++i) {
          page.rawPos[i] = page.mappers[i].reverseMap();
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

  // Expander-backed IO
  PinDevice& pinDev_;
  TabButtons tabBtns_;
  TriggerBtnMgr triggerBtn_;
  bool activity_ = false;
};

}  // namespace zlkm::ui
