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
#include "mod/Parameters.h"
#include "platform/boards/Current.h"
#include "platform/platform.h"
#include "ui/Controller.h"
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
  using CurBoard = ::zlkm::platform::boards::Current;
  using SrcPin = CurBoard::SrcPinId;

  static constexpr size_t kMaxPagesPerTab = 4;  // define sizes explicitly here
  static constexpr size_t kRotaryCount = 4;

  using Selection =
      ::zlkm::ui::ParameterTabControlT<4, kMaxPagesPerTab, kRotaryCount>;

  using PinSource = CurBoard::PinSource;
  using Sampler = hw::io::QuadManagerIO<PinSource, kRotaryCount>;
  using SamplerCfg = typename Sampler::Cfg;
  using ControllerT = ::zlkm::ui::Controller<Sampler, Selection::count(),
                                             kMaxPagesPerTab, kRotaryCount>;
  using ViewT = ::zlkm::ui::View<Selection>;

  using TabButtons = hw::io::ButtonManager<4, PinSource>;
  using Encoders = hw::io::QuadManagerIO<PinSource, kRotaryCount>;
  using TrigButton = hw::io::ButtonManager<1, PinSource>;
  using TrigBtnCfg = TrigButton::Cfg;

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };

    mod::EnvCfg& env(int idx) { return pCfg->envs[idx]; }

    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_)
        : pCfg(pCfg_), tabBtns{.pins = CurBoard::TAB_BUTTONS} {}

    static constexpr int kNumTabs = 4;

    TabButtons::Cfg tabBtns;

    std::array<uint8_t, kNumTabs> tabPageCount{3, 1, 1, 1};

    Calcis::Cfg* pCfg;

    float snapMultiplier = 0.0f;
    float activityThresh = 32.f;
    float encClkDiv = 50.0f;
    uint32_t screenIdleMs = 10000;
    uint16_t pollMs = 5;
    SrcPin trigPin = CurBoard::TRIG_IN;  // kept for potential future use
  };

  using ViewCfg = typename ViewT::Cfg;

  static CurBoard::PinSource& pins() { return CurBoard::pins(); }

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
      : ucfg_(cfg),
        fb_(fb),
        idleTimer_(ucfg_.screenIdleMs),
        sampler_(pins(),
                 SamplerCfg{.pins = CurBoard::ENCODER, .usePullUp = true}),
        controller_(
            *ucfg_.pCfg, ucfg_.tabBtns, *fb_, sampler_, selection_,
            TrigBtnCfg{.pins = TrigButton::GroupArrayT{CurBoard::TRIG_IN},
                       .activeLow = true,
                       .usePullUp = true,
                       .debounceTicks = 5}),
        view_(selection_, ViewCfg{.fps = 60, .pCfg = ucfg_.pCfg}, fb_) {
    initSpecs();
    controller_.seedFromCfg();
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
    using namespace zlkm::mod;

    static constexpr int SR = CalcisTR::SR;
    // Tab 0: Source
    auto& t0 = selection_.tabs[0];
    t0.pageCount = 4;
    t0.currentPage = 0;

    auto& sw = ucfg_.pCfg->swarmOsc;
    auto& cfg = *ucfg_.pCfg;
    // Page 0
    {
      auto& p0 = t0.pages[0];
      // Labels (const char*, no allocations)
      p0.labels = {"PIT", "ADEC", "PDEC", "VOL"};
      p0.mappers[0] =
          ZLKM_UI_LIN_FMAPPER(cycles(65.f), cycles(260.f), &sw.cyclesPerSample);
      p0.mappers[1] =
          ZLKM_UI_RATE_FMAPPER(20.f, 2000.f, SR, &cfg.envs[CH::EnvAmp].decay);
      p0.mappers[2] =
          ZLKM_UI_RATE_FMAPPER(2.f, 80.f, SR, &cfg.envs[CH::EnvPitch].decay);
      p0.mappers[3] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &cfg.outGain);
    }
    // Page 1
    {
      auto& p1 = t0.pages[1];
      p1.labels = {"PW", "MRPH", "DET", "SPRD"};
      p1.mappers[0] = ZLKM_UI_LIN_FMAPPER(0.01f, 0.99f, &sw.pulseWidth);
      p1.mappers[1] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &sw.morph);
      p1.mappers[2] = ZLKM_UI_LIN_FMAPPER(1.f, 1.05946f, &sw.detuneMul);
      p1.mappers[3] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &sw.stereoSpread);
    }
    // Page 2
    {
      auto& p2 = t0.pages[2];
      p2.labels = {"UNI", "MMOD", "RPHS", ""};
      p2.mappers[0] = ZLKM_UI_INT_MAPPER(1.f, CH::MAX_SWARM_VOICES, &sw.voices);
      p2.mappers[1] = ZLKM_UI_INT_MAPPER(0.f, 1.f, &sw.morphMode);
      p2.mappers[2] = ZLKM_UI_BOOL_MAPPER(&sw.randomPhase);
    }

    // Page 3: Amp Envelope (Attack/Decay/Depth/Curve)
    {
      auto& p3 = t0.pages[3];
      p3.labels = {"ATK", "DEC", "DEP", "CURV"};
      auto& envAmp = cfg.envs[CH::EnvAmp];
      p3.mappers[0] = ZLKM_UI_RATE_FMAPPER(1.f, 1000.f, SR, &envAmp.attack);
      p3.mappers[1] = ZLKM_UI_RATE_FMAPPER(20.f, 2000.f, SR, &envAmp.decay);
      p3.mappers[2] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &envAmp.depth);
      p3.mappers[3] = EnvCurveMapper::make(envAmp);
    }

    // Tab 1: Filter
    auto& t1 = selection_.tabs[1];
    t1.pageCount = 1;
    t1.currentPage = 0;
    {
      auto& p = t1.pages[0];
      p.labels = {"RES", "CUT", "MRPH", "DRV"};
      auto& fcfg = cfg.filter;
      p.mappers[0] = ZLKM_UI_LIN_FMAPPER(0.707f, 12.f, &fcfg.Q);
      p.mappers[1] = ZLKM_UI_LIN_FMAPPER(20.f, 16000.f, &fcfg.cutoffHz);
      p.mappers[2] = ZLKM_UI_LIN_FMAPPER(0.f, 1.f, &fcfg.morph);
      p.mappers[3] = ZLKM_UI_LIN_FMAPPER(1.f, 16.f, &cfg.drive);
    }
  }

  Cfg ucfg_;
  Calcis::Feedback* fb_{};
  zlkm::util::IdleTimer idleTimer_{10000};

  // Components
  Selection selection_;
  Sampler sampler_;
  ControllerT controller_;
  ViewT view_;

};  // class UI

}  // namespace zlkm::ch
