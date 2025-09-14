#pragma once
#include <Arduino.h>
#include <I2S.h>
#include <ArduinoLog.h>

typedef void (*AudioFillFn)(int16_t *dst, int nFrames, int sampleRate);

class AudioOut
{
public:
    bool begin(uint8_t pinBclk, uint8_t pinData, int sampleRate,
               int framesPerBlock, int numBuffers, int warmupMs = 0);
    void setFillCallback(AudioFillFn fn) { fillFn = fn; }
    void loop(); // call frequently (main loop)

    int sampleRate() const { return sr; }
    int framesPerBlock() const { return frames; }

private:
    static void onTxISR();
    void handleTx();

    I2S i2s{OUTPUT};
    static AudioOut *s_self;

    volatile bool needFill = false;
    int16_t *bufA = nullptr;
    int16_t *bufB = nullptr;
    int which = 0;

    int sr = 48000;
    int frames = 64;
    int blockBytes = 0;
    int bufferWords = 0;
    int numBuf = 2;

    AudioFillFn fillFn = nullptr;

    volatile int queued = 0; // how many blocks currently queued to I2S

#ifdef DEBUG
    volatile uint32_t underrunCount = 0;
    volatile uint32_t txCallbacks = 0;
#endif
};