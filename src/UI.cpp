#include "UI.h"

#include <ArduinoLog.h>
#include <math.h>

static constexpr int kAdcMaxCode = 4095;  // RP2040 12-bit

UI::UI(UIConfig* cfg, CalcisHumilis* kick)
    : ucfg_(cfg),
      trigBtn_(cfg->trigPin, /*activeLow=*/false, /*pullupActive=*/false),
      kick_(kick) {
  for (int i = 0; i < PotSpec::Count; ++i) {
    pots_[i] = ResponsiveAnalogRead(cfg->pots[i].pin, true);
  }
  initAdc_();
  initPots_();

  trigBtn_.attachPress(UI::onPress_, this);
  trigBtn_.setDebounceMs(3);
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

void UI::initAdc_() {
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_RASPBERRY_PI_PICO) || \
    defined(ARDUINO_ARCH_MBED_RP2040)
  analogReadResolution(12);  // 0..4095
#endif
}

void UI::initPots_() {
  for (auto& p : pots_) {
    p.setAnalogResolution(kAdcMaxCode);
    p.setSnapMultiplier(ucfg_->snapMultiplier);
    p.setActivityThreshold(ucfg_->activityThresh);
    p.enableEdgeSnap();
  }
}

bool UI::processPot_(int id) {
  const size_t idx = static_cast<size_t>(id);
  auto& rar = pots_[idx];
  const PotSpec& spec = ucfg_->pots[idx];

  rar.update();
  if (!rar.hasChanged()) return false;

  const int raw = rar.getValue();  // smoothed 0..4095
  float val =
      mapLinear_(raw, kAdcMaxCode, spec.outMin, spec.outMax, spec.invert);

  // quantize to reduce chatter
  if (spec.step > 0.0f) {
    val = roundf(val / spec.step) * spec.step;
  }

  return spec.setCfgValue(val);
}

void UI::onPress_(void* param) {
  UI* self = reinterpret_cast<UI*>(param);
  if (self->kick_) {
    self->kick_->trigger();
  }
}