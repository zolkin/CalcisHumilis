#include <Arduino.h>
#include "Debug.h"
#include "AudioOut.h"
#include "Kick.h"
#include "Input.h"

// Pins you already use
constexpr uint8_t PIN_BCLK = 10; // LRCK auto = 11
constexpr uint8_t PIN_DATA = 12;
constexpr int SR = 48000;

AudioOut audio;
KickSynth kick;
Input io({.trigBtn = 6}, /*activeHigh=*/true); // your pulldown wiring

void fillCallback(int16_t *dst, int nFrames, int sampleRate)
{
  kick.fillBlock(dst, nFrames, sampleRate);
}

void setup()
{
  DBG_BEGIN(115200);

  DBG_LED_INIT();
  delay(100);

  DBG_PRINTLN("[BOOT] LED init test...");
  io.begin();
  io.setDebounce(3, 8); // tweak if needed

  for (int i = 0; i < 4; ++i)
  {
    DBG_LED_GREEN_ON();
    delay(250);
    DBG_LED_GREEN_OFF();
    delay(250);
  }

  DBG_PRINTLN("[BOOT] Pico2 Kick starting…");

  // Kick params: (baseHz, startMult, ampMs, pitchMs, clickMs, clickAmt, outGain, trigPeriodMs)
  kick.init(SR, 55.0f, 6.0f, 220.0f, 30.0f, 6.0f, 0.20f, 0.85f, 2000);
  DBG_PRINTLN("[INIT] KickSynth configured");

  // Audio: 48k, 64-frame blocks, 2 buffers (low latency)
  bool ok = audio.begin(PIN_BCLK, PIN_DATA, SR, 64, 2, /*warmupMs=*/100);
  DBG_PRINT("[INIT] AudioOut begin: %s, SR=%d, frames=%d, bufs=%d\n",
            ok ? "OK" : "FAIL", SR, audio.framesPerBlock(), 2);
  if (!ok)
    while (1)
    {
    } // simple fatal halt

  audio.setFillCallback(fillCallback);
}

void loop()
{
  io.poll();
  if (io.takeTrigPressed())
  {
    kick.trigger();
  }
  audio.loop(); // services DMA; calls fillCallback when a block is needed
  // (Optionally do UI / serial / CV here)
}