#pragma once
#include <AudioTools.h>

#include "AudioTraits.h"
#include "platform/boards/Current.h"

namespace zlkm::audio {

template <class TR_, template <class> class AppT>
class AudioCore {
 public:
  using TR = TR_;
  using App = AppT<TR>;
  using Cfg = typename App::Cfg;
  using CurBoard = zlkm::platform::boards::Current;
  using SrcPinId = typename CurBoard::SrcPinId;
  using Feedback = typename App::Feedback;

  int getPin(SrcPinId pin) { return zlkm::hw::io::getPin(pin).value; }

  AudioCore(Cfg* cfg, Feedback* fb) : app_(cfg, fb) {
    // TODO: move device out of the core
    auto icfg = i2sOut_.defaultConfig(TX_MODE);
    icfg.sample_rate = TR::SR;                   // 96 kHz
    icfg.channels = 2;                           // stereo
    icfg.bits_per_sample = 32;                   // 32-bit words
  // Increase internal I2S ring buffer depth to reduce blocking in write()
  // Defaults are small (size=512, count=6). On RP2350 we can afford more.
  icfg.buffer_size = 2048;   // bytes per buffer; tune 1024..4096
  icfg.buffer_count = 8;     // number of buffers in ring
    icfg.pin_bck = getPin(CurBoard::PIN_BCK);    // PCM510X BCK
    icfg.pin_ws = getPin(CurBoard::PIN_LRCK);    // PCM510X LRCK
    icfg.pin_data = getPin(CurBoard::PIN_DATA);  // PCM510X DIN
    Log.notice(F("[I2S] BCK=%d WS=%d DATA=%d, %d Hz, %d-bit, ch=%d" CR),
               icfg.pin_bck, icfg.pin_ws, icfg.pin_data, icfg.sample_rate,
               icfg.bits_per_sample, icfg.channels);
    bool ok = i2sOut_.begin(icfg);
    if (!ok) {
      Log.error(
          F("[I2S] begin() failed; check pins, adjacency, and wiring" CR));
    }

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
      ZLKM_PERF_SCOPE("AudioCore::I2S write");
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