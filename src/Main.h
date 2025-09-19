#pragma once
#include <Arduino.h>
#include <ArduinoLog.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/sync.h>

#include <atomic>

namespace zlkm {
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
    waitForSerial(5000);  // keep your original behavior
    Log.begin(LOG_LEVEL_TRACE, &Serial);
    get();  // initialize the app for the first time
    Log.infoln("Application %s started on core0", name);
    started_ = true;
  }

  static void core1_start() {
    while (!started_) {
      delay(1);
    }
    Log.infoln("Application %s started on core1", appName_);
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
    mutex_enter_blocking(&cfgMutex_);
    audioCfg_ = sharedCfg_;
    mutex_exit(&cfgMutex_);
  }

  void publishAudioCfg() {
    mutex_enter_blocking(&cfgMutex_);
    sharedCfg_ = uiAudioCfg_;
    mutex_exit(&cfgMutex_);
  }

  void publishUIFeedback() {
    mutex_enter_blocking(&fbMutex_);
    sharedFb_ = audioUiFb_;
    mutex_exit(&fbMutex_);
  }

  void snapUIFeedback() {
    mutex_enter_blocking(&fbMutex_);
    uiFb_ = sharedFb_;
    mutex_exit(&fbMutex_);
  }

  static MainApp& get() {
    static MainApp<AudioSource, UI> inst;
    return inst;
  }

  MainApp() {
    mutex_init(&cfgMutex_);
    mutex_init(&fbMutex_);
  }

  using Cfg = typename AudioSource::Cfg;
  using Feedback = typename AudioSource::Feedback;

  mutex_t cfgMutex_;
  Cfg sharedCfg_{};
  Cfg audioCfg_{};
  Cfg uiAudioCfg_{};

  mutex_t fbMutex_;
  Feedback sharedFb_{};
  Feedback audioUiFb_{};
  Feedback uiFb_{};

  AudioSource audio_{&audioCfg_, &audioUiFb_};
  UI ui_{&uiAudioCfg_, &uiFb_};

  static inline std::atomic<bool> started_ = {false};
  static inline const char* appName_ = nullptr;
};

}  // namespace zlkm