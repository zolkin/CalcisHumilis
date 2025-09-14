#include <Arduino.h>
#include <ArduinoLog.h>
#include "AudioOut.h"
#include "Kick.h"
#include <Bounce2.h>

// Pins you already use
constexpr uint8_t PIN_BCLK = 10; // LRCK auto = 11
constexpr uint8_t PIN_DATA = 12;
constexpr int SR = 48000;

AudioOut audio;
KickSynth kick;

#define BTN_PIN 6   // your trigger button pin

Bounce2::Button trigBtn = Bounce2::Button();


void fillCallback(int16_t *dst, int nFrames, int sampleRate)
{
  kick.fillBlock(dst, nFrames, sampleRate);
}

void setup()
{
  Serial.begin(115200);
  // Initialize SerialDebug
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  delay(100);

  
  Log.infoln("[BOOT] Button init...");
  trigBtn.attach(BTN_PIN, INPUT_PULLDOWN); // or INPUT if using pulldown
  trigBtn.interval(5);                   // debounce interval in ms
  trigBtn.setPressedState(HIGH);          // LOW = pressed if wiring to GND

  Log.infoln("[BOOT] Pico2 Kick starting...");

  // Kick params: (baseHz, startMult, ampMs, pitchMs, clickMs, clickAmt, outGain, trigPeriodMs)
  kick.init(SR, 55.0f, 6.0f, 220.0f, 30.0f, 6.0f, 0.20f, 0.85f, 2000);
  Log.infoln("[INIT] KickSynth configured");

  // Audio: 48k, 64-frame blocks, 2 buffers (low latency)
  bool ok = audio.begin(PIN_BCLK, PIN_DATA, SR, 64, 2, /*warmupMs=*/100);
  Log.infoln("[INIT] AudioOut begin: %s, SR=%d, frames=%d, bufs=%d",
            ok ? "OK" : "FAIL", SR, audio.framesPerBlock(), 2);
  if (!ok)
    while (1)
    {
    } // simple fatal halt

  audio.setFillCallback(fillCallback);
}

void loop()
{
  trigBtn.update();

  if (trigBtn.pressed())
  {
    kick.trigger();
  }
  audio.loop(); // services DMA; calls fillCallback when a block is needed
  // (Optionally do UI / serial / CV here)
}