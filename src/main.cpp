#include <Arduino.h>
#include <ArduinoLog.h>
#include <AudioTools.h>

#include "CalcisHumilis.h"
#include "UI.h"

using namespace audio_tools;

constexpr int SR = 48000;
constexpr uint8_t PIN_BCLK = 10, PIN_LRCK = 11, PIN_DATA = 12;

constexpr size_t BLOCK_FRAMES = 64;
constexpr size_t BLOCK_BYTES = BLOCK_FRAMES * 2 * sizeof(int32_t);

int32_t audioBufA[BLOCK_FRAMES * 2];
int32_t audioBufB[BLOCK_FRAMES * 2];
int whichBuf = 0;
uint8_t* writePtr = nullptr;
size_t bytesLeft = 0;

CalcisConfig kickCfg;
CalcisHumilis kick;

I2SStream i2sOut;

UIConfig uiCfg{&kickCfg};
UI ui(&uiCfg, &kick);  // default: trig=6, A0 amp decay 20..2000ms, A1 pitch
                       // decay 2..800ms

static void queueNextBlockIfNeeded() {
  if (bytesLeft == 0) {
    int32_t* buf = (whichBuf == 0) ? audioBufA : audioBufB;
    whichBuf ^= 1;
    kick.fillBlock(buf, BLOCK_FRAMES);
    writePtr = reinterpret_cast<uint8_t*>(buf);
    bytesLeft = BLOCK_BYTES;
  }
  if (bytesLeft) {
    size_t wrote = i2sOut.write(writePtr, bytesLeft);
    writePtr += wrote;
    bytesLeft -= wrote;
  }
}

void setup() {
  Serial.begin(115200);
  UI::waitForSerial();
  ui.attachADS();
  Log.begin(LOG_LEVEL_NOTICE, &Serial);

  auto icfg = i2sOut.defaultConfig(TX_MODE);
  icfg.sample_rate = SR;
  icfg.channels = 2;
  icfg.bits_per_sample = 32;
  icfg.pin_bck = PIN_BCLK;
  icfg.pin_ws = PIN_LRCK;
  icfg.pin_data = PIN_DATA;
  i2sOut.begin(icfg);

  kickCfg.sampleRate = SR;
  kickCfg.baseHz = 75.0f;
  kickCfg.startMult = 6.0f;
  kickCfg.ampMs = 220.0f;
  kickCfg.pitchMs = 30.0f;
  kickCfg.clickMs = 6.0f;
  kickCfg.clickAmt = 0.20f;
  kickCfg.outGain = 0.85f;
  kickCfg.pan = 0.0f;

  kick.setConfig(kickCfg);

  // Prime audio
  queueNextBlockIfNeeded();

  Log.notice(F("[Audio] %d Hz, 32-bit, block=%u" CR), SR,
             (unsigned)BLOCK_FRAMES);
}

void loop() {
  ui.update();
  queueNextBlockIfNeeded();
  kick.tickLED();
}