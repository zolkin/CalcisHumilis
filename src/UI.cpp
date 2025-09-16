#include "UI.h"

#include <ArduinoLog.h>
#include <math.h>

static constexpr int kAdcMaxCode = 4095;  // RP2040 12-bit

// ---------- ctor ----------
UI::UI(UIConfig* cfg, CalcisHumilis* kick)
    : ucfg_(cfg),
      trigBtn_(cfg->trigPin, /*activeLow=*/true, /*pullupActive=*/true),
      kick_(kick) {
  initAdc_();
  initTabs_();  // NEW

  trigBtn_.attachPress(UI::onPress_, this);
  trigBtn_.setDebounceMs(3);
}

// ---------- ADS attach ----------
void UI::attachADS() {
  Wire.setSDA(4);  // GP4
  Wire.setSCL(5);  // GP5
  Wire.begin();
  Wire.setClock(400000);

  bool needAds = false;
  bool adsOk_ = false;
  for (const auto& ps : ucfg_->potSources)
    if (ps.useAds) {
      needAds = true;
      break;
    }

  if (needAds) {
    adsOk_ = ads_.begin(0x48);
    if (adsOk_) {
      ads_.setGain(GAIN_ONE);
      ads_.setDataRate(RATE_ADS1015_3300SPS);
      Log.notice(F("[UI] ADS1015 ready (±4.096V, 3300 SPS)" CR));
    } else {
      Log.error(F("[UI] ADS1x15 not found at 0x48" CR));
    }
  }
  initPots_(adsOk_);
}

// ---------- init ADC ----------
void UI::initAdc_() {
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_RASPBERRY_PI_PICO) || \
    defined(ARDUINO_ARCH_MBED_RP2040)
  analogReadResolution(12);  // 0..4095
#endif
}

// ---------- wait for serial ----------
void UI::waitForSerial(unsigned long timeoutMs) {
#if defined(USBCON) || defined(SERIAL_PORT_USBVIRTUAL)
  const unsigned long t0 = millis();
  while (!(Serial && Serial.dtr()) && (millis() - t0) < timeoutMs) {
    delay(10);
  }
#else
  (void)timeoutMs;
#endif
}

// ---------- init pots ----------
void UI::initPots_(bool adsOk) {
  // Build unified pot readers
  int pot_idx = 0;
  for (int i = 0; i < PotSource::Count; ++i) {
    const auto& ps = ucfg_->potSources[i];
    if (ps.useAds && adsOk) {
      pots_[i].attachADS(&ads_, ps.adsChannel, 3.3f);
      pots_[i].setADSParams(kAdcMaxCode, /*emaAlpha=*/0.12f);
    } else {
      pots_[i].attachInternal(ps.pin, /*sleep=*/true);
      pots_[i].setRARParams(kAdcMaxCode, ucfg_->snapMultiplier,
                            ucfg_->activityThresh, /*edgeSnap=*/true);
    }
  }
}

// ---------- map helpers ----------
inline float UI::mapLin_(int raw, int rawMax, float outMin, float outMax,
                         bool invert) {
  if (raw < 0) raw = 0;
  if (raw > rawMax) raw = rawMax;
  float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
  if (invert) x = 1.0f - x;
  return outMin + x * (outMax - outMin);
}

inline float UI::mapExp_(int raw, int rawMax, float outMin, float outMax,
                         bool invert) {
  if (raw < 0) raw = 0;
  if (raw > rawMax) raw = rawMax;

  float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
  if (invert) x = 1.0f - x;

  constexpr float kGamma = 1.6f;

  if (outMin < 0.0f && outMax <= 0.0f) {
    const float dB = outMin + x * (outMax - outMin);
    return powf(10.0f, dB / 20.0f);  // linear gain 0..1
  } else {
    const float yNorm = powf(x, kGamma);  // 0..1 (shaped)
    return outMin + (outMax - outMin) * yNorm;
  }
}

// ---------- process one pot ----------
bool UI::processPot_(int id) {
  const PotSpec& spec = getPotSpec(id);
  const PotSource& source = ucfg_->potSources[id];

  if (!pots_[id].update()) return false;
  int raw = pots_[id].value();  // 0..4095

  float val = 0.0f;
  switch (spec.response) {
    case PotSpec::RsLin:
      val = mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax, source.invert);
      break;
    case PotSpec::RsExp:
      val = mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax, source.invert);
      break;
    default:
      break;
  }

  if (spec.step > 0.0f) {
    val = roundf(val / spec.step) * spec.step;
  }

  return spec.setCfgValue(val) && spec.reconfig;
}

// ===================== NEW: Tabs & Pages =====================

// Setup pins and internal state
// ---------- init tabs (OneButton) ----------
void UI::initTabs_() {
  // LEDs
  for (int i = 0; i < kNumTabs; ++i) {
    pinMode(ucfg_->tabLedPins[i], OUTPUT);
    digitalWrite(ucfg_->tabLedPins[i], LOW);
  }

  // OneButton for each tab button (INPUT_PULLUP, activeLow=true)
  for (int i = 0; i < kNumTabs; ++i) {
    tabBtns_[i] = OneButton(ucfg_->tabBtnPins[i], /*activeLow=*/false,
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
  if (now - tPrev_ < ucfg_->pollMs) return;
  tPrev_ = now;

  bool changed = false;

  // Only enabled tabs processes pots (unchanged behavior)
  if (ucfg_->potTabs[currentTab_].enabled) {
    for (int i = 0; i < PotSource::Count; ++i) {
      changed |= processPot_(i);
    }
  }

  if (changed && kick_) {
    kick_->setConfig(*ucfg_->pCfg);
  }
}

// ---------- trigger press (unchanged) ----------
void UI::onPress_(void* param) {
  UI* self = reinterpret_cast<UI*>(param);
  if (self->kick_) self->kick_->trigger();
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
  uint8_t pc = ucfg_->tabPageCount[currentTab_];
  if (pc == 0) pc = 1;
  if (currentPage_[currentTab_] >= pc) currentPage_[currentTab_] = 0;
  updateLeds_();
}

void UI::advancePage_() {
  uint8_t pc = ucfg_->tabPageCount[currentTab_];
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
    digitalWrite(ucfg_->tabLedPins[i], (i == currentTab_) ? HIGH : LOW);
  }
}

void UI::blinkLed_(uint8_t tab, uint8_t count) {
  const uint8_t pin = ucfg_->tabLedPins[tab];
  digitalWrite(pin, LOW);
  delay(40);
  for (uint8_t n = 0; n < count; ++n) {
    digitalWrite(pin, HIGH);
    delay(80);
    digitalWrite(pin, LOW);
    delay(80);
  }
  if (tab == currentTab_) digitalWrite(pin, HIGH);
}