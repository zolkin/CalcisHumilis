#pragma once
#include <Wire.h>

#include <cstdint>

#include "platform/platform.h"

namespace zlkm::hw::io {

// I2C configuration for the expander device created inside the class
struct I2cCfg {
  uint8_t address;   // MCP23017 default (A2..A0 = 000)
  TwoWire* wire;     // I2C bus pointer
  uint32_t clockHz;  // optional: setClock() if nonzero

  PinId i2cSDA;  // optional: explicit SDA pin (set by board code)
  PinId i2cSCL;  // optional: explicit SCL pin (set by board code)
};

}  // namespace zlkm::hw::io