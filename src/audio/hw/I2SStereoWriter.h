#pragma once

#include <I2S.h>
#include <stdint.h>

#include <array>

#include "hw/io/Pin.h"

namespace zlkm::audio::hw {

struct I2SBlockWriterCfg {
  int bclkPin = -1;
  int lrckPin = -1;
  int dataPin = -1;
  int mclkPin = -1;
  int bufferBlocks = 4;
  int buffers = 3;
};

template <class TR_>
class I2SBlockWriter {
 public:
  using TR = TR_;
  using SampleT = typename TR::SampleT;

  static_assert((TR::BITS == 16) || (TR::BITS == 24) || (TR::BITS == 32),
                "I2SBlockWriter supports only 16, 24, or 32 bits per sample");
  static constexpr int SR = TR::SR;
  static constexpr int BITS = TR::BITS;
  static constexpr int CHANNELS = TR::CHANNELS;

  // Configure and start I2S using current board pins
  bool begin(I2SBlockWriterCfg cfg) {
    cfg_ = cfg;

    // Configure clock pins; library requires WS adjacent to BCK
    if (cfg.lrckPin == cfg.bclkPin + 1) {
      if (!i2s_.setBCLK(cfg.bclkPin)) return false;
    } else if (cfg.lrckPin == cfg.bclkPin - 1) {
      // WS below BCLK: set BCLK first, then swap
      if (!i2s_.setBCLK(cfg.bclkPin)) return false;
      if (!i2s_.swapClocks()) return false;
    } else {
      // Pins must be adjacent; unsupported wiring
      return false;
    }

    if (!i2s_.setDATA(cfg.dataPin)) return false;
    if (!i2s_.setBitsPerSample(BITS)) return false;

    constexpr int WORDS_PER_FRAME = TR::FRAME_SIZE / sizeof(int32_t);
    const int wordsPerBuffer =
        TR::BLOCK_FRAMES * WORDS_PER_FRAME * cfg.bufferBlocks;
    i2s_.setBuffers(cfg.buffers, wordsPerBuffer);

    // Start I2S at TR::SR in output mode
    if (!i2s_.begin(SR)) return false;

    const int latencyMs =
        (cfg.buffers * cfg.bufferBlocks * TR::BLOCK_FRAMES * 1000) / SR;

    Log.infoln(
        F("[I2S] Started I2S output: SR=%d Hz, BITS=%d, BCLK Pin=%d, LRCK "
          "Pin=%d, "
          "DATA Pin=%d, buffers=%d of %d blocks (%d frames each); "
          "Estimated latency=%d ms" CR),
        SR, BITS, cfg.bclkPin, cfg.lrckPin, cfg.dataPin, cfg.buffers,
        cfg.bufferBlocks, TR::BLOCK_FRAMES, latencyMs);

    active_ = true;
    return true;
  }

  void end() {
    if (active_) {
      i2s_.end();
      active_ = false;
    }
  }

  bool isActive() const { return active_; }

  // Non-blocking write of interleaved stereo frames.
  // Returns number of frames actually written to the driver.
  int writeSamples(const SampleT* interleavedLR, int frames) {
    if (!active_ || interleavedLR == nullptr || frames <= 0) return 0;
    constexpr int frameSizeBytes = sizeof(SampleT) * 2;
    const uint8_t* bufPtr = reinterpret_cast<const uint8_t*>(interleavedLR);
    const int availableFrames = i2s_.availableForWrite() / frameSizeBytes;
    const int toWriteFrames = std::min(availableFrames, frames);
    size_t bytesWritten = i2s_.write(bufPtr, toWriteFrames * frameSizeBytes);
    const int written = bytesWritten / frameSizeBytes;
    if (written != toWriteFrames) {
      Log.warning(
          F("[I2S] writeSamples requested %d frames, wrote %d frames" CR),
          toWriteFrames, written);
    }

    return written;
  }

  // Blocking write of interleaved stereo frames.
  int writeAll(const SampleT* interleavedLR, int frames) {
    if (!active_ || interleavedLR == nullptr || frames == 0) return 0;
    const int requestedFrames = frames;
    while (true) {
      int written = writeSamples(interleavedLR, frames);
      if (written > 0) {
        interleavedLR += written * 2;  // advance by frames * 2 channels
        frames -= written;
        if (frames == 0) {
          return requestedFrames;  // all done
        }
      }
      overUnderflowCount += int(i2s_.getOverUnderflow());
      tight_loop_contents();
    }
  }

  ~I2SBlockWriter() { end(); }

  I2SBlockWriterCfg cfg() const { return cfg_; }
  int getOverUnderflowCount() const { return overUnderflowCount; }

 private:
  I2S i2s_;
  bool active_ = false;
  int overUnderflowCount = 0;

  I2SBlockWriterCfg cfg_;
};

}  // namespace zlkm::audio::hw
