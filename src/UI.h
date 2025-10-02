#pragma once

#include <Arduino.h>
#include <JLED.h>
#include <OneButton.h>

#include <array>

#include "AudioTraits.h"
#include "CalcisHumilis.h"
#include "MultiInput.h"
#include "hw/pico/QuadManagerPio.h"
#include "hw\ADSPinReader.h"

// Identify each pot (extend as you add more)
namespace zlkm {

using CalcisTR = AudioTraits<48000, 1, 32, 64>;
using Calcis = CalcisHumilis<CalcisTR>;
using ClcisPots = MultiInput<hw::ADS1015Reader>;

struct RotaryInputSpec {
  enum Response {
    RsLin,
    RsExp,
    RsRate,
    RsGCut,
    RsKDamp,
    RsMorph,
    RsDrive,
    RsInt,
    RsBool
  };

  // Mapping
  float outMin = 0.0f;
  float outMax = 1.0f;

  Response response = RsLin;

  void* cfgValue = nullptr;

  // 0 = use default (two turns). Otherwise, e.g. 96, 192, 384…
  int encSpanCounts = 0;
  // optional starting offset in counts (applied before wrap)
  int encOffset = 0;
  bool encWrap = false;    // true: wrap within span; false: clamp 0..span
  bool encInvert = false;  // per-control direction flip

  template <class T>
  inline void setCfgValue(T value) const {
    *reinterpret_cast<T*>(cfgValue) = value;
  }
};

struct ParameterPage {
  static constexpr int ROTARY_COUNT = 4;

  RotaryInputSpec rotary[ROTARY_COUNT];
  bool enabled = false;
};

using ClcisEncs = hw::QuadManagerPIO<ParameterPage::ROTARY_COUNT>;

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
      rotaryTabs[TabSrc].enabled = true;
      rotaryTabs[TabSrc].pages[0] = ParameterPage{
          {
              /*  MIN,   MAX, INV,  RESPONSE, parameter* */
              {cyc(65.f), cyc(260.f), RotaryInputSpec::RsLin,
               &pCfg->cyclesPerSample}, /* Pitch */
              {20.f, 2000.f, RotaryInputSpec::RsRate,
               &env(CH::EnvAmp).decay}, /* Dec */
              {2.f, 80.f, RotaryInputSpec::RsRate,
               &env(CH::EnvPitch).decay},                         /* PDec*/
              {0.f, 1.f, RotaryInputSpec::RsLin, &pCfg->outGain}, /* Gain*/
          },
          true};

      auto& sw = pCfg->swarmOsc;
      rotaryTabs[TabSrc].pages[1] = ParameterPage{
          {
              /*  MIN,  MAX,  RESPONSE     , parameter* */
              {0.01f, 0.99f, RotaryInputSpec::RsLin, &sw.pulseWidth}, /* PWM*/
              {0.f, 1.f, RotaryInputSpec::RsLin, &sw.morph}, /* morph */
              {1.f, 1.05946f, RotaryInputSpec::RsLin,
               &sw.detuneMul},                                      /* Detune */
              {0.f, 1.f, RotaryInputSpec::RsLin, &sw.stereoSpread}, /* Spread */
          },
          true};
      rotaryTabs[TabSrc].pages[2] = ParameterPage{
          {
              /*  MIN,  MAX,  RESPONSE     , parameter* */
              {1.f, Calcis::MAX_SWARM_VOICES, RotaryInputSpec::RsInt,
               &sw.voices},                                      /* PWM*/
              {0.f, 1.f, RotaryInputSpec::RsInt, &sw.morphMode}, /* morphMode */
              {0.f, 1.f, RotaryInputSpec::RsBool,
               &sw.randomPhase},                           /* randomPhase */
              {0.f, 1.f, RotaryInputSpec::RsLin, nullptr}, /* Spread */
          },
          true};

