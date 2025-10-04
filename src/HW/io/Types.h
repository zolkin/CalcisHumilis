#pragma once
#include <stdint.h>

namespace zlkm::hw::io {

enum class PinMode : uint8_t { Input, InputPullUp, Output };

// I2C configuration for the expander device created inside the class
struct I2cCfg {
  uint8_t address = 0x20;  // MCP23017 default (A2..A0 = 000)
  TwoWire* wire = &Wire;   // I2C bus pointer
  uint32_t clockHz = 0;    // optional: setClock() if nonzero
  int16_t i2cSDA = 20;     // optional: explicit SDA pin
  int16_t i2cSCL = 21;     // optional: explicit SCL pin
};

}  // namespace zlkm::hw::io