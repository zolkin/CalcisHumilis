#pragma once
#include <AudioTools.h>

#include "AudioTraits.h"

namespace zlkm::audio {
// ---------- Hardware pins ---------- TODO: move to the Output.h
constexpr uint8_t PIN_BCLK = 10, PIN_LRCK = 11, PIN_DATA = 12;

template <class TR_, template <class> class AppT>
class AudioCore {
 public:
  using TR = TR_;
  using App = AppT<TR>;
  using Cfg = typename App::Cfg;
  using Feedback = typename App::Feedback;

  AudioCore(Cfg* cfg, Feedback* fb) : app_(cfg, fb) {
    // TODO: move device out of the core
    auto icfg = i2sOut_.defaultConfig(TX_MODE);
    icfg.sample_rate = TR::SR;  // 96 kHz
    icfg.channels = 2;          // stereo
    icfg.bits_per_sample = 32;  // 32-bit words
    icfg.pin_bck = PIN_BCLK;    // PCM5100 BCK
    icfg.pin_ws = PIN_LRCK;     // PCM5100 LRCK
    icfg.pin_data = PIN_DATA;   // PCM5100 DIN
    i2sOut_.begin(icfg);

    // Prime audio
    queueNextBlockIfNeeded_();

    Log.notice(F("[Audio] %d Hz, 32-bit, block=%u" CR), TR::SR,
               (unsigned)TR::BLOCK_FRAMES);
    inited_ = true;
  }
  // Called in a tight loop by MainApp on core 1
  void update() {
    // keep I2S fed (non-blocking; double-buffered)
    queueNextBlockIfNeeded_();
    // Lightweight pacing hint for SDK; no sleeps in audio path
    tight_loop_contents();
  }

 private:
  // ---- audio write helper (non-blocking) ----
  void queueNextBlockIfNeeded_() {
    if (bytesLeft_ == 0) {
      OutBuffer& buf = (whichBuf_ == 0) ? audioBufA_ : audioBufB_;
      whichBuf_ ^= 1;
      app_.fillBlock(buf);

      writePtr_ = reinterpret_cast<uint8_t*>(buf.data());
      bytesLeft_ = TR::BLOCK_BYTES;
    }
    if (bytesLeft_) {
      size_t wrote = i2sOut_.write(writePtr_, bytesLeft_);
      writePtr_ += wrote;
      bytesLeft_ -= wrote;
    }
  }

  using OutBuffer = typename TR::BufferT;

  // double-buffering
  alignas(8) OutBuffer audioBufA_;
  alignas(8) OutBuffer audioBufB_;

  I2SStream i2sOut_;
  App app_;
  bool inited_ = false;

  int whichBuf_ = 0;
  uint8_t* writePtr_ = nullptr;
  size_t bytesLeft_ = 0;

  // trigger edge tracking
  uint32_t lastTrigCounter_ = 0;
};

}  // namespace zlkm::audio