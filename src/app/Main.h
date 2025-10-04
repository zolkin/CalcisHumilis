#pragma once
#include <Arduino.h>
#include <ArduinoLog.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/sync.h>

#include <atomic>

#include "util/spin_lock.h"

namespace zlkm::app {
/**
 * Dual-core harness for RP2350 (Pico 2).
 *
 * Core 1: AudioSource (tight loop; treat as higher priority)
 * Core 0: UI (yields/sleeps; lower impact)
 */
template <class AudioSource, class UI>
class MainApp {
 public:
  static void core0_start(const char* name) {
    appName_ = name;
    Serial.begin(115200);
    waitForSerial(10000);  // keep your original behavior
    Log.begin(LOG_LEVEL_TRACE, &Serial);
    get();  // initialize the app for the first time
    delay(100);
    Log.infoln("Application %s started on core0", name);
    c0Started_ = true;
    while (!c1Started_) {
      delay(1);
    }
  }

  static void core1_start() {
    while (!c0Started_) {
      delay(1);
    }
    Log.infoln("Application %s started on core1", appName_);
    c1Started_ = true;
  }

  // -------- Core 1: Audio --------
  static void core1_loop() {
    get().snapAudioCfg();
    get().audio_.update();
    get().publishUIFeedback();
  }

  // -------- Core 0: UI --------
  static void core0_loop() {
    get().snapUIFeedback();
    get().ui_.update();
    get().publishAudioCfg();
    sleep_ms(2);
  }

 private:
  static void waitForSerial(unsigned long timeoutMs) {
    const unsigned long t0 = millis();
    while (!(Serial && Serial.dtr()) && (millis() - t0) < timeoutMs) {
      delay(10);
    }
  }

  void snapAudioCfg() {
    sl_guard guard{cfgSL_};
    audioCfg_ = sharedCfg_;
  }

  void publishAudioCfg() {
    sl_guard guard{cfgSL_};
    sharedCfg_ = uiAudioCfg_;
  }

  void publishUIFeedback() {
    sl_guard guard{fbSL_};
    sharedFb_ = audioUiFb_;
  }

  void snapUIFeedback() {
    sl_guard guard{fbSL_};
    uiFb_ = sharedFb_;
  }

  static MainApp& get() {
    static MainApp<AudioSource, UI> inst;
    return inst;
  }

  MainApp() = default;

  using Cfg = typename AudioSource::Cfg;
  using Feedback = typename AudioSource::Feedback;

  spin_lock cfgSL_;
  Cfg sharedCfg_{};
  Cfg audioCfg_{};
  Cfg uiAudioCfg_{};

  spin_lock fbSL_;
  Feedback sharedFb_{};
  Feedback audioUiFb_{};
  Feedback uiFb_{};

  UI ui_{&uiAudioCfg_, &uiFb_};
  AudioSource audio_{&audioCfg_, &audioUiFb_};

  static inline std::atomic<bool> c0Started_ = {false};
  static inline std::atomic<bool> c1Started_ = {false};
  static inline const char* appName_ = nullptr;
};

}  // namespace zlkm