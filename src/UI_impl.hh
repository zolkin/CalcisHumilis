#include <ArduinoLog.h>
#include <math.h>

#include "UI.h"

namespace zlkm::ch {

static constexpr int kAdcMaxCode = 4095;  // RP2040 12-bit

// ---------- ctor ----------
UI::UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
    : ucfg_(cfg),
      fb_(fb),
      trigBtn_(ucfg_.trigPin, /*activeLow=*/true, /*pullupActive=*/true),
      encs_(pio0, ucfg_.encPinsA, ucfg_.encClkDiv),
      screen_(ScreenSSD::Cfg()) {
  initTabs_();
  seedRawFromCfg_();

  resetEncoderBaselines_();

  trigBtn_.attachPress(UI::onPress_, this);
  trigBtn_.setDebounceMs(5);

  Log.notice(F("[UI] ready" CR));
}

void UI::resetEncoderBaselines_() {
  for (size_t i = 0; i < encLast_.size(); ++i) {
    encLast_[i] = encs_.read(static_cast<int>(i));
  }
}

void UI::tickLED() {
  if (saturationCounter_ < fb_->saturationCounter) {
    clippingLED.FadeOff(80);
    saturationCounter_ = fb_->saturationCounter;
  }
  triggerLED.Update();
  clippingLED.Update();
}

inline float stickEnds(float f) {
  static constexpr float stickyEnds = 0.05;
  static constexpr float reducedRangeScale = 1.f / (1.f - 2.f * stickyEnds);
  if (f < stickyEnds) {
    return 0.f;
  }
  if (f > 1.f - stickyEnds) {
    return 1.f;
  }
  return f * reducedRangeScale;
}

// ---------- map helpers ----------
inline float UI::mapLin_(int raw, int rawMax, float outMin, float outMax,
                         bool invert = false) {
  raw = math::clamp(raw, 0, rawMax);
  float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
  if (invert) x = 1.0f - x;
  x = stickEnds(x);
  return outMin + x * (outMax - outMin);
}

inline float UI::mapExp_(int raw, int rawMax, float outMin, float outMax,
                         bool invert = false) {
  if (rawMax <= 0) return outMin;
  float x = float(math::clamp(raw, 0, rawMax)) / rawMax;

  if (outMin < 0 && outMax <= 0) {
    float dB = outMin + (invert ? 1 - x : x) * (outMax - outMin);
    return powf(10.f, dB * 0.05f);  // 0.05 = 1/20
  }

  float y = powf(x, 1.6f);
  if (invert) y = 1.0f - y;
  y = stickEnds(y);
  return outMin + (outMax - outMin) * y;
}

inline float potToRate(int raw, int rawMax, float msMin, float msMax,
                       float SR) {
  raw = math::clamp(raw, 0, rawMax);
  const float pot = float(raw) / float(rawMax);  // 0..1
  float ms = msMin + pot * (msMax - msMin);
  return dsp::msToRate(ms, SR);  // == 1 / (SR * ms / 1000)
}

// ---------- process one pot ----------
void UI::processRotary_(int id) {
  const RotaryInputSpec& spec = getRotaryInputSpec(id);
  if (spec.cfgValue == nullptr) {
    return;
  }

  int raw = tabs_[currentTab_].pages[tabs_[currentTab_].currentPage].rawPos[id];

  Log.infoln("ENC[%d] value = %d", id, raw);

  switch (spec.response) {
    case RotaryInputSpec::RsLin:
      spec.setCfgValue(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      break;
    case RotaryInputSpec::RsExp:
      spec.setCfgValue(mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      break;
    case RotaryInputSpec::RsRate:
      spec.setCfgValue(
          potToRate(raw, kAdcMaxCode, spec.outMin, spec.outMax, CalcisTR::SR));
      break;
    case RotaryInputSpec::RsGCut:
      filterParams_.setCutoff01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case RotaryInputSpec::RsKDamp:
      filterParams_.setRes01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case RotaryInputSpec::RsMorph:
      filterParams_.setMorph01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;

    case RotaryInputSpec::RsDrive:
      filterParams_.setDrive01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case RotaryInputSpec::RsInt: {
      int val = int(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      Log.infoln("Setting RsInt to %d", val);
      spec.setCfgValue(val);
    } break;
    case RotaryInputSpec::RsBool: {
      bool val = bool(int(mapLin_(raw, kAdcMaxCode, 0.f, 1.f)));
      Log.infoln("Setting RsBool to %d", val);
      spec.setCfgValue(val);
    } break;
    default:
      break;
  }
}

// ===================== NEW: Tabs & Pages =====================

// Setup pins and internal state
// ---------- init tabs (OneButton) ----------
void UI::initTabs_() {
  // LEDs
  for (int i = 0; i < kNumTabs; ++i) {
    pinMode(ucfg_.tabLedPins[i], OUTPUT);
    digitalWrite(ucfg_.tabLedPins[i], LOW);
  }

  // OneButton for each tab button (INPUT_PULLUP, activeLow=true)
  for (int i = 0; i < kNumTabs; ++i) {
    tabBtns_[i] = OneButton(ucfg_.tabBtnPins[i], /*activeLow=*/false,
                            /*pullupActive=*/false);
    tabBtns_[i].setDebounceMs(3);
    tabCtx_[i] = TabCtx{this, static_cast<uint8_t>(i)};
    tabBtns_[i].attachPress(UI::onTabPress_, &tabCtx_[i]);
  }

  // Start on Tab 0
  currentTab_ = 0;
  for (auto& t : tabs_) t.currentPage = 0;
  updateLeds_();
}

/// ---------- main loop ----------
void UI::update() {
  // Tick all buttons
  trigBtn_.tick();

  for (int i = 0; i < kNumTabs; ++i) tabBtns_[i].tick();

  const uint32_t now = millis();
  if (now - tPrev_ < ucfg_.pollMs) return;
  tPrev_ = now;

  // Only enabled tabs processes pots (unchanged behavior)
  if (ucfg_.rotaryTabs[currentTab_].enabled) {
    encs_.update();  // drain PIO FIFOs
    auto& ts = tabs_[currentTab_];
    const uint8_t pg = ts.currentPage;
    for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i) {
      const int32_t nowCnt = encs_.read(i);
      int32_t d = nowCnt - encLast_[i];
      if (d != 0) {
        encLast_[i] = nowCnt;
        int deltaRaw = deltaRawFromEnc_(this, i, d);
        int& raw = ts.pages[pg].rawPos[i];
        raw += deltaRaw;
        if (raw < 0) raw = 0;
        if (raw > kAdcMaxCode) raw = kAdcMaxCode;
        processRotary_(i);
      }
    }
  }

  screen_.update([&](U8G2& g) {
    g.setFont(u8g2_font_6x12_tf);
    g.drawStr(0, 12, "Calcis Humilis");
    // char buf[32]; snprintf(buf, sizeof(buf), "Cutoff: %d", ui.cutoff());
    // g.drawStr(0, 28, buf);
    g.drawFrame(0, 0, screen_.width(), screen_.height());
  });

  tickLED();
}

// ---------- trigger press (unchanged) ----------
void UI::onPress_(void* param) {
  UI* self = reinterpret_cast<UI*>(param);
  ++(self->ucfg_.pCfg->trigCounter);
  self->triggerLED.FadeOff(
      dsp::rateToMs(self->ucfg_.env(CH::EnvAmp).decay, CalcisTR::SR));
}

// ---------- tab press (select or advance) ----------
void UI::onTabPress_(void* param) {
  auto* ctx = reinterpret_cast<TabCtx*>(param);
  UI* self = ctx->self;
  const uint8_t idx = ctx->idx;

  if (idx == self->currentTab_) {
    self->advancePage_();
  } else {
    self->selectTab_(idx);
  }
}

// ---------- select tab / advance page / LEDs (unchanged logic) ----------
void UI::selectTab_(uint8_t tab) {
  if (tab >= kNumTabs) return;
  currentTab_ =
      tab;  // (typo: remove 'self->' if pasting—should be just currentTab_)
  uint8_t pc = ucfg_.tabPageCount[currentTab_];
  if (pc == 0) pc = 1;
  if (tabs_[currentTab_].currentPage >= pc) tabs_[currentTab_].currentPage = 0;
  updateLeds_();
  resetEncoderBaselines_();
}

void UI::advancePage_() {
  uint8_t pc = ucfg_.tabPageCount[currentTab_];
  if (pc <= 1) {
    blinkLed_(currentTab_, 1);
    return;
  }
  auto& ts = tabs_[currentTab_];
  ts.currentPage = (ts.currentPage + 1) % pc;
  blinkLed_(currentTab_, ts.currentPage + 1);
  updateLeds_();
  resetEncoderBaselines_();
}

void UI::updateLeds_() {
  for (uint8_t i = 0; i < kNumTabs; ++i) {
    digitalWrite(ucfg_.tabLedPins[i], (i == currentTab_) ? HIGH : LOW);
  }
}

void UI::blinkLed_(uint8_t tab, uint8_t count) {
  const uint8_t pin = ucfg_.tabLedPins[tab];
  digitalWrite(pin, LOW);
  delay(100);
  for (uint8_t n = 0; n < count; ++n) {
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
  }
  if (tab == currentTab_) digitalWrite(pin, HIGH);
}

// inverse of mapLin_ to raw (ignores sticky-ends during seeding on purpose)
static inline int inv_toRaw_Lin_(float value, float outMin, float outMax) {
  if (outMax == outMin) return 0;
  float t = (value - outMin) / (outMax - outMin);  // 0..1
  t = math::clamp01(t);
  return int(lroundf(t * kAdcMaxCode));
}

// inverse of mapExp_ (common case with gamma=1.6, positive ranges)
static inline int inv_toRaw_Exp_(float value, float outMin, float outMax,
                                 bool invert = false) {
  if (outMax == outMin) return 0;
  float t = (value - outMin) / (outMax - outMin);  // target shaped 0..1
  t = math::clamp01(t);
  if (invert) t = 1.f - t;
  // forward used y = pow(x, 1.6); so inverse is x = pow(y, 1/1.6)
  constexpr float kInvGamma = 1.f / 1.6f;
  float x = powf(t, kInvGamma);
  x = math::clamp01(x);
  return int(lroundf(x * kAdcMaxCode));
}

// inverse of RsRate mapping (ms linear mapped then ms->rate)
// forward: rate = 1000 / (SR * ms), with ms in [msMin..msMax]
static inline int inv_toRaw_Rate_(float rate, float msMin, float msMax,
                                  float SR) {
  if (rate <= 0.f || msMax == msMin) return 0;
  float ms = 1000.f / (SR * rate);
  float t = (ms - msMin) / (msMax - msMin);
  t = math::clamp01(t);
  return int(lroundf(t * kAdcMaxCode));
}

void UI::seedRawFromCfg_() {
  // For each tab/page/control that has a cfgValue, read current value and
  // compute a 0..4095 "raw" that your existing mapping will reproduce.
  for (uint8_t tab = 0; tab < kNumTabs; ++tab) {
    if (!ucfg_.rotaryTabs[tab].enabled) continue;

    const uint8_t pageCount =
        (ucfg_.tabPageCount[tab] == 0) ? 1 : ucfg_.tabPageCount[tab];
    for (uint8_t pg = 0; pg < pageCount && pg < ParameterTab::MAX_PAGE; ++pg) {
      auto& page = ucfg_.rotaryTabs[tab].pages[pg];
      if (!page.enabled) continue;

      for (int i = 0; i < ParameterPage::ROTARY_COUNT; ++i) {
        const RotaryInputSpec& spec = page.rotary[i];
        if (!spec.cfgValue) continue;

        int seededRaw = 0;

        switch (spec.response) {
          case RotaryInputSpec::RsLin: {
            float v = *reinterpret_cast<float*>(spec.cfgValue);
            seededRaw = inv_toRaw_Lin_(v, spec.outMin, spec.outMax);
          } break;

          case RotaryInputSpec::RsExp: {
            float v = *reinterpret_cast<float*>(spec.cfgValue);
            seededRaw =
                inv_toRaw_Exp_(v, spec.outMin, spec.outMax, /*invert=*/false);
          } break;

          case RotaryInputSpec::RsRate: {
            float rate =
                *reinterpret_cast<float*>(spec.cfgValue);  // current rate
            seededRaw =
                inv_toRaw_Rate_(rate, spec.outMin, spec.outMax, CalcisTR::SR);
          } break;

          case RotaryInputSpec::RsInt: {
            int v = *reinterpret_cast<int*>(spec.cfgValue);
            // treat as linear in [outMin..outMax]
            seededRaw =
                inv_toRaw_Lin_(static_cast<float>(v), spec.outMin, spec.outMax);
          } break;

          case RotaryInputSpec::RsBool: {
            bool v = *reinterpret_cast<bool*>(spec.cfgValue);
            seededRaw = v ? kAdcMaxCode : 0;
          } break;

          case RotaryInputSpec::RsGCut:
          case RotaryInputSpec::RsKDamp:
          case RotaryInputSpec::RsMorph:
          case RotaryInputSpec::RsDrive:
          default: {
            // Without a public getter for normalized 0..1 from filter cfg,
            // seed to middle to avoid surprises. If you expose getters later,
            // we can compute exact raw here.
            seededRaw = kAdcMaxCode / 2;
          } break;
        }

        tabs_[tab].pages[pg].rawPos[i] = seededRaw;
      }  // pot
    }  // page
  }  // tab
}

}  // namespace zlkm