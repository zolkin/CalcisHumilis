#pragma once

// Drop-in UI facade built on the refactored ui components (InputSampler,
// Controller, View). It preserves the original UI class name and public API
// to minimize changes elsewhere in the codebase.

#include <Arduino.h>
#include <JLED.h>

#include <array>
#include <memory>

#include "CalcisHumilis.h"
#include "audio/AudioTraits.h"
#include "dsp/Util.h"
#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"
#include "hw/io/GpioPins.h"
#include "hw/io/McpPins.h"
#include "hw/io/QuadManagerIO.h"
#include "ui/Controller.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"
#include "ui/View.h"
#include "util/Profiler.h"

namespace zlkm::ch {

class UI {
 public:
  using CH = Calcis;
  static constexpr float cyc(float p) { return Calcis::cycles(p); }
  using CalcisTR = zlkm::ch::CalcisTR;

  static constexpr size_t kMaxPagesPerTab = 4;  // define sizes explicitly here
  static constexpr size_t kRotaryCount = 4;

  using Selection =
      ::zlkm::ui::ParameterTabControlT<4, kMaxPagesPerTab, kRotaryCount>;

  using PinExpander = hw::io::Mcp23017Pins;
  using Sampler = hw::io::QuadManagerIO<PinExpander, kRotaryCount>;
  using ControllerT = ::zlkm::ui::Controller<Sampler, Selection::count(),
                                             kMaxPagesPerTab, kRotaryCount>;
  using ViewT = ::zlkm::ui::View<Selection>;

  using TabButtons = hw::io::ButtonManager<4, PinExpander>;
  using Encoders = hw::io::QuadManagerIO<PinExpander, kRotaryCount>;

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };

    CH::EnvCfg& env(int idx) { return pCfg->envs[idx]; }

    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_) : pCfg(pCfg_), tabBtns{.pins = {4, 5, 6, 7}} {}

    static constexpr int kNumTabs = 4;

    TabButtons::Cfg tabBtns;

    std::array<uint8_t, kNumTabs> tabPageCount{3, 1, 1, 1};
    std::array<uint8_t, kRotaryCount> encPinsA{0, 2, 4, 13};

    // ParameterTab rotaryTabs[kNumTabs];

    Calcis::Cfg* pCfg;

    float snapMultiplier = 0.0f;
    float activityThresh = 32.f;
    float encClkDiv = 50.0f;
    uint32_t screenIdleMs = 10000;
    uint16_t pollMs = 5;
    uint8_t trigPin = 26;
  };

  using ViewCfg = typename ViewT::Cfg;

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
      : ucfg_(cfg),
        fb_(fb),
        pinExp_(hw::io::I2cCfg{.address = 0x20,
                               .wire = &Wire,
                               .clockHz = 400000,
                               .i2cSDA = 20,
                               .i2cSCL = 21},
                hw::io::PinMode::Output),
        sampler_(pinExp_, typename Sampler::Cfg{.pinsA = {8, 10, 12, 14},
                                                .pinsB = {9, 11, 13, 15},
                                                .usePullUp = true}),
        selection_(),
        controller_(
            *ucfg_.pCfg, pinExp_, ucfg_.tabBtns, *fb_, sampler_, selection_,
            ucfg_.trigPin,
            zlkm::hw::io::ButtonManager<1, zlkm::hw::io::GpioPins<1>>::Cfg{
                .pins = {0},
                .activeLow = true,
                .usePullUp = true,
                .debounceTicks = 5}),
        view_(selection_, pinExp_,
              ViewCfg{.ledPins_ = {0, 1, 2, 3}, .fps = 60, .pCfg = ucfg_.pCfg},
              fb_) {
    initSpecs();
    controller_.seedFromCfg();
    // Move expander-backed buttons/LEDs to Controller; keep expander here
  }

  // Legacy UI API: update does UI and also ticks sampler
  void update() {
    ZLKM_PERF_SCOPE("UI update");
    // Trigger button handled in controller
    sampler_.update();
    processInputs();
    view_.update(controller_.hasActivity());
  }
  void processInputs() { controller_.update(); }

 private:
  // Selection (tabs/pages) used by both controller and view
  void initSpecs() {
    using PPage = ::zlkm::ui::ParameterPageT<kRotaryCount>;
    // Tab 0: Source
    auto& t0 = selection_.tabs[0];
    t0.pageCount = 3;
    t0.currentPage = 0;
    // Page 0
    {
      auto& p0 = t0.pages[0];
      p0.rotary[0] = {Calcis::cycles(65.f), Calcis::cycles(260.f),
                      zlkm::ch::RotaryInputSpec::RsLin,
                      &ucfg_.pCfg->cyclesPerSample};
      p0.rotary[1] = {20.f, 2000.f, zlkm::ch::RotaryInputSpec::RsRate,
                      &ucfg_.pCfg->envs[CH::EnvAmp].decay};
      p0.rotary[2] = {2.f, 80.f, zlkm::ch::RotaryInputSpec::RsRate,
                      &ucfg_.pCfg->envs[CH::EnvPitch].decay};
      p0.rotary[3] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsLin,
                      &ucfg_.pCfg->outGain};
    }
    // Page 1
    {
      auto& p1 = t0.pages[1];
      auto& sw = ucfg_.pCfg->swarmOsc;
      p1.rotary[0] = {0.01f, 0.99f, zlkm::ch::RotaryInputSpec::RsLin,
                      &sw.pulseWidth};
      p1.rotary[1] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsLin, &sw.morph};
      p1.rotary[2] = {1.f, 1.05946f, zlkm::ch::RotaryInputSpec::RsLin,
                      &sw.detuneMul};
      p1.rotary[3] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsLin,
                      &sw.stereoSpread};
    }
    // Page 2
    {
      auto& p2 = t0.pages[2];
      auto& sw = ucfg_.pCfg->swarmOsc;
      p2.rotary[0] = {1.f, CH::MAX_SWARM_VOICES,
                      zlkm::ch::RotaryInputSpec::RsInt, &sw.voices};
      p2.rotary[1] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsInt,
                      &sw.morphMode};
      p2.rotary[2] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsBool,
                      &sw.randomPhase};
      p2.rotary[3] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsLin, nullptr};
    }

    // Tab 1: Filter
    auto& t1 = selection_.tabs[1];
    t1.pageCount = 1;
    t1.currentPage = 0;
    {
      auto& p = t1.pages[0];
      p.rotary[0] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsKDamp,
                     &ucfg_.pCfg->filter};
      p.rotary[1] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsGCut,
                     &ucfg_.pCfg->filter};
      p.rotary[2] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsMorph,
                     &ucfg_.pCfg->filter};
      p.rotary[3] = {0.f, 1.f, zlkm::ch::RotaryInputSpec::RsDrive,
                     &ucfg_.pCfg->filter};
    }
  }

  Cfg ucfg_;
  Calcis::Feedback* fb_{};

  // Hardware pieces reused
  hw::io::Mcp23017Pins pinExp_;

  // Components
  Selection selection_;
  Sampler sampler_;
  ControllerT controller_;
  ViewT view_;

};  // class UI

}  // namespace zlkm::ch
