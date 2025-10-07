#pragma once

// Drop-in UI facade built on the refactored ui components (InputSampler,
// Controller, View). It preserves the original UI class name and public API
// to minimize changes elsewhere in the codebase.

#include <Arduino.h>

#include <array>

#include "CalcisHumilis.h"
#include "audio/AudioTraits.h"
#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"
#include "hw/io/McpPins.h"
#include "hw/io/QuadManagerIO.h"
#include "hw/screensavers/SaverMux.h"
#include "hw/screensavers/StarField.h"
#include "hw/screensavers/ThroughTheStars.h"
#include "ui/Controller.h"
#include "ui/InputSampler.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"
#include "ui/View.h"

namespace zlkm::ch {

class UI {
 public:
  using CH = Calcis;
  static constexpr float cyc(float p) { return Calcis::cycles(p); }

  using PinExpander = hw::io::Mcp23017Pins;
  using TabButtons = hw::io::ButtonManager<4, PinExpander>;
  using Encoders =
      hw::io::QuadManagerIO<PinExpander, ParameterPage::ROTARY_COUNT>;

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };

    CH::EnvCfg& env(int idx) { return pCfg->envs[idx]; }

    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_) : pCfg(pCfg_), tabBtns{.pins = {4, 5, 6, 7}} {
      rotaryTabs[TabSrc].enabled = true;
      rotaryTabs[TabSrc].pages[0] = ParameterPage{
          {
              {cyc(65.f), cyc(260.f), RotaryInputSpec::RsLin,
               &pCfg->cyclesPerSample},
              {20.f, 2000.f, RotaryInputSpec::RsRate, &env(CH::EnvAmp).decay},
              {2.f, 80.f, RotaryInputSpec::RsRate, &env(CH::EnvPitch).decay},
              {0.f, 1.f, RotaryInputSpec::RsLin, &pCfg->outGain},
          },
          true};

      auto& sw = pCfg->swarmOsc;
      rotaryTabs[TabSrc].pages[1] = ParameterPage{
          {
              {0.01f, 0.99f, RotaryInputSpec::RsLin, &sw.pulseWidth},
              {0.f, 1.f, RotaryInputSpec::RsLin, &sw.morph},
              {1.f, 1.05946f, RotaryInputSpec::RsLin, &sw.detuneMul},
              {0.f, 1.f, RotaryInputSpec::RsLin, &sw.stereoSpread},
          },
          true};
      rotaryTabs[TabSrc].pages[2] = ParameterPage{
          {
              {1.f, Calcis::MAX_SWARM_VOICES, RotaryInputSpec::RsInt,
               &sw.voices},
              {0.f, 1.f, RotaryInputSpec::RsInt, &sw.morphMode},
              {0.f, 1.f, RotaryInputSpec::RsBool, &sw.randomPhase},
              {0.f, 1.f, RotaryInputSpec::RsLin, nullptr},
          },
          true};

      rotaryTabs[TabFilter].enabled = true;
      rotaryTabs[TabFilter].pages[0] =
          ParameterPage{{
                            {0.f, 1.f, RotaryInputSpec::RsKDamp, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsGCut, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsMorph, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsDrive, &pCfg->filter},
                        },
                        true};
    }

    uint8_t trigPin = 26;
    static constexpr int kNumTabs = 4;
    TabButtons::Cfg tabBtns;
    std::array<uint8_t, kNumTabs> tabPageCount{3, 1, 1, 1};
    uint16_t pollMs = 5;
    float snapMultiplier = 0.0f;
    float activityThresh = 32.f;
    Calcis::Cfg* pCfg;
    std::array<uint8_t, ParameterPage::ROTARY_COUNT> encPinsA{0, 2, 4, 13};
    float encClkDiv = 50.0f;
    uint32_t screenIdleMs = 10000;
    ParameterTab rotaryTabs[kNumTabs];
  };

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
      : ucfg_(cfg),
        fb_(fb),
        pinExp_(hw::io::I2cCfg{.address = 0x20,
                               .wire = &Wire,
                               .clockHz = 400000,
                               .i2cSDA = 20,
                               .i2cSCL = 21},
                hw::io::PinMode::Output),
        sampler_(pinExp_,
                 typename Sampler::Cfg{.encCfg = {.pinsA = {8, 10, 12, 14},
                                                  .pinsB = {9, 11, 13, 15},
                                                  .usePullUp = true},
                                       .pollUs = 1000}),
        view_(&pinExp_, &selection_,
              typename ViewT::Cfg{.tabBtns = ucfg_.tabBtns, .fps = 60}),
        controller_(ucfg_.pCfg, fb_, &selection_, &sampler_) {
    loadSpecsFromCfg_();
    controller_.seedFromCfg();
  }

  // Legacy UI API: update does UI and also ticks sampler
  void update() {
    sampler_.tick();
    if (uiProcessesInputs_) controller_.process();
    view_.update();
  }

  // New: processing to be called from audio loop/core
  void processInputs() { controller_.process(); }

  // When wiring audio loop, call setUiProcessesInputs(false)
  void setUiProcessesInputs(bool enabled) { uiProcessesInputs_ = enabled; }

 private:
  // Selection (tabs/pages) used by both controller and view
  using ProcPage = zlkm::ui::ProcPage;
  using TabSrc = zlkm::ui::Tab<ProcPage, ProcPage, ProcPage>;
  using TabFilter = zlkm::ui::Tab<ProcPage>;
  using Selection = zlkm::ui::TabControl<TabSrc, TabFilter>;

  using Sampler =
      zlkm::ui::InputSampler<hw::io::Mcp23017Pins, ParameterPage::ROTARY_COUNT>;
  using ControllerT =
      zlkm::ui::Controller<hw::io::Mcp23017Pins, ParameterPage::ROTARY_COUNT,
                           TabSrc, TabFilter>;
  using ViewT = zlkm::ui::View<Selection>;

  Cfg ucfg_;
  Calcis::Feedback* fb_{};

  // Hardware pieces reused
  hw::io::Mcp23017Pins pinExp_;

  // Shared selection state
  Selection selection_{};

  // Components
  Sampler sampler_;
  ViewT view_;
  ControllerT controller_;
  bool uiProcessesInputs_ = true;

  void loadSpecsFromCfg_() {
    // Tab 0: Source, 3 pages
    auto& t0 = std::get<0>(selection_.tabs);
    {
      // page 0
      auto& dst0 = std::get<0>(t0.pages);
      auto& src0 = ucfg_.rotaryTabs[Cfg::TabSrc].pages[0];
      dst0.enabled = src0.enabled;
      for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i)
        dst0.spec[i] = src0.rotary[i];
      // page 1
      auto& dst1 = std::get<1>(t0.pages);
      auto& src1 = ucfg_.rotaryTabs[Cfg::TabSrc].pages[1];
      dst1.enabled = src1.enabled;
      for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i)
        dst1.spec[i] = src1.rotary[i];
      // page 2
      auto& dst2 = std::get<2>(t0.pages);
      auto& src2 = ucfg_.rotaryTabs[Cfg::TabSrc].pages[2];
      dst2.enabled = src2.enabled;
      for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i)
        dst2.spec[i] = src2.rotary[i];
    }
    // Tab 1: Filter, 1 page
    auto& t1 = std::get<1>(selection_.tabs);
    {
      auto& dst = std::get<0>(t1.pages);
      auto& src = ucfg_.rotaryTabs[Cfg::TabFilter].pages[0];
      dst.enabled = src.enabled;
      for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i)
        dst.spec[i] = src.rotary[i];
    }
  }
};

}  // namespace zlkm::ch
