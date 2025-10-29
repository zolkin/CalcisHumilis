#pragma once
#include <AudioTools.h>

#include "AudioTraits.h"
#include "audio/hw/I2SStereoWriter.h"
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
  using SampleT = typename TR::SampleT;

  int getPin(SrcPinId pin) { return zlkm::hw::io::getPin(pin).value; }

  AudioCore(Cfg* cfg, Feedback* fb) : app_(cfg, fb) {
    // Direct I2S writer (no AudioTools stream)
    hw::I2SBlockWriterCfg i2sCfg;
    i2sCfg.bclkPin = getPin(CurBoard::PIN_BCK);
    i2sCfg.lrckPin = getPin(CurBoard::PIN_LRCK);
    i2sCfg.dataPin = getPin(CurBoard::PIN_DATA);
    i2sCfg.buffers = 3;
    i2sCfg.bufferBlocks = 2;
    bool ok = i2s_.begin(i2sCfg);
    if (!ok) {
      Log.error(F("[I2S] direct begin() failed; check pins/wiring" CR));
    } else {
      Log.notice(F("[I2S] direct, %d Hz, %d-bit, ch=2" CR), TR::SR, TR::BITS);
    }

    // Prime audio
    update();

    Log.notice(F("[Audio] %d Hz, 32-bit, block=%u" CR), TR::SR,
               (unsigned)TR::BLOCK_FRAMES);
    inited_ = true;
  }
  // Called in a tight loop by MainApp on core 1
  void update() {
    app_.fillBlock(audioBuffer_);
    {
      ZLKM_PERF_SCOPE("AudioCore::I2S writeAll");
      i2s_.writeAll(audioBuffer_.data(), TR::BLOCK_FRAMES);
      app_.feedback().overUnderFlowCount = i2s_.getOverUnderflowCount();
    }
  }

 private:
  using OutBuffer = typename TR::BufferT;

  // double-buffering
  alignas(8) OutBuffer audioBuffer_;

  hw::I2SBlockWriter<TR> i2s_;

  App app_;
  bool inited_ = false;

  int whichBuf_ = 0;
  uint8_t* writePtr_ = nullptr;
  size_t bytesLeft_ = 0;
  // New frame-based enqueue state for direct writer

  // trigger edge tracking
  uint32_t lastTrigCounter_ = 0;
};

}  // namespace zlkm::audio