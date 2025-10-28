#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace zlkm::mod {

// TODO: remove default values from everywhere
struct EnvCfg {
  float attack = 0.0f;  // SR-normalized coeff
  float decay = 0.0f;   // SR-normalized coeff
  float curve = 0.0f;   // lin by deault
};

template <int N>
class ADEnvelopes {
 public:
  using Cfg = std::array<EnvCfg, N>;
  using Mod = Cfg;

  static_assert(N > 0, "N must be > 0");
  static constexpr int ENV_COUNT = N;

  // TODO: tie to epsilon?
  static constexpr float kPeakThresh = 0.999f;  // Attack -> Decay
  static constexpr float kFloorThresh = 1e-4f;  // Decay  -> Idle

  explicit ADEnvelopes(const Cfg& cfg = Cfg{}) : cfg_(cfg) {}

  Cfg& cfg() { return cfg_; }
  const Cfg& cfg() const { return cfg_; }

  Mod& mod() { return mod_; }
  const Mod& mod() const { return mod_; }

  void trigger(int i) { states_[i] = State::Attack; }
  void triggerAll() { states_.fill(State::Attack); }

  // ------ processing (all envelopes) ------
  // A: y += a*(1 - y);  D: y -= d*y
  void update() {
    for (int i = 0; i < N; ++i) {
      float& y = values_[i];
      switch (states_[i]) {
        case State::Idle:
          break;

        case State::Attack: {
          const float a = fmax(0.0f, cfg_[i].attack + mod_[i].attack);
          y += a;
          if (y >= kPeakThresh) {
            y = 1.0f;
            states_[i] = State::Decay;
          }
          const float c = math::clamp01(cfg_[i].curve + mod_[i].curve);
          curved_[i] = computeAttack(y, c);
        } break;

        case State::Decay: {
          const float d = fmax(0.0f, cfg_[i].decay + mod_[i].decay);
          y -= d;
          if (y <= kFloorThresh) {
            y = 0.0f;
            states_[i] = State::Idle;
          }
          const float c = math::clamp01(cfg_[i].curve + mod_[i].curve);
          curved_[i] = computeDecay(y, c);
        } break;
      }
    }
  }

  // ------ queries / maintenance ------
  float value(int i) const { return curved_[i]; }

  void resetAll() {
    states_.fill(State::Idle);
    values_.fill(0.0f);
    curved_.fill(0.0f);
  }

 private:
  static constexpr float computeAttack(float y, float curve) {
    return (1.f - curve) * y + curve * (y * y);
  }

  static constexpr float computeDecay(float y, float curve) {
    return (2.0f - curve) * y - (1.0f - curve) * (y * y);
  }

  enum class State : int8_t { Idle = 0, Attack, Decay };

  Cfg cfg_;
  Cfg mod_{};  // ensure zeroed modulation by default
  std::array<float, N> values_;
  std::array<float, N> curved_;
  std::array<State, N> states_{};
};

}  // namespace zlkm::mod