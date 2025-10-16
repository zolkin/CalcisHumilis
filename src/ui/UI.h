#pragma once

// Drop-in UI facade built on the refactored ui components (InputSampler,
// Controller, View). It preserves the original UI class name and public API
// to minimize changes elsewhere in the codebase.

#include <JLED.h>

#include <array>
#include <memory>

#include "CalcisHumilis.h"
#include "audio/AudioTraits.h"
#include "dsp/Util.h"
#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"
#include "hw/io/QuadManagerIO.h"
#include "platform/boards/Current.h"
#include "platform/platform.h"
#include "ui/Controller.h"
#include "ui/InputMapper.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"
#include "ui/View.h"
#include "util/IdleTimer.h"
#include "util/Profiler.h"

namespace zlkm::ch {

class UI {
 public:
  using CH = Calcis;
  static constexpr float cyc(float p) { return Calcis::cycles(p); }
  using CalcisTR = zlkm::ch::CalcisTR;
  using PinDefs = typename ::zlkm::platform::boards::current::PinDefs;
  using PinId = typename ::zlkm::platform::boards::current::PinId;

  static constexpr size_t kMaxPagesPerTab = 4;  // define sizes explicitly here
  static constexpr size_t kRotaryCount = 4;

  using Selection =
      ::zlkm::ui::ParameterTabControlT<4, kMaxPagesPerTab, kRotaryCount>;

  using PinSource = typename ::zlkm::platform::boards::current::PinSource;
  using Sampler = hw::io::QuadManagerIO<PinSource, kRotaryCount>;
  using SamplerCfg = typename Sampler::Cfg;
  using ControllerT = ::zlkm::ui::Controller<Sampler, Selection::count(),
                                             kMaxPagesPerTab, kRotaryCount>;
  using ViewT = ::zlkm::ui::View<Selection>;

  using TabButtons = hw::io::ButtonManager<4, PinSource>;
  using Encoders = hw::io::QuadManagerIO<PinSource, kRotaryCount>;
  using TrigBtnCfg = zlkm::hw::io::ButtonManager<1, PinSource>::Cfg;
  using TrigPinsArray =
      typename zlkm::hw::io::ButtonManager<1, PinSource>::PinArrayT;

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };

