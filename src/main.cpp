#include <Arduino.h>
#include <I2S.h>
#include <math.h>

I2S i2s(OUTPUT);
constexpr int SAMPLE_RATE = 48000;
constexpr uint8_t PIN_BCLK = 10;   // LRCK auto = 11
constexpr uint8_t PIN_DATA = 12;   // DIN

// Kick params
constexpr float BASE_FREQ_HZ   = 55.0f;
constexpr float START_MULT     = 6.0f;
constexpr float AMP_DECAY_MS   = 220.0f;
constexpr float PITCH_DECAY_MS = 30.0f;
constexpr float CLICK_DECAY_MS = 6.0f;
constexpr float CLICK_AMOUNT   = 0.20f;
constexpr float OUT_GAIN       = 0.85f;

// Periodic trigger every 2 seconds
constexpr int   TRIGGER_PERIOD_SAMPLES = SAMPLE_RATE * 2;

// block size per main loop iteration
constexpr int   BLOCK_FRAMES = 256;

float ampEnv=0, pitchEnv=0, clickEnv=0;
float phase=0, phaseInc=0;
float ampA, pitchA, clickA;
uint32_t sampleCounter = 0;

volatile uint32_t gTrigCount = 0;   // trigger counter


float decayCoeffMs(float ms) {
  float tau = (ms <= 0.1f) ? 1.0f : (ms * SAMPLE_RATE / 1000.0f);
  return expf(-1.0f / tau);
}

inline float softClip(float x) {
  const float t = 0.95f;
  if (x >  t) return t + (x - t) * 0.05f;
  if (x < -t) return -t + (x + t) * 0.05f;
  return x;
}

void triggerKick() {
  ampEnv = 1.0f;
  pitchEnv = 1.0f;
  clickEnv = 1.0f;
  phase = 0.0f;
  Serial.printf("TRIG #%lu @ %lu ms\n", (unsigned long)gTrigCount, (unsigned long)millis());
  gTrigCount++;
}

void setup() {
  Serial.begin(115200);          // <-- add this

  ampA   = decayCoeffMs(AMP_DECAY_MS);
  pitchA = decayCoeffMs(PITCH_DECAY_MS);
  clickA = decayCoeffMs(CLICK_DECAY_MS);

  i2s.setBCLK(PIN_BCLK);
  i2s.setDOUT(PIN_DATA);
  i2s.setBitsPerSample(16);     // if your board prefers, try 32
  i2s.setFrequency(SAMPLE_RATE);
  if (!i2s.begin()) { while (1) {} }

  delay(20); // 20–50 ms
  // push some silence so DIN is valid too
  for (int i = 0; i < 48000/10; ++i) i2s.write16(0, 0); // ~100 ms of zeros

  triggerKick(); // fire immediately
}

void loop() {
  // produce a fixed block of audio each loop
  int framesWritten = 0;
  while (framesWritten < BLOCK_FRAMES) {
    if (!i2s.availableForWrite()) break;

    // re-trigger exactly every TRIGGER_PERIOD_SAMPLES
    if ((sampleCounter % TRIGGER_PERIOD_SAMPLES) == 0) {
      triggerKick();
    }

    // instantaneous freq with decaying pitch
    float fNow = BASE_FREQ_HZ * (1.0f + (START_MULT - 1.0f) * pitchEnv);
    // slight slew for stability
    float targetInc = (2.0f * PI * fNow) / (float)SAMPLE_RATE;
    phaseInc += (targetInc - phaseInc) * 0.25f;
    phase += phaseInc;
    if (phase >= 2.0f * PI) phase -= 2.0f * PI;

    float s = sinf(phase);
    float click = CLICK_AMOUNT * clickEnv * (s >= 0 ? 1.0f : -1.0f);
    float y = softClip((ampEnv * s + click) * OUT_GAIN);
    int16_t v = (int16_t)(y * 32767.0f);

    i2s.write16(v, v);  // stereo
    framesWritten++;
    sampleCounter++;

    // update envelopes
    ampEnv   *= ampA;
    pitchEnv *= pitchA;
    clickEnv *= clickA;
    if (ampEnv < 1e-6f) { ampEnv = 0; clickEnv = 0; } // tail cut
  }

  // brief breather to let other housekeeping happen
  // (adjust or remove if not needed)
  // delayMicroseconds(50);
}