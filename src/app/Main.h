#pragma once
#include <atomic>

#include "platform/platform.h"
#include "util/Profiler.h"
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
  static void ui_start(const char* name) {
    appName_ = name;
    Serial.begin(115200);
    waitForSerial(10000);  // keep your original behavior
    Log.begin(LOG_LEVEL_TRACE, &Serial);
    get();  // initialize the app for the first time
    delay(100);
    Log.infoln("SDK Version: %s", zlkm::platform::getSdkVersion());
    Log.infoln("Application %s started on core0", name);
    c0Started_ = true;
    while (!c1Started_) {
      delay(1);
    }
  }

  static void ui_loop() {
    get().snapUIFeedback();
    get().ui_.update();
    get().publishAudioCfg();
    sleep_ms(2);
  }

  static void audio_start() {
    while (!c0Started_) {
      delay(1);
    }
    Log.infoln("Application %s started on core1", appName_);
    c1Started_ = true;
  }

  static void audio_loop() {
    get().snapAudioCfg();
    get().audio_.update();
    get().publishUIFeedback();
  }

 private:
  static void waitForSerial(unsigned long timeoutMs) {
    const unsigned long t0 = millis();
    while (!(Serial && Serial.dtr()) && (millis() - t0) < timeoutMs) {
      delay(10);
    }
  }

  void snapAudioCfg() {
    ZLKM_PERF_SCOPE("MainApp::snapAudioCfg");
    util::sl_guard guard{cfgSL_};
    audioCfg_ = sharedCfg_;
  }

  void publishAudioCfg() {
    ZLKM_PERF_SCOPE("MainApp::publishAudioCfg");
    util::sl_guard guard{cfgSL_};
    sharedCfg_ = uiAudioCfg_;
  }

  void publishUIFeedback() {
    ZLKM_PERF_SCOPE("MainApp::publishUIFeedback");
    util::sl_guard guard{fbSL_};
    sharedFb_ = audioUiFb_;
  }

  void snapUIFeedback() {
    ZLKM_PERF_SCOPE("MainApp::snapUIFeedback");
    util::sl_guard guard{fbSL_};
    uiFb_ = sharedFb_;
  }

  static MainApp& get() {
    static MainApp<AudioSource, UI> inst;
    return inst;
  }

  MainApp() = default;

  using Cfg = typename AudioSource::Cfg;
  using Feedback = typename AudioSource::Feedback;

  util::spin_lock cfgSL_;
  Cfg sharedCfg_{};
  Cfg audioCfg_{};
  Cfg uiAudioCfg_{};

  util::spin_lock fbSL_;
  Feedback sharedFb_{};
  Feedback audioUiFb_{};
  Feedback uiFb_{};

  UI ui_{&uiAudioCfg_, &uiFb_};
  AudioSource audio_{&audioCfg_, &audioUiFb_};

  static inline std::atomic<bool> c0Started_ = {false};
  static inline std::atomic<bool> c1Started_ = {false};
  static inline const char* appName_ = nullptr;
};

}  // namespace zlkm::app