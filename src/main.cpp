#include <Arduino.h>
#include <ArduinoLog.h>
#include <AudioTools.h>

#include "CalcisHumilis.h"
#include "UI.h"

using namespace audio_tools;

constexpr uint8_t PIN_BCLK = 10, PIN_LRCK = 11, PIN_DATA = 12;

constexpr size_t BLOCK_FRAMES = 256;
constexpr size_t BLOCK_BYTES = BLOCK_FRAMES * 2 * sizeof(int32_t);
constexpr size_t FRAME_COUNT = 4;

int32_t audioBufA[BLOCK_FRAMES * 2];
int32_t audioBufB[BLOCK_FRAMES * 2];
int whichBuf = 0;
uint8_t* writePtr = nullptr;
size_t bytesLeft = 0;

Calcis::Cfg kickCfg;
Calcis kick;

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
  icfg.sample_rate = CALCIS_SR;  // 96 kHz
  icfg.channels = 2;             // stereo
  icfg.bits_per_sample = 32;     // 32-bit words → BCK should be 64*fs
  icfg.pin_bck = PIN_BCLK;       // to PCM5100 BCK
  icfg.pin_ws = PIN_LRCK;        // to PCM5100 LRCK
  icfg.pin_data = PIN_DATA;      // to PCM5100 DIN
  i2sOut.begin(icfg);

  kick.setConfig(kickCfg);

  // Prime audio
  queueNextBlockIfNeeded();

  Log.notice(F("[Audio] %d Hz, 32-bit, block=%u" CR), CALCIS_SR,
             (unsigned)BLOCK_FRAMES);
}

void loop() {
  static int frames = 0;
  static int64_t total_millis = 0;

  static int pframes = 0;
  static int64_t ptotal_millis = millis();

  ui.update();

  queueNextBlockIfNeeded();

  kick.tickLED();

  total_millis = millis();
  ++frames;
  if (frames % 1000 == 0) {
    const double avgFrameTime =
        float(total_millis - ptotal_millis) / (float)(frames - pframes);
    Log.infoln("[MAIN] avg frame time: %F", avgFrameTime);

    ptotal_millis = total_millis;
    pframes = frames;
  }
}