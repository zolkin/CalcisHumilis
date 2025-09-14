#include <Arduino.h>
#include <ArduinoLog.h>
#include <AudioTools.h>
#include <Bounce2.h>
#include "CalcisHumilis.h"

using namespace audio_tools;

constexpr int SR = 48000;
constexpr uint8_t PIN_BCLK = 10, PIN_LRCK = 11, PIN_DATA = 12;
constexpr uint8_t BTN_PIN  = 6;

// choose a sensible audio block size (64–256 frames)
constexpr size_t BLOCK_FRAMES = 64;
constexpr size_t BLOCK_BYTES  = BLOCK_FRAMES * 2 /*ch*/ * sizeof(int16_t);

// double buffer
int16_t audioBufA[BLOCK_FRAMES * 2];
int16_t audioBufB[BLOCK_FRAMES * 2];

int whichBuf = 0;
uint8_t* writePtr = nullptr;
size_t   bytesLeft = 0;

CalcisConfig cfg;
CalcisHumilis kick;

I2SStream i2sOut;
Bounce2::Button trigBtn;

void queueNextBlockIfNeeded() {
  if (bytesLeft == 0) {
    // fill next buffer
    int16_t* buf = (whichBuf == 0) ? audioBufA : audioBufB;
    whichBuf ^= 1;
    kick.fillBlock(buf, BLOCK_FRAMES);
    writePtr  = reinterpret_cast<uint8_t*>(buf);
    bytesLeft = BLOCK_BYTES;
  }

  if (bytesLeft) {
    size_t wrote = i2sOut.write(writePtr, bytesLeft); // single virtual per block-chunk
    writePtr += wrote;
    bytesLeft -= wrote;
  }
}

void setup() {
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_NOTICE, &Serial);

  // Button
  trigBtn.attach(BTN_PIN, INPUT_PULLDOWN);
  trigBtn.interval(5);
  trigBtn.setPressedState(HIGH);

  // I2S
  auto icfg = i2sOut.defaultConfig(TX_MODE);
  icfg.sample_rate     = SR;
  icfg.channels        = 2;
  icfg.bits_per_sample = 16;
  icfg.pin_bck  = PIN_BCLK;
  icfg.pin_ws   = PIN_LRCK;  // must be BCLK+1 on RP2040
  icfg.pin_data = PIN_DATA;
  i2sOut.begin(icfg);

  // Kick params
  cfg.sampleRate = SR;
  cfg.baseHz     = 55.0f;
  cfg.startMult  = 6.0f;
  cfg.ampMs      = 220.0f;
  cfg.pitchMs    = 30.0f;
  cfg.clickMs    = 6.0f;
  cfg.clickAmt   = 0.20f;
  cfg.outGain    = 0.85f;
  cfg.pan        = 0.0f;

  kick.setConfig(cfg);

  // Prime first block so audio starts immediately
  queueNextBlockIfNeeded();

  Log.notice(F("[Audio] Started: %d Hz, 16-bit stereo, block=%u frames" CR),
             SR, (unsigned)BLOCK_FRAMES);
}

void loop() {
  trigBtn.update();
  if (trigBtn.pressed()) {
    kick.trigger();
  }

  // pump audio: fill then write in sizable blocks
  queueNextBlockIfNeeded();

  // optional LED service outside audio path
  kick.tickLED();
}