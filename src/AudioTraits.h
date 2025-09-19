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

template <int SR_, int OS_, int BITS_, int BLOCK_FRAMES_>
struct AudioTraits {
  using IMPL = BitTraitsImpl<BITS_>;
  using SampleT = typename IMPL::SampleT;

  static constexpr int BITS = BITS_;
  static constexpr int BLOCK_FRAMES = BLOCK_FRAMES_;
  static constexpr int SR = SR_;
  static constexpr int OS = OS_;
  static constexpr size_t BLOCK_BYTES = BLOCK_FRAMES * 2 * sizeof(SampleT);

  using BufferT = std::array<SampleT, BLOCK_FRAMES>;
};

}  // namespace zlkm