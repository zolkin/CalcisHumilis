#include "app\Main.h"  // your dual-core harness (header-only)

#include <Arduino.h>

#include "audio/AudioCore.h"
#include "util/Profiler.h"
#include "UI.h"

using namespace audio_tools;
using namespace zlkm;
using namespace zlkm::ch;

using MyAudioCore = audio::AudioCore<CalcisTR, CalcisHumilis>;
using App = app::MainApp<MyAudioCore, UI>;

// ---------- Arduino entry points ----------

void setup() {
  App::core0_start(
      "CalcisHumilis");  // hands over to dual-core loops; never returns
}

void loop() {
  App::core0_loop();  // UI tick on core 0
  Profiler::instance().tick_and_log(millis());
}

void setup1() { App::core1_start(); }

void loop1() {
  ZLKM_PERF_SCOPE_N("core1.loop(audio)", 1000);
  App::core1_loop();  // audio tick on core 1
}