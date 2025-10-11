#include "app/Main.h"

#include <Arduino.h>

#include "audio/AudioCore.h"
#include "util/Profiler.h"

// this needs to come last
#include "ui/UI.h"

using namespace audio_tools;
using namespace zlkm;
using namespace zlkm::ch;
using namespace zlkm::util;
using namespace zlkm::ui;

using MyAudioCore = audio::AudioCore<CalcisTR, CalcisHumilis>;
using App = app::MainApp<MyAudioCore, UI>;

#include <pico/platform.h>  // get_core_num()

// ---------- Arduino entry points ----------

void setup() {
  ZLKM_PROFILE_INIT_DEFAULT();
  ZLKM_PROFILE_SET_THREAD_INDEX(
      []() -> uint8_t { return (uint8_t)get_core_num(); });
  ZLKM_PROFILE_SET_EMIT_THREAD(0);  // UI/core0 prints for all threads

  App::ui_start(
      "CalcisHumilis");  // hands over to dual-core loops; never returns
}

void loop() {
  {
    ZLKM_PERF_SCOPE("core0.loop(UI)");
    App::ui_loop();  // UI tick on core 0
  }
  ZLKM_PROFILE_TICK();
}

void setup1() { App::audio_start(); }

void loop1() {
  ZLKM_PERF_SCOPE("core1.loop(audio)");
  App::audio_loop();  // audio tick on core 1
}