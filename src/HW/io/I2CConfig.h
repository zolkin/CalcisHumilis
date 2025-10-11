#pragma once
#include <Wire.h>

#include "platform/pins.h"
#include "platform/platform.h"

namespace zlkm::hw::io {

// I2C configuration for the expander device created inside the class
struct I2cCfg {
  using Pins = ::zlkm::platform::Pins;
  uint8_t address = 0x20;          // MCP23017 default (A2..A0 = 000)
  TwoWire* wire = &Wire;           // I2C bus pointer
  uint32_t clockHz = 0;            // optional: setClock() if nonzero
  int16_t i2cSDA = Pins::I2C_SDA;  // optional: explicit SDA pin
  int16_t i2cSCL = Pins::I2C_SCL;  // optional: explicit SCL pin
};

}  // namespace zlkm::hw::io