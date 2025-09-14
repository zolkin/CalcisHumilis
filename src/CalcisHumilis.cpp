#include "CalcisHumilis.h"
#include <ArduinoLog.h>
#include <math.h>
using namespace audio_tools;

static inline float sgn_soft(float x) { return tanhf(8.0f * x); }

float CalcisHumilis::rateFromMs(float ms, int sr)
{
    float samples = ms * (float)sr * 0.001f;
    if (samples < 1.0f)
        samples = 1.0f;
    return 1.0f / samples; // per-sample step
}
float CalcisHumilis::softClip(float x)
{
    const float t = 0.95f;
    if (x > t)
        return t + (x - t) * 0.05f;
    if (x < -t)
        return -t + (x + t) * 0.05f;
    return x;
}
int16_t CalcisHumilis::sat16(float v)
{
    if (v > 32767.0f)
        v = 32767.0f;
    if (v < -32768.0f)
        v = -32768.0f;
    return (int16_t)v;
}

CalcisHumilis::CalcisHumilis(const CalcisConfig &cfg) : cfg_(cfg)
{
    updatePanGains();
    applyEnvelopeRates();
    phase = phaseInc = 0.0f;

    Log.notice(F("[Calcis] ctor sr=%d base=%.1f startX=%.1f A=%.0fms P=%.0fms C=%.0fms" CR),
               cfg_.sampleRate, cfg_.baseHz, cfg_.startMult,
               cfg_.ampMs, cfg_.pitchMs, cfg_.clickMs);
}

void CalcisHumilis::setConfig(const CalcisConfig &cfg)
{
    cfg_ = cfg;
    updatePanGains();
    applyEnvelopeRates();
    Log.notice(F("[Calcis] setConfig sr=%d A=%.0fms P=%.0fms C=%.0fms pan=%.2f" CR),
               cfg_.sampleRate, cfg_.ampMs, cfg_.pitchMs, cfg_.clickMs, cfg_.pan);

    hpfL.begin(cfg_.hpfHz, cfg_.sampleRate, cfg_.hpfQ);
    hpfR.begin(cfg_.hpfHz, cfg_.sampleRate, cfg_.hpfQ);
}

void CalcisHumilis::applyEnvelopeRates()
{
    envAmp.setAttackRate(rateFromMs(cfg_.ampAttackMs, cfg_.sampleRate));
    envAmp.setDecayRate(rateFromMs(cfg_.ampMs, cfg_.sampleRate));
    envAmp.setSustainLevel(0.0f);
    envAmp.setReleaseRate(0.0f);

    envPitch.setAttackRate(rateFromMs(cfg_.pitchAttackMs, cfg_.sampleRate));
    envPitch.setDecayRate(rateFromMs(cfg_.pitchMs, cfg_.sampleRate));
    envPitch.setSustainLevel(0.0f);
    envPitch.setReleaseRate(0.0f);

    envClick.setAttackRate(rateFromMs(cfg_.clickAttackMs, cfg_.sampleRate));
    envClick.setDecayRate(rateFromMs(cfg_.clickMs, cfg_.sampleRate));
    envClick.setSustainLevel(0.0f);
    envClick.setReleaseRate(0.0f);
}

void CalcisHumilis::updatePanGains()
{
    // equal-power: -1..+1 -> 0..pi/2
    const float t = (cfg_.pan + 1.0f) * 0.5f * (PI * 0.5f);
    gainL = cosf(t);
    gainR = sinf(t);
}

void CalcisHumilis::trigger()
{
    envAmp.keyOn(1.0f);
    envPitch.keyOn(1.0f);
    envClick.keyOn(1.0f);
    phase = 0.0f;
    triggerLED.FadeOff((uint32_t)cfg_.ampMs);
}

void CalcisHumilis::tickLED()
{
    triggerLED.Update();
}

void CalcisHumilis::fillBlock(int16_t *dstLR, size_t nFrames)
{
    // tight, branchless-ish inner loop; no virtual calls
    const float sr = static_cast<float>(cfg_.sampleRate);

    for (size_t i = 0; i < nFrames; ++i)
    {
        const float a = envAmp.tick();
        const float p = envPitch.tick();
        const float c = envClick.tick();

        const float fNow = cfg_.baseHz * (1.0f + (cfg_.startMult - 1.0f) * p);
        const float targetInc = (2.0f * PI * fNow) / sr;
        phaseInc += (targetInc - phaseInc) * 0.25f;
        phase += phaseInc;
        if (phase >= 2.0f * PI)
        {
            phase -= 2.0f * PI;
        }

        const float s = sinf(phase);
        const float click = cfg_.clickAmt * c * tanhf(8.0f * s);
        float y = (a * s + click) * cfg_.outGain;

        // equal-power pan
        float l = y * gainL;
        float r = y * gainR;

        // High-pass (AudioTools)
        if (cfg_.hpfEnabled)
        {
            l = hpfL.process(l);
            r = hpfR.process(r);
        }

        // soft clip + store
        l = softClip(l);
        r = softClip(r);
        dstLR[2 * i + 0] = sat16(l * 32767.0f);
        dstLR[2 * i + 1] = sat16(r * 32767.0f);
    }
}