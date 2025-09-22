#pragma once

#include <Arduino.h>
#include <JLED.h>
#include <OneButton.h>

#include <array>

#include "AudioTraits.h"
#include "CalcisHumilis.h"
#include "hw\ADSPinReader.h"
#include "MultiInput.h"

// Identify each pot (extend as you add more)
namespace zlkm {

using CalcisTR = AudioTraits<48000, 1, 32, 64>;
using Calcis = CalcisHumilis<CalcisTR>;
using ClcisPots = MultiInput<hw::ADS1015Reader>;

struct PotSpec {
  enum PotResponse {
    RsLin,
    RsExp,
    RsRate,
    RsGCut,
    RsKDamp,
    RsMorph,
    RsDrive,
    RsInt
  };

  // Mapping
  float outMin = 0.0f;
  float outMax = 1.0f;

  PotResponse response = RsLin;

  void* cfgValue = nullptr;

  template <class T>
  inline void setCfgValue(T value) const {
    *reinterpret_cast<T*>(cfgValue) = value;
  }
};

struct ParameterPage {
  static constexpr int POT_COUNT = 4;

  PotSpec pots[POT_COUNT];
  bool enabled = false;
};

struct ParameterTab {
  static constexpr int MAX_PAGE = 4;

  ParameterPage pages[MAX_PAGE];
  bool enabled = false;
};

class UI {
 public:
  static constexpr float cyc(float p) { return Calcis::cycles(p); }
  using CH = Calcis;

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };

    CH::EnvCfg& env(int idx) { return pCfg->envs[idx]; }

    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_) : pCfg(pCfg_) {
      potTabs[TabSrc].enabled = true;
      potTabs[TabSrc].pages[0] = ParameterPage{
          {
              /*  MIN,   MAX, INV,  RESPONSE, parameter* */
              {cyc(65.f), cyc(260.f), PotSpec::RsLin,
               &pCfg->cyclesPerSample}, /* Pitch */
              {20.f, 2000.f, PotSpec::RsRate, &env(CH::EnvAmp).decay}, /* Dec */
              {2.f, 80.f, PotSpec::RsRate, &env(CH::EnvPitch).decay},  /* PDec*/
              {0.f, 1.f, PotSpec::RsLin, &pCfg->outGain},              /* Gain*/
          },
          true};

      auto& sw = pCfg->swarmOsc;
      potTabs[TabSrc].pages[1] = ParameterPage{
          {
              /*  MIN,  MAX,  RESPONSE     , parameter* */
              {0.01f, 0.99f, PotSpec::RsLin, &sw.pulseWidth}, /* PWM*/
              {0.f, 1.f, PotSpec::RsLin, &sw.morph},          /* morph */
              {1.f, 1.05946f, PotSpec::RsLin, &sw.detuneMul}, /* Detune */
              {0.f, 1.f, PotSpec::RsLin, &sw.stereoSpread},   /* Spread */
          },
          true};
      potTabs[TabSrc].pages[2] = ParameterPage{
          {
              /*  MIN,  MAX,  RESPONSE     , parameter* */
              {1.f, Calcis::MAX_SWARM_VOICES, PotSpec::RsInt,
               &sw.voices},                            /* PWM*/
              {0.f, 1.f, PotSpec::RsLin, nullptr},     /* morph */
              {1.f, 1.2599f, PotSpec::RsLin, nullptr}, /* Detune */
              {0.f, 1.f, PotSpec::RsLin, nullptr},     /* Spread */
          },
          true};

      potTabs[TabFilter].enabled = true;
      potTabs[TabFilter].pages[0] =
          ParameterPage{{
                            /*  MIN,   MAX, INV,  RESPONSE, parameter* */
                            {0.f, 1.f, PotSpec::RsKDamp, &pCfg->filter},
                            {0.f, 1.f, PotSpec::RsGCut, &pCfg->filter},
                            {0.f, 1.f, PotSpec::RsMorph, &pCfg->filter},
                            {0.f, 1.f, PotSpec::RsDrive, &pCfg->filter},
                        },
                        true};
    }

    // Trigger button (unchanged)
    uint8_t trigPin = 6;

    // NEW: Tab buttons and LEDs
    static constexpr int kNumTabs = 4;

    uint8_t tabBtnPins[kNumTabs] = {14, 15, 16, 17};  // GP14..GP17
    uint8_t tabLedPins[kNumTabs] = {18, 19, 20, 21};  // GP18..GP21

    // NEW: per-tab page counts (can change later)
    uint8_t tabPageCount[kNumTabs] = {3, 1, 1, 1};  // Tab 0 = 1 page for now

    // Scanning/timing
    uint16_t pollMs = 5;          // ~200 Hz
    float snapMultiplier = 0.0f;  // ResponsiveAnalogRead smoothing
    float activityThresh = 32.f;  // counts (default in RAR ≈ 4)

    Calcis::Cfg* pCfg;

    hw::ADS1015Reader::Cfg readerCfg = {
        4,                    /* i2cSDA */
        5,                    /* i2cSCL */
        400000,               /* i2cHz */
        0x48,                 /* i2cAddr */
        GAIN_ONE,             /* gain */
        RATE_ADS1015_3300SPS, /* rate */
        3.3f                  /* vrefVolts */
    };

    ClcisPots::Cfg potsCfg = {
        {0, 1, 2, 3}, /* channels */
        4095,         /* maxCode */
        0.2f,         /* emalAlpha */
        32.f,         /* activityLSB */
    };

    // Pots
    ParameterTab potTabs[kNumTabs];
  };

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb);

  void update();  // call each loop()

 private:
  Cfg ucfg_;
  Calcis::Feedback* fb_;
  int saturationCounter_ = 0;

  // Buttons
  OneButton trigBtn_;
  static constexpr int kNumTabs = Cfg::kNumTabs;
  std::array<OneButton, kNumTabs> tabBtns_;  // NEW: OneButton for tabs

  // Context passed to OneButton callbacks (so we know which tab fired)
  struct TabCtx {
    UI* self;
    uint8_t idx;
  };
  std::array<TabCtx, kNumTabs> tabCtx_;  // NEW

  SafeFilterParams<CalcisTR::SR> filterParams_;

  // Pots
  hw::ADS1015Reader adsPinReader_;
  ClcisPots multiInput_;

  uint32_t tPrev_ = 0;

  // Tabs / pages state
  uint8_t currentTab_ = 0;
  uint8_t currentPage_[kNumTabs] = {0, 0, 0, 0};

  // LEDs
  JLed triggerLED{2};
  JLed clippingLED{3};

  void tickLED();

  // Setup helpers
  void initTabs_();

  // Handlers
  static void onPress_(void* param);     // trigger
  static void onTabPress_(void* param);  // NEW: tab press (select/advance)

  const PotSpec& getPotSpec(int index) const {
    return ucfg_.potTabs[currentTab_]
        .pages[currentPage_[currentTab_]]
        .pots[index];
  }

  // Mapping helpers
  static inline float mapLin_(int raw, int rawMax, float outMin, float outMax,
                              bool invert);
  static inline float mapExp_(int raw, int rawMax, float outMin, float outMax,
                              bool invert);

  // Per-pot processing — returns true if config changed
  void processPot_(int id);

  // NEW: Tabs / pages
  void scanTabButtons_();
  void selectTab_(uint8_t tab);
  void advancePage_();
  void updateLeds_();
  void blinkLed_(uint8_t tab, uint8_t count);
};

}  // namespace zlkm

#include "UI_impl.hh"