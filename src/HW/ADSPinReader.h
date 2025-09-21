#pragma once
#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <Wire.h>

#include <array>

/* ------------------------------ ADSPinReader ------------------------------ */
/* Owns + initializes an ADS device in the constructor and provides          */
/* readVolts(). Device is typically Adafruit_ADS1015 or Adafruit_ADS1115.    */

namespace zlkm {

namespace hw {

template <class Device>
class ADSPinReader {
 public:
  // ADS1x15 has 4 single-ended channels (A0..A3)
  static constexpr int CHAN_COUNT = 4;

  struct Cfg {
    // I2C
    uint8_t i2cSDA = 4;
    uint8_t i2cSCL = 5;
    uint32_t i2cHz = 400000;

    // ADS
    uint8_t i2cAddr = 0x48;
    adsGain_t gain = GAIN_ONE;                 // ±4.096 V FS
    uint16_t dataRate = RATE_ADS1015_3300SPS;  // use ADS1115 enum if needed

    // Clamp/reference for readVolts()
    float vrefVolts = 4.096f;  // typically matches gain
  };

  explicit ADSPinReader(const Cfg& cfg) : cfg_(cfg) {
    // Bring up I²C + ADS *here* (constructor), per your request.
    Wire.setSDA(cfg_.i2cSDA);
    Wire.setSCL(cfg_.i2cSCL);
    Wire.begin();
    Wire.setClock(cfg_.i2cHz);

    ok_ = dev_.begin(cfg_.i2cAddr);
    if (ok_) {
      dev_.setGain(cfg_.gain);
      dev_.setDataRate(cfg_.dataRate);
    }
  }

  bool ok() const { return ok_; }

  // Read single-ended channel and return volts clamped to [0..vrefVolts].
  float readVolts(uint8_t ch) {
    const int16_t raw = dev_.readADC_SingleEnded(ch);
    float v = dev_.computeVolts(raw);
    if (v < 0.f) v = 0.f;
    if (v > cfg_.vrefVolts) v = cfg_.vrefVolts;
    return v;
  }

  float vrefVolts() const { return cfg_.vrefVolts; }
  Device& device() { return dev_; }
  const Device& device() const { return dev_; }

 private:
  Device dev_;
  Cfg cfg_;
  bool ok_ = false;
};

using ADS1015Reader = ADSPinReader<Adafruit_ADS1015>;

}  // namespace hw

}  // namespace zlkm