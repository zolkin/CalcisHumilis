#pragma once

namespace zlkm::platform {

// Generic board descriptor tying a PinSource device and the PinDefs constants
// PinSource: a class implementing the pin device API (readPin, readPins, etc.)
// PinDefsT: a struct with static constexpr pin constants (platform::Pins)
// PinT:      the pin type exposed by the board (PinSource::PinT)
template <typename PinSourceT, typename PinDefsT>
struct BoardT {
  using PinSource = PinSourceT;
  using PinDefs = PinDefsT;
  using PinId = typename PinSource::PinIdT;
};

}  // namespace zlkm::platform
