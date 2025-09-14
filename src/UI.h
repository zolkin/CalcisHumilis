#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <ResponsiveAnalogRead.h>

#include <array>

#include "CalcisHumilis.h"

// Identify each pot (extend as you add more)

// Mapping + pin setup for a single pot
struct PotSpec {
  enum PotId { AmpDecay = 0, PitchDecay, Pan, Count };

  uint8_t pin = A0;
  float outMin = 0.0f;
  float outMax = 1.0f;
  float step = 0.5f;    // quantization step in output units
  bool invert = false;  // set true if wired “backwards”
  float* cfgValue = nullptr;

  inline bool setCfgValue(float value) const {
    if (cfgValue == nullptr || fabsf(value - *cfgValue) < step) {
      return false;
    }
    *cfgValue = value;
    return true;
  }
};

// Overall UI config (no code duplication)
struct UIConfig {
  UIConfig(CalcisConfig* pCfg_) : pCfg(pCfg_) {
    pots[PotSpec::AmpDecay].cfgValue = &pCfg->ampMs;
    pots[PotSpec::PitchDecay].cfgValue = &pCfg->pitchMs;
    pots[PotSpec::Pan].cfgValue = &pCfg->pan;
  }

  uint8_t trigPin = 6;
  uint16_t pollMs = 5;           // ~200 Hz
  float snapMultiplier = 0.01f;  // ResponsiveAnalogRead smoothing “stickiness”
  float activityThresh = 1.0f;   // counts (default in RAR ≈ 4)

  CalcisConfig* pCfg;

  // Pots (linear mapping)
  PotSpec pots[static_cast<size_t>(PotSpec::Count)] = {
      /* AmpDecay  */ {A0, 20.0f, 2000.0f, .1f, false, nullptr},
      /* PitchDecay*/ {A1, 2.0f, 800.0f, .1f, false, nullptr},
      /* Pan*/ {A2, -1.f, 1.f, .001f, false, nullptr},
  };
};

class UI {
 public:
  explicit UI(UIConfig* cfg, CalcisHumilis* kick);

  // Attach to your synth + config (references kept)

  // Call each loop()
  void update();

  // Optional: wait until a serial terminal is open (USB-CDC boards)
  static void waitForSerial(unsigned long timeoutMs = 3000);

 private:
  UIConfig* ucfg_;
  CalcisHumilis* kick_ = nullptr;

  OneButton trigBtn_;
  std::array<ResponsiveAnalogRead, static_cast<size_t>(PotSpec::Count)> pots_;
  uint32_t tPrev_ = 0;

  void initAdc_();
  void initPots_();
  static void onPress_(void* param);

  // Linear map helper (shared by all pots)
  static inline float mapLinear_(int raw, int rawMax, float outMin,
                                 float outMax, bool invert) {
    if (raw < 0) raw = 0;
    if (raw > rawMax) raw = rawMax;
    float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
    if (invert) x = 1.0f - x;
    return outMin + x * (outMax - outMin);
  }

  // Process one pot by index; returns true if cfg changed
  bool processPot_(int id);
};