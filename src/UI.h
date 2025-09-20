#pragma once

#include <AnalogInput.h>
#include <Arduino.h>
#include <JLED.h>
#include <OneButton.h>

#include <array>

#include "AudioTraits.h"
#include "CalcisHumilis.h"

// Identify each pot (extend as you add more)
namespace zlkm {

using CalcisTR = AudioTraits<48000, 1, 32, 64>;
using Calcis = CalcisHumilis<CalcisTR>;

struct PotSource {
  enum PotId { A = 0, B, C, D, Count };

  // Source selection
  bool useAds = false;     // false = internal ADC (RAR), true = ADS1x15
  uint8_t adsChannel = 0;  // 0..3 for ADS1x15 when useAds == true

  // For internal ADC
  uint8_t pin = A0;
  bool invert = false;  // set true if wired “backwards”
};

struct PotSpec {
  enum PotResponse { RsLin, RsExp, RsRate, RsGCut, RsKDamp };

  // Mapping
  float outMin = 0.0f;
  float outMax = 1.0f;

  PotResponse response = RsLin;

  float* cfgValue = nullptr;

  inline bool setCfgValue(float value) const {
    *cfgValue = value;
    return true;
  }
};

struct ParameterPage {
  bool enabled = false;

  PotSpec pots[PotSource::Count];
};

struct ParameterTab {
  static constexpr int MAX_PAGE = 4;

  bool enabled = false;

  ParameterPage pages[MAX_PAGE];
};

class UI {
 public:
  static constexpr float cyc(float p) { return Calcis::cycles(p); }

  struct Cfg {
    enum Tabs { TabSrc = 0, TabFilter, TabCount };
    Cfg(const Cfg&) = delete;
    Cfg(Calcis::Cfg* pCfg_) : pCfg(pCfg_) {
      potTabs[TabSrc].enabled = true;
      potTabs[TabSrc].pages[0] = ParameterPage{
          true,
          {
              /*  MIN,   MAX, INV,  RESPONSE, parameter* */
              {cyc(130.f), cyc(16640.f), PotSpec::RsLin,
               &pCfg->cyclesPerSample},                       /* Pitch */
              {20.f, 2000.f, PotSpec::RsRate, &pCfg->ampDec}, /* Dec */
              {2.f, 80.f, PotSpec::RsRate, &pCfg->pitchDec},  /* PDec*/
              {0.f, 1.f, PotSpec::RsLin, &pCfg->outGain},     /* Gain*/
          }};

      auto& sw = pCfg->swarmOsc;
      potTabs[TabSrc].pages[1] = ParameterPage{
          true,
          {
              /*  MIN,  MAX,  RESPONSE     , parameter* */
              {0.01f, 0.99f, PotSpec::RsLin, &sw.pulseWidth}, /* PWM*/
              {0.f, 1.f, PotSpec::RsLin, &sw.morph},          /* morph */
              {1.f, 1.2599f, PotSpec::RsLin, &sw.detuneMul},  /* Detune */
              {0.f, 1.f, PotSpec::RsLin, &sw.stereoSpread},   /* Spread */
          }};

      potTabs[TabFilter].enabled = true;
      potTabs[TabFilter].pages[0] =
          ParameterPage{true,
                        {
                            /*  MIN,   MAX, INV,  RESPONSE, parameter* */
                            {0.f, 1.f, PotSpec::RsKDamp, &pCfg->filterKDamp},
                            {20.f, 20000.f, PotSpec::RsGCut, &pCfg->filterGCut},
                            {0.f, 1.f, PotSpec::RsLin, &pCfg->filterMorph},
                            {0.f, 16.f, PotSpec::RsLin, &pCfg->filterDrive},
                        }};
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
    uint16_t pollMs = 5;           // ~200 Hz
    float snapMultiplier = 0.01f;  // ResponsiveAnalogRead smoothing
    float activityThresh = 1.0f;   // counts (default in RAR ≈ 4)

    Calcis::Cfg* pCfg;

    PotSource potSources[PotSource::Count] = {
        /* ADS ,CH,PIN, INV*/
        {true, 1, 0, false},
        {false, 0, A2, false},
        {false, 0, A1, false},
        {false, 0, A0, false},
    };

    // Pots
    ParameterTab potTabs[kNumTabs];
  };

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb);

  void update();  // call each loop()
  static void waitForSerial(unsigned long timeoutMs = 3000);

  void attachADS();

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

  // Pots
  std::array<AnalogInput, static_cast<size_t>(PotSource::Count)> pots_;
  uint32_t tPrev_ = 0;

  // Tabs / pages state
  uint8_t currentTab_ = 0;
  uint8_t currentPage_[kNumTabs] = {0, 0, 0, 0};

  // ADS  (optional)
  Adafruit_ADS1015 ads_;
  float adsVref_ = 3.3f;  // your pot runs off 3V3

  // LEDs
  JLed triggerLED{2};
  JLed clippingLED{3};

  void tickLED();

  // Setup helpers
  void initAdc_();
  void initPots_(bool adsOk);
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
  bool processPot_(int id);

  // NEW: Tabs / pages
  void scanTabButtons_();
  void selectTab_(uint8_t tab);
  void advancePage_();
  void updateLeds_();
  void blinkLed_(uint8_t tab, uint8_t count);
};

}  // namespace zlkm

#include "UI_impl.hh"