      rotaryTabs[TabFilter].enabled = true;
      rotaryTabs[TabFilter].pages[0] =
          ParameterPage{{
                            /*  MIN,   MAX, INV,  RESPONSE, parameter* */
                            {0.f, 1.f, RotaryInputSpec::RsKDamp, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsGCut, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsMorph, &pCfg->filter},
                            {0.f, 1.f, RotaryInputSpec::RsDrive, &pCfg->filter},
                        },
                        true};
    }

    // Trigger button (unchanged)
    uint8_t trigPin = 26;

    // NEW: Tab buttons and LEDs
    static constexpr int kNumTabs = 4;

    std::array<uint8_t, kNumTabs> tabBtnPins{15, 16, 17, 18};  // GP14..GP17
    std::array<uint8_t, kNumTabs> tabLedPins{19, 20, 21, 22};  // GP18..GP21
    std::array<uint8_t, kNumTabs> tabPageCount{3, 1, 1, 1};

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

    std::array<uint8_t, ParameterPage::ROTARY_COUNT> encPinsA{0, 2, 4, 13};
    float encClkDiv = 50.0f;  // ~40 kHz sample

    // Rotary Input
    ParameterTab rotaryTabs[kNumTabs];
  };

  explicit UI(Calcis::Cfg* cfg, Calcis::Feedback* fb);

  void update();  // call each loop()

 private:
  // Encoder geometry: 24 PPR, quadrature x4 => 96 counts/rev, two turns => 192
  static constexpr int kEncCountsPerRev = 24 * 4;
  static constexpr int kEncTwoTurnSpan = 2 * kEncCountsPerRev;
  static constexpr int kAdcMaxCode = 4095;

  Cfg ucfg_;
  Calcis::Feedback* fb_;
  int saturationCounter_ = 0;

  // Buttons
  OneButton trigBtn_;
  static constexpr int kNumTabs = Cfg::kNumTabs;
  std::array<OneButton, kNumTabs> tabBtns_;
  struct TabCtx {
    UI* self;
    uint8_t idx;
  };
  std::array<TabCtx, kNumTabs> tabCtx_;

  struct PageState {
    std::array<int, ParameterPage::ROTARY_COUNT> rawPos{0, 0, 0, 0};
  };
  struct TabState {
    uint8_t currentPage = 0;
    std::array<PageState, ParameterTab::MAX_PAGE> pages{};
  };

  uint8_t currentTab_ = 0;
  std::array<TabState, kNumTabs> tabs_{};

  SafeFilterParams<CalcisTR::SR> filterParams_;

  uint32_t tPrev_ = 0;

  // LEDs
  JLed triggerLED{27};
  JLed clippingLED{28};

  ClcisEncs encs_;
  std::array<int32_t, ParameterPage::ROTARY_COUNT> encLast_{0, 0, 0, 0};

  void tickLED();

  // Setup helpers
  void initTabs_();

  // Handlers
  static void onPress_(void* param);     // trigger
  static void onTabPress_(void* param);  // NEW: tab press (select/advance)

  const RotaryInputSpec& getRotaryInputSpec(int index) const {
    return ucfg_.rotaryTabs[currentTab_]
        .pages[tabs_[currentTab_].currentPage]
        .rotary[index];
  }

  // Mapping helpers
  static inline float mapLin_(int raw, int rawMax, float outMin, float outMax,
                              bool invert);
  static inline float mapExp_(int raw, int rawMax, float outMin, float outMax,
                              bool invert);

  // Per-pot processing — returns true if config changed
  void processRotary_(int id);

  // NEW: Tabs / pages
  void scanTabButtons_();
  void selectTab_(uint8_t tab);
  void advancePage_();
  void updateLeds_();
  void blinkLed_(uint8_t tab, uint8_t count);
  void resetEncoderBaselines_();

  static inline int spanFor_(const zlkm::UI* self, int id) {
    const auto& spec = self->getRotaryInputSpec(id);
    return (spec.encSpanCounts > 0) ? spec.encSpanCounts : kEncTwoTurnSpan;
  }

  static inline int deltaRawFromEnc_(const zlkm::UI* self, int id,
                                     int32_t deltaCounts) {
    const auto& spec = self->getRotaryInputSpec(id);
    if (spec.encInvert) deltaCounts = -deltaCounts;
    const int span = spanFor_(self, id);
    const float scale = (span > 0) ? (float(kAdcMaxCode) / float(span)) : 0.0f;
    return int(lroundf(float(deltaCounts) * scale));
  }

  void seedRawFromCfg_();
};

}  // namespace zlkm

#include "UI_impl.hh"