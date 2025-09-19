#include "Main.h"  // your dual-core harness (header-only)

#include <Arduino.h>

#include "AudioCore.h"
#include "Profiler.h"
#include "UI.h"

using namespace audio_tools;
using namespace zlkm;

using MyAudioCore = AudioCore<CalcisTR, CalcisHumilis>;
using App = MainApp<MyAudioCore, UI>;

// ---------- Arduino entry points ----------

void setup() {
  App::core0_start(
      "CalcisHumilis");  // hands over to dual-core loops; never returns
}

void loop() {
  ZLKM_PERF_SCOPE_N("core0.loop", 1000);
  App::core0_loop();  // UI tick on core 0
}

void setup1() { App::core1_start(); }

void loop1() {
  App::core1_loop();  // audio tick on core 1
  Profiler::instance().tick_and_log(millis());
}