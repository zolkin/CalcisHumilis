#include "UI.h"

#include <ArduinoLog.h>
#include <math.h>

static constexpr int kAdcMaxCode = 4095;  // RP2040 12-bit

UI::UI(UIConfig* cfg, CalcisHumilis* kick)
    : ucfg_(cfg),
      trigBtn_(cfg->trigPin, /*activeLow=*/false, /*pullupActive=*/false),
      kick_(kick) {
  initAdc_();

  // ADS1015 example; if you use ADS1115, just swap the type of ads_ in UI.h
  // and keep everything else identical.

  trigBtn_.attachPress(UI::onPress_, this);
  trigBtn_.setDebounceMs(3);
}

void UI::attachADS() {
  Wire.setSDA(4);  // GP4
  Wire.setSCL(5);  // GP5
  Wire.begin();    // start I2C0 at default 100 kHz (or
  Wire.setClock(400000);

  bool needAds = false;
  bool adsOk_ = false;
  for (const auto& ps : ucfg_->pots)
    if (ps.useAds) {
      needAds = true;
      break;
    }
  if (needAds) {
    // ADS1015 example; if you use ADS1115, just swap the type of ads_ in UI.h
    // and keep everything else identical.
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

void UI::update() {
  trigBtn_.tick();

  const uint32_t now = millis();
  if (now - tPrev_ < ucfg_->pollMs) return;
  tPrev_ = now;

  bool changed = false;
  for (int i = 0; i < PotSpec::Count; ++i) {
    changed |= processPot_(i);
  }

  if (changed && kick_) {
    kick_->setConfig(*ucfg_->pCfg);
  }
}

void UI::initAdc_() {
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_RASPBERRY_PI_PICO) || \
    defined(ARDUINO_ARCH_MBED_RP2040)
  analogReadResolution(12);  // 0..4095
#endif
}

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

void UI::initPots_(bool adsOk) {
  // Build unified pot readers
  for (size_t i = 0; i < pots_.size(); ++i) {
    const PotSpec& ps = ucfg_->pots[i];
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

bool UI::processPot_(int id) {
  const size_t idx = static_cast<size_t>(id);
  const PotSpec& spec = ucfg_->pots[idx];

  if (!pots_[idx].update()) return false;
  int raw = pots_[idx].value();  // 0..4095

  float val = 0.0;
  switch (spec.response) {
    case PotSpec::RsLin:
      val = mapLin_(raw, kAdcMaxCode, spec.outMin, spec.outMax, spec.invert);
      ;
      break;
    case PotSpec::RsExp:
      val = mapExp_(raw, kAdcMaxCode, spec.outMin, spec.outMax, spec.invert);
      break;
    default:
      break;
  }

  if (spec.step > 0.0f) {
    val = roundf(val / spec.step) * spec.step;
  }

  return spec.setCfgValue(val) && spec.reconfig;
}

void UI::onPress_(void* param) {
  UI* self = reinterpret_cast<UI*>(param);
  if (self->kick_) {
    self->kick_->trigger();
  }
}