#include <ArduinoLog.h>
#include <math.h>

#include "UI.h"

namespace zlkm {

static constexpr int kAdcMaxCode = 4095;  // RP2040 12-bit

// ---------- ctor ----------
UI::UI(Calcis::Cfg* cfg, Calcis::Feedback* fb)
    : ucfg_(cfg),
      fb_(fb),
      trigBtn_(ucfg_.trigPin, /*activeLow=*/true, /*pullupActive=*/true),
      adsPinReader_(ucfg_.readerCfg),
      multiInput_(adsPinReader_, ucfg_.potsCfg) {
  initTabs_();  // NEW

  trigBtn_.attachPress(UI::onPress_, this);
  trigBtn_.setDebounceMs(5);

  Log.notice(F("[UI] ready" CR));
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
void UI::processPot_(int id) {
  const PotSpec& spec = getPotSpec(id);
  if (spec.cfgValue == nullptr) {
    return;
  }

  int raw = multiInput_.value(id);  // 0..4095

  Log.infoln("POT[%d] value = %d", id, raw);

  switch (spec.response) {
    case PotSpec::RsLin:
      spec.setCfgValue(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      break;
    case PotSpec::RsExp:
      spec.setCfgValue(mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      break;
    case PotSpec::RsRate:
      spec.setCfgValue(
          potToRate(raw, kAdcMaxCode, spec.outMin, spec.outMax, CalcisTR::SR));
      break;
    case PotSpec::RsGCut:
      filterParams_.setCutoff01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case PotSpec::RsKDamp:
      filterParams_.setRes01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case PotSpec::RsMorph:
      filterParams_.setMorph01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;

    case PotSpec::RsDrive:
      filterParams_.setDrive01(
          mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      spec.setCfgValue(filterParams_.cfg);
      break;
    case PotSpec::RsInt: {
      int val = int(mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax));
      Log.infoln("Setting RsInt to %d", val);
      spec.setCfgValue(val);
    } break;
    case PotSpec::RsBool: {
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
  for (int i = 0; i < kNumTabs; ++i) currentPage_[i] = 0;
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
  if (ucfg_.potTabs[currentTab_].enabled && multiInput_.update()) {
    for (int i = 0; i < ParameterPage::POT_COUNT; ++i) {
      if (multiInput_.valueChanged(i)) {
        processPot_(i);
      }
    }
  }

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
  if (currentPage_[currentTab_] >= pc) currentPage_[currentTab_] = 0;
  updateLeds_();
}

void UI::advancePage_() {
  uint8_t pc = ucfg_.tabPageCount[currentTab_];
  if (pc <= 1) {
    blinkLed_(currentTab_, 1);
    return;
  }
  currentPage_[currentTab_] = (currentPage_[currentTab_] + 1) % pc;
  blinkLed_(currentTab_, currentPage_[currentTab_] + 1);
  updateLeds_();
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

}  // namespace zlkm