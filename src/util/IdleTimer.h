#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace zlkm::util {

class IdleTimer {
 public:
  explicit IdleTimer(uint32_t timeout_ms) : timeoutMs_(timeout_ms) { last_ = millis(); }
  inline void noteActivity() { last_ = millis(); }
  inline bool isIdle(uint32_t now) const { return (now - last_) >= timeoutMs_; }

 private:
  uint32_t timeoutMs_{60000};
  uint32_t last_{0};
};

}  // namespace zlkm::util