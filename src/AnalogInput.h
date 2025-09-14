#pragma once
#include <Adafruit_ADS1X15.h>  // base for ADS1015/ADS1115
#include <Arduino.h>
#include <ResponsiveAnalogRead.h>

class AnalogInput {
 public:
  enum class Kind : uint8_t { Internal, ADS1x15 };

  AnalogInput() = default;
  ~AnalogInput() { delete rar_; }

  // --- Internal ADC (ResponsiveAnalogRead) ---
  void attachInternal(uint8_t pin, bool sleep = true) {
    kind_ = Kind::Internal;
    delete rar_;
    rar_ = new ResponsiveAnalogRead(pin, sleep);
  }
  void setRARParams(int maxCode, float snapMultiplier, float activityThresh,
                    bool edgeSnap = true) {
    maxCode_ = maxCode;
    if (!rar_) return;
    rar_->setAnalogResolution(maxCode_);
    rar_->setSnapMultiplier(snapMultiplier);
    rar_->setActivityThreshold(activityThresh);
    if (edgeSnap) rar_->enableEdgeSnap();
  }

  // --- ADS1015 / ADS1115 ---
  void attachADS(Adafruit_ADS1X15* ads, uint8_t channel,
                 float vrefVolts = 3.3f) {
    kind_ = Kind::ADS1x15;
    ads_ = ads;
    adsCh_ = channel;
    vref_ = vrefVolts;
  }
  void setADSParams(int maxCode, float emaAlpha = 0.12f) {
    maxCode_ = maxCode;
    emaAlpha_ = emaAlpha;
    ema_ = NAN;  // re-init EMA next update
  }

  // --- Common API ---
  void setMaxCode(int maxCode) { maxCode_ = maxCode; }
  int maxCode() const { return maxCode_; }

  // Call often; returns true if a new (meaningful) value is available
  bool update() {
    switch (kind_) {
      case Kind::Internal: {
        if (!rar_) return false;
        rar_->update();
        if (!rar_->hasChanged()) return false;
        const int v = rar_->getValue();
        if (v != last_) {
          last_ = v;
          return true;
        }
        return false;
      }
      case Kind::ADS1x15: {
        if (!ads_) return false;
        int16_t raw = ads_->readADC_SingleEnded(adsCh_);
        float volts = ads_->computeVolts(raw);
        if (volts < 0.0f) volts = 0.0f;
        if (volts > vref_) volts = vref_;

        const float code = volts * (static_cast<float>(maxCode_) / vref_);

        // EMA smoothing (first sample seeds the EMA)
        if (!isfinite(ema_)) ema_ = code;
        ema_ += emaAlpha_ * (code - ema_);
        const int v = static_cast<int>(lroundf(ema_));

        if (v != last_) {
          last_ = v;
          return true;
        }
        return false;
      }
    }
    return false;
  }

  // Last delivered value (0..maxCode)
  int value() const { return last_; }

 private:
  Kind kind_ = Kind::Internal;

  // Internal ADC
  ResponsiveAnalogRead* rar_ = nullptr;

  // ADS
  Adafruit_ADS1X15* ads_ = nullptr;
  uint8_t adsCh_ = 0;
  float vref_ = 3.3f;
  float emaAlpha_ = 0.12f;
  float ema_ = NAN;

  // Common
  int maxCode_ = 4095;
  int last_ = 0;
};