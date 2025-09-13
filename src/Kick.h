#pragma once
#include <Arduino.h>
#include <Debug.h>

class KickSynth
{
public:
    void init(int sampleRate,
              float baseHz, float startMult,
              float ampMs, float pitchMs, float clickMs,
              float clickAmt, float outGain,
              int trigPeriodMs);
    void trigger();
    void fillBlock(int16_t *dstInterleaved, int nFrames, int sampleRate);

private:
    // params
    int sr = 48000;
    float baseHz = 55.0f, startMult = 6.0f;
    float ampMs = 220.0f, pitchMs = 30.0f, clickMs = 6.0f;
    float clickAmt = 0.2f, outGain = 0.85f;

    // state
    float ampEnv = 0, pitchEnv = 0, clickEnv = 0;
    float ampA = 0, pitchA = 0, clickA = 0;
    float phase = 0, phaseInc = 0;
    uint32_t sampleCounter = 0;

#ifdef DEBUG
    uint32_t trigCounter = 0;
    static const uint32_t LED_BLINK_SAMPLES = 1000;

    int32_t ledOnSamples = 0;

    void autoTurnOffLed()
    {
        if (ledOnSamples > 0 && ledOnSamples <= sampleCounter)
        {
            DBG_LED_GREEN_OFF();
        }
    }

    void turnOnLed()
    {
        ledOnSamples = sampleCounter + LED_BLINK_SAMPLES;
        DBG_LED_GREEN_ON();
    }
#else
    void autoTurnOffLed() {}
    void turnOnLed() {}
#endif

    inline float softClip(float x) const
    {
        const float t = 0.95f;
        if (x > t)
            return t + (x - t) * 0.05f;
        if (x < -t)
            return -t + (x + t) * 0.05f;
        return x;
    }
};