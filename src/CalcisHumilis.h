#pragma once
#include <Arduino.h>
#include <AudioTools.h>
#include <Stream.h>
#include <JLED.h> // or <jled.h> depending on your install

struct CalcisConfig
{
    int sampleRate = 48000;
    float baseHz = 55.0f;
    float startMult = 6.0f;
    float ampMs = 220.0f;
    float pitchMs = 30.0f;
    float clickMs = 6.0f;
    float clickAmt = 0.2f;
    float outGain = 0.85f;
    float pan = 0.0f; // -1..+1 equal-power

    // small attacks (ms) for smooth starts
    float ampAttackMs = 0.001f;
    float pitchAttackMs = 0.01f;
    float clickAttackMs = 0.001f;

    // ... existing ...
    bool  hpfEnabled = true;
    float hpfHz      = 28.0f;     // ~25–35 Hz works well on kicks
    float hpfQ       = 0.7071f;   // Butterworth
};

class CalcisHumilis
{
public:
    explicit CalcisHumilis(const CalcisConfig &cfg = CalcisConfig());

    // Update parameters & envelope rates WITHOUT resetting envelopes/phase
    void setConfig(const CalcisConfig &cfg);

    void trigger(); // retrigger envelopes + reset phase
    void tickLED(); // call from loop() if you want

    // Fill an interleaved stereo block (nFrames = stereo frames)
    void fillBlock(int16_t *dstLR, size_t nFrames);

private:
    static float rateFromMs(float ms, int sr);
    static float softClip(float x);
    static int16_t sat16(float v);
    void applyEnvelopeRates();
    void updatePanGains();

    CalcisConfig cfg_;
    audio_tools::ADSR envAmp, envPitch, envClick;


    audio_tools::HighPassFilter<float> hpfL{28.0f, 48000.0f, 0.7071f};
    audio_tools::HighPassFilter<float> hpfR{28.0f, 48000.0f, 0.7071f};

    float phase = 0.0f, phaseInc = 0.0f;

    // pan gains
    float gainL = 0.7071f, gainR = 0.7071f;

    // LED
    JLed triggerLED{2};
};