    CH::EnvCfg& env(int idx) { return pCfg->envs[idx]; }

    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_)
        : pCfg(pCfg_), tabBtns{.pins = PinDefs::TAB_BUTTONS} {}

    static constexpr int kNumTabs = 4;

    TabButtons::Cfg tabBtns;

    std::array<uint8_t, kNumTabs> tabPageCount{3, 1, 1, 1};

    Calcis::Cfg* pCfg;

    float snapMultiplier = 0.0f;
    float activityThresh = 32.f;
    float encClkDiv = 50.0f;
    uint32_t screenIdleMs = 10000;
    uint16_t pollMs = 5;
    PinId trigPin = PinDefs::TRIG_IN;  // kept for potential future use
  };

  using ViewCfg = typename ViewT::Cfg;

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
      : ucfg_(cfg),
        fb_(fb),
        idleTimer_(ucfg_.screenIdleMs),
        pinSrc_(::zlkm::platform::boards::pico2::pinSource()),
        sampler_(
            pinSrc_,
            SamplerCfg{
                .group = PinDefs::GROUP_EXPANDER,
                .pinsA =
                    [] {
                      std::array<::zlkm::hw::io::PinId, kRotaryCount> ids{};
                      for (size_t i = 0; i < kRotaryCount; ++i)
                        ids[i] = PinDefs::ENCODER_A[i].pin();
                      return ids;
                    }(),
                .pinsB =
                    [] {
                      std::array<::zlkm::hw::io::PinId, kRotaryCount> ids{};
                      for (size_t i = 0; i < kRotaryCount; ++i)
                        ids[i] = PinDefs::ENCODER_B[i].pin();
                      return ids;
                    }(),
                .usePullUp = true}),
        selection_(),
        controller_(*ucfg_.pCfg, pinSrc_, ucfg_.tabBtns, *fb_, sampler_,
                    selection_,
                    TrigBtnCfg{.pins = TrigPinsArray{PinDefs::TRIG_IN},
                               .activeLow = true,
                               .usePullUp = true,
                               .debounceTicks = 5}),
        view_(selection_, pinSrc_,
              ViewCfg{.ledPins_ = PinDefs::LEDS, .fps = 60, .pCfg = ucfg_.pCfg},
              fb_),
        filterParams_(&ucfg_.pCfg->filter) {
    initSpecs();
    controller_.seedFromCfg();
    // Move expander-backed buttons/LEDs to Controller; keep expander here
  }

  // Legacy UI API: update does UI and also ticks sampler
  void update() {
    ZLKM_PERF_SCOPE("UI update");
    // Trigger button handled in controller
    sampler_.update();
    controller_.update(idleTimer_);
    view_.update(idleTimer_);
  }

 private:
  static constexpr float cycles(float p) { return Calcis::cycles(p); }
  // Selection (tabs/pages) used by both controller and view
  void initSpecs() {
    using PPage = ::zlkm::ui::ParameterPageT<kRotaryCount>;
    using namespace zlkm::ui;
    // Tab 0: Source
    auto& t0 = selection_.tabs[0];
    t0.pageCount = 3;
    t0.currentPage = 0;
    // Page 0
    {
      auto& p0 = t0.pages[0];
      auto& cfg = *ucfg_.pCfg;
      p0.mappers[0] = ZLKM_UI_LIN_FMAPPER(cycles(65.f), cycles(260.f),
                                          &cfg.cyclesPerSample);
      p0.mappers[1] = ZLKM_UI_RATE_FMAPPER(20.f, 2000.f, CalcisTR::SR,
                                           &cfg.envs[CH::EnvAmp].decay);
      p0.mappers[2] = ZLKM_UI_RATE_FMAPPER(2.f, 80.f, CalcisTR::SR,
                                           &cfg.envs[CH::EnvPitch].decay);
      p0.mappers[3] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &cfg.outGain);
    }
    // Page 1
    {
      auto& p1 = t0.pages[1];
      auto& sw = ucfg_.pCfg->swarmOsc;
      p1.mappers[0] = ZLKM_UI_LIN_FMAPPER(0.01f, 0.99f, &sw.pulseWidth);
      p1.mappers[1] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &sw.morph);
      p1.mappers[2] = ZLKM_UI_LIN_FMAPPER(1.f, 1.05946f, &sw.detuneMul);
      p1.mappers[3] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &sw.stereoSpread);
    }
    // Page 2
    {
      auto& p2 = t0.pages[2];
      auto& sw = ucfg_.pCfg->swarmOsc;
      p2.mappers[0] = ZLKM_UI_INT_MAPPER(1.f, CH::MAX_SWARM_VOICES, &sw.voices);
      p2.mappers[1] = ZLKM_UI_INT_MAPPER(0.f, 1.f, &sw.morphMode);
      p2.mappers[2] = ZLKM_UI_BOOL_MAPPER(&sw.randomPhase);
    }

    // Tab 1: Filter
    auto& t1 = selection_.tabs[1];
    t1.pageCount = 1;
    t1.currentPage = 0;
    {
      auto& p = t1.pages[0];
      using MyFilterMapper = ::zlkm::ui::FilterMapper<CalcisTR::SR>;
      p.mappers[0] = MyFilterMapper::makeResonance(filterParams_);
      p.mappers[1] = MyFilterMapper::makeCutoff(filterParams_);
      p.mappers[2] = MyFilterMapper::makeMorph(filterParams_);
      p.mappers[3] = MyFilterMapper::makeDrive(filterParams_);
    }
  }

  Cfg ucfg_;
  Calcis::Feedback* fb_{};
  zlkm::util::IdleTimer idleTimer_{10000};

  // Pin source reference
  PinSource& pinSrc_;
  audio::SafeFilterParams<CalcisTR::SR> filterParams_;

  // Components
  Selection selection_;
  Sampler sampler_;
  ControllerT controller_;
  ViewT view_;

};  // class UI

}  // namespace zlkm::ch
