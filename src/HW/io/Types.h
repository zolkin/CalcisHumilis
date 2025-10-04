#pragma once
#include <stdint.h>

namespace zlkm::hw::io {

enum class PinMode : uint8_t { Input, InputPullUp, Output };

}  // namespace zlkm::hw::io