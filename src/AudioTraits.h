#pragma once
#include <AudioTools.h>

#include <array>

namespace zlkm {

template <int BITS>
struct BitTraitsImpl;

template <>
struct BitTraitsImpl<16> {
  using SampleT = int16_t;
};

template <>
struct BitTraitsImpl<24> {
  using SampleT = int32_t;
};

template <>
struct BitTraitsImpl<32> {
  using SampleT = int32_t;
};

template <int SR_, int OS_, int BITS_, int BLOCK_FRAMES_, bool STEREO_ = true>
struct AudioTraits {
  using IMPL = BitTraitsImpl<BITS_>;
  using SampleT = typename IMPL::SampleT;

  static constexpr bool STEREO = STEREO_;
  static constexpr int BITS = BITS_;
  static constexpr int BLOCK_FRAMES = BLOCK_FRAMES_;
  static constexpr int SR = SR_;
  static constexpr int OS = OS_;
  static constexpr size_t BLOCK_BYTES = BLOCK_FRAMES * 2 * sizeof(SampleT);
  static constexpr int BLOCK_ELEMS = STEREO ? BLOCK_FRAMES * 2 : BLOCK_FRAMES;

  using BufferT = std::array<SampleT, BLOCK_ELEMS>;
};

}  // namespace zlkm