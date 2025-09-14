#pragma once
#include <AnalogInput.h>
#include <Arduino.h>
#include <OneButton.h>

#include <array>

#include "CalcisHumilis.h"

// Identify each pot (extend as you add more)

// Mapping + pin setup for a single pot
struct PotSpec {
  enum PotId { AmpDecay = 0, PitchDecay, Pan, Volume, Count };
  enum PotResponse { RsLin, RsExp };

  // Source selection
  bool useAds = false;     // false = internal ADC (RAR), true = ADS1x15
  uint8_t adsChannel = 0;  // 0..3 for ADS1x15 when useAds == true

  // For internal ADC
  uint8_t pin = A0;

  // Mapping
  float outMin = 0.0f;
  float outMax = 1.0f;
  float step = 0.5f;    // quantization step in output units
  bool invert = false;  // set true if wired “backwards”
  bool reconfig = true;
  PotResponse response = RsLin;

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
    pots[PotSpec::Volume].cfgValue = &pCfg->outGain;
  }

  uint8_t trigPin = 6;
  uint16_t pollMs = 5;           // ~200 Hz
  float snapMultiplier = 0.01f;  // ResponsiveAnalogRead smoothing “stickiness”
  float activityThresh = 1.0f;   // counts (default in RAR ≈ 4)

  CalcisConfig* pCfg;

  // Pots (linear mapping)
  PotSpec pots[static_cast<size_t>(PotSpec::Count)] = {
      /* ADS,CH,PIN, MIN , MAX   ,STEP, INV  , RECONF,  RESPONSE */
      {false, 0, A0, 20.f, 2000.f, .1f, false, true, PotSpec::RsLin}, /* Dec*/
      {false, 0, A1, 2.f, 800.f, .1f, false, true, PotSpec::RsLin},   /* PDec*/
      {false, 0, A2, -1.f, 1.f, .001f, false, false, PotSpec::RsLin}, /* Pan */
      {true, 0, 0, 0.f, 1.f, .01f, false, false, PotSpec::RsExp},     /* Gain */
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

  void attachADS();

 private:
  UIConfig* ucfg_;
  CalcisHumilis* kick_ = nullptr;

  OneButton trigBtn_;
  std::array<AnalogInput, static_cast<size_t>(PotSpec::Count)> pots_;
  uint32_t tPrev_ = 0;

  void initAdc_();
  void initPots_(bool adsOk);
  static void onPress_(void* param);

  Adafruit_ADS1015 ads_;
  // (optional) add to UI as a member:
  float adsVref_ = 3.3f;  // your pot runs off 3V3

  // Linear map helper (shared by all pots)
  static inline float mapLin_(int raw, int rawMax, float outMin, float outMax,
                              bool invert) {
    if (raw < 0) raw = 0;
    if (raw > rawMax) raw = rawMax;
    float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
    if (invert) x = 1.0f - x;
    return outMin + x * (outMax - outMin);
  }

  // Exponential/log mapping with the SAME signature as mapLinear_.
  // - If outMin < 0 (e.g., -48 .. 0) → interpret as dB range and return linear
  // gain.
  // - Else → power curve in linear domain for "more control near start".
  static inline float mapExp_(int raw, int rawMax, float outMin, float outMax,
                              bool invert) {
    if (raw < 0) raw = 0;
    if (raw > rawMax) raw = rawMax;

    float x = static_cast<float>(raw) / static_cast<float>(rawMax);  // 0..1
    if (invert) x = 1.0f - x;

    // Tweak curvature here ( >1.0 = more resolution near start )
    constexpr float kGamma = 1.6f;

    if (outMin < 0.0f && outMax <= 0.0f) {
      // Treat as dB mapping: outMin..outMax are dB (e.g., -48..0)
      const float dB = outMin + x * (outMax - outMin);  // [-48..0]
      return powf(10.0f, dB / 20.0f);                   // → linear gain 0..1
    } else {
      // Power-curve in linear domain between outMin..outMax
      const float yNorm = powf(x, kGamma);  // 0..1 (shaped)
      return outMin + (outMax - outMin) * yNorm;
    }
  }

  // Process one pot by index; returns true if cfg changed
  bool processPot_(int id);
};