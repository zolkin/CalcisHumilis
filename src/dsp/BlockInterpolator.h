#pragma once
#include <math.h>

#include "math/Constants.h"

namespace zlkm {
namespace dsp {

// Intended Usage:
// Group all the interpolatable parameters in one place and modulate them all at
// once
template <int BLOCK_SIZE, int N>
class BlockInterpolatorN {
  static constexpr float PER_SAMPLE = 1.f / float(BLOCK_SIZE);

 public:
  explicit BlockInterpolatorN(float* src, std::array<float, N> const& targets)
      : cur_(src) {
    for (int i = 0; i < N; ++i) {
      step_[i] = (targets[i] - cur_[i]) * PER_SAMPLE;
    }
  }

  void update() {
    for (int i = 0; i < N; ++i) {
      cur_[i] += step_[i];
    }
  }

 private:
  float* cur_;
  std::array<float, N> step_;
};

template <int BLOCK_SIZE, int N>
inline BlockInterpolatorN<BLOCK_SIZE, N> makeBlockInterpolator(
    float* src, std::array<float, N> const& targets) {
  return BlockInterpolatorN<BLOCK_SIZE, N>(src, targets);
}

}  // namespace dsp
}  // namespace zlkm