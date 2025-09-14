#include "AudioOut.h"
#include <string.h>

AudioOut *AudioOut::s_self = nullptr;

bool AudioOut::begin(uint8_t pinBclk, uint8_t pinData, int sampleRate,
                     int framesPerBlock, int numBuffers, int warmupMs)
{
    queued = 0;
    s_self = this;
    sr = sampleRate;
    frames = framesPerBlock;
    numBuf = numBuffers;

    blockBytes = frames * 2 /*channels*/ * sizeof(int16_t);
    bufferWords = blockBytes / 4;

    // allocate two interleaved stereo buffers
    bufA = (int16_t *)malloc(blockBytes);
    bufB = (int16_t *)malloc(blockBytes);
    if (!bufA || !bufB)
        return false;
    memset(bufA, 0, blockBytes);
    memset(bufB, 0, blockBytes);

    i2s.setBCLK(pinBclk); // LRCK auto = pinBclk + 1
    i2s.setDOUT(pinData);
    i2s.setBitsPerSample(16);
    i2s.setBuffers(numBuf, bufferWords, 0); // ring of N buffers
    i2s.setFrequency(sr);
    i2s.onTransmit(&AudioOut::onTxISR);

    if (!i2s.begin())
        return false;

    Log.info("[I2S] BCLK=GP%d, LRCK=GP%d, DATA=GP%d, SR=%d, frames=%d, bufs=%d\n", pinBclk, pinBclk + 1, pinData, sr, frames, numBuf);

    // Warm-up: a little silence so the DAC locks (optional but helps PCM510x)
    if (warmupMs > 0)
    {
        // write enough zero frames to cover warmupMs
        const int framesWarm = (sr * warmupMs) / 1000;
        int remaining = framesWarm;
        while (remaining > 0)
        {
            int chunk = min(remaining, frames);
            memset(bufA, 0, blockBytes);
            i2s.write((uint8_t *)bufA, chunk * 2 * sizeof(int16_t));
            remaining -= chunk;
        }
    }

    Log.info("[I2S] Warm-up %d ms of silence\n", warmupMs);

    // Prime one buffer so DMA starts
    i2s.write((uint8_t *)bufA, blockBytes);
    queued = 1;      // ← we have one block in flight now
    needFill = true; // request the next buffer to be synthesized ASAP
    return true;
}

void AudioOut::onTxISR()
{
    if (s_self)
    {
        s_self->needFill = true;
        s_self->queued--; // one finished
#ifdef DEBUG
        s_self->txCallbacks++;
#endif
    }
}

void AudioOut::handleTx()
{
    if (!needFill || !fillFn)
        return;
    needFill = false;

    // Only queue if we have headroom (keep ≤1 queued total)
    if (queued >= 1)
        return;

    // Flip buffer (double-buffering)
    int16_t *buf = (which == 0) ? bufB : bufA;
    which ^= 1;

    // Fill one block
    fillFn(buf, frames, sr);

    // Queue exactly one block; if queue is full, try next loop tick
    size_t wrote = i2s.write((uint8_t *)buf, blockBytes);

    needFill = wrote == 0;
    if (needFill)
    {
        Log.info("[I2S] UNDERRUN #%d: queue full, will retry\n", underrunCount++);
    }
}

void AudioOut::loop()
{
    handleTx();
    // Optional: small idle; not required
    // delayMicroseconds(20);
}