#pragma once

#include <Adafruit_MCP23X17.h>
#include <ArduinoLog.h>
#include <Wire.h>

#include <array>
#include <bitset>

#include "Types.h"

namespace zlkm::hw::io {

template <typename DeviceT, int N>
class McpPins {
 public:
  static_assert(N > 0 && N <= 16, "McpPins<N>: N must be 1..16");

  using Modes = std::array<PinMode, N>;
  using Bits = std::bitset<N>;

  // --- configuration/ownership semantics ---
  McpPins(const McpPins&) = delete;
  McpPins& operator=(const McpPins&) = delete;
  McpPins(McpPins&&) = default;
  McpPins& operator=(McpPins&&) = default;

  explicit McpPins(I2cCfg i2c, PinMode default_mode = PinMode::Input)
      : dev_{}, i2c_(i2c) {
    initDevice_();
    setMode(default_mode);
  }

  McpPins(const Modes& modes, I2cCfg i2c) : dev_{}, i2c_(i2c) {
    initDevice_();
    for (int i = 0; i < N; ++i) applyPinMode((uint8_t)i, modes[i]);
  }

  inline bool isOk() const { return ok_; }

  inline void setMode(PinMode m) {
    for (int i = 0; i < N; ++i) applyPinMode((uint8_t)i, m);
  }

  inline void setPinMode(int i, PinMode m) { applyPinMode((uint8_t)i, m); }

  inline void writeAll(Bits b) {
    uint16_t v = (uint16_t)b.to_ulong();
    dev_.writeGPIO((uint8_t)(v & 0xFF), 0);  // GPIOA
    dev_.writeGPIO((uint8_t)(v >> 8), 1);    // GPIOB
  }

  inline void writePin(int i, bool high) {
    dev_.digitalWrite((uint8_t)i, high ? 1 : 0);
  }

  inline Bits readAll() const {
    uint8_t a = dev_.readGPIO(0);  // GPIOA
    uint8_t b = dev_.readGPIO(1);  // GPIOB
    return Bits(((uint16_t)b << 8) | a);
  }

  inline bool readPin(int i) const { return dev_.digitalRead((uint8_t)i) != 0; }

  // accessors
  inline uint8_t rawPin(int i) const { return (uint8_t)i; }
  inline DeviceT& device() { return dev_; }
  inline const DeviceT& device() const { return dev_; }
  inline I2cCfg i2c() const { return i2c_; }

 private:
  mutable DeviceT dev_;
  I2cCfg i2c_;
  bool ok_{false};

  inline void initDevice_() {
    if (i2c_.wire) {
      if (i2c_.i2cSDA >= 0 && i2c_.i2cSCL >= 0) {
        i2c_.wire->setSDA(i2c_.i2cSDA);
        i2c_.wire->setSCL(i2c_.i2cSCL);
      }
      i2c_.wire->begin();
      if (i2c_.clockHz) i2c_.wire->setClock(i2c_.clockHz);
    }

    ok_ = dev_.begin_I2C(i2c_.address, i2c_.wire ? i2c_.wire : &Wire);
    if (ok_) {
      Log.info(F("[MCP] Initialized @0x%02X on %s (N=%d)" CR), i2c_.address,
               (i2c_.wire == &Wire ? "Wire" : "WireX"), N);
    } else {
      Log.error(F("[MCP] begin_I2C failed @0x%02X (check "
                  "wiring/address/pull-ups)" CR),
                i2c_.address);
    }
  }

  inline void applyPinMode(uint8_t p, PinMode m) {
    static const uint8_t map[] = {INPUT, INPUT_PULLUP, OUTPUT};
    dev_.pinMode(p, map[(int)m]);
  }
};

// typedef for the full 16-pin expander
using Mcp23017Pins = McpPins<Adafruit_MCP23X17, 16>;

}  // namespace zlkm::hw::io