#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace zlkm::mod {

template <int N>
class ADEnvelopes {
 public:
  static_assert(N > 0, "N must be > 0");
  static constexpr int ENV_COUNT = N;

  struct EnvCfg {
    float attack = 1.0f;  // SR-normalized coeff
    float decay = 1.0f;   // SR-normalized coeff
    float depth = 1.0f;   // output scaling
  };

  struct Cfg {
    float peakThresh = 0.999f;  // Attack -> Decay when y >= this
    float floorThresh = 1e-4f;  // Decay  -> Idle  when y <= this
    bool resetToZeroOnTrigger = false;
  };

  explicit ADEnvelopes(const Cfg& cfg = Cfg{}) : cfg_(cfg) {
    values_.fill(0.0f);
    states_.fill(State::Idle);
    for (int i = 0; i < N; ++i) env_[i] = EnvCfg{};
  }

  // ------ configuration (no sanitization) ------
  void setEnv(int i, const EnvCfg& e) { env_[i] = e; }
  EnvCfg getEnv(int i) const { return env_[i]; }
  void setAllEnv(const EnvCfg& e) {
    for (auto& v : env_) v = e;
  }

  void setRates(int i, float a, float d) {
    env_[i].attack = a;
    env_[i].decay = d;
  }
  void setAttack(int i, float a) { env_[i].attack = a; }
  void setDecay(int i, float d) { env_[i].decay = d; }
  void setDepth(int i, float depth) { env_[i].depth = depth; }

  void setAllRates(float a, float d) {
    for (auto& v : env_) {
      v.attack = a;
      v.decay = d;
    }
  }
  void setAllDepth(float depth) {
    for (auto& v : env_) v.depth = depth;
  }

  // Bulk set: all per-env configs at once
  void setEnvs(const std::array<EnvCfg, N>& cfgs) { env_ = cfgs; }

  // ------ triggering ------
  void trigger(int i) {
    if (cfg_.resetToZeroOnTrigger) values_[i] = 0.0f;
    states_[i] = State::Attack;
  }
  void triggerAll() {
    if (cfg_.resetToZeroOnTrigger) values_.fill(0.0f);
    states_.fill(State::Attack);
  }

  // ------ processing (all envelopes) ------
  // A: y += a*(1 - y);  D: y -= d*y
  void update() {
    for (int i = 0; i < N; ++i) {
      float& y = values_[i];
      switch (states_[i]) {
        case State::Idle:
          break;

        case State::Attack: {
          const float a = env_[i].attack;
          y += a;
          if (y >= cfg_.peakThresh) {
            y = 1.0f;
            states_[i] = State::Decay;
          }
        } break;

        case State::Decay: {
          const float d = env_[i].decay;
          y -= d;
          if (y <= cfg_.floorThresh) {
            y = 0.0f;
            states_[i] = State::Idle;
          }
        } break;
      }
      // internal safety clamp; remove if you prefer raw math
      y = std::clamp(y, 0.0f, 1.0f);
    }
  }

  // ------ queries / maintenance ------
  float value(int i) const {
    return values_[i] * env_[i].depth;
  }  // depth-applied
  float valueRaw(int i) const { return values_[i]; }  // pre-depth 0..1
  bool isActive(int i) const { return states_[i] != State::Idle; }

  void resetAll() {
    values_.fill(0.0f);
    states_.fill(State::Idle);
  }

  Cfg& cfg() { return cfg_; }
  const Cfg& cfg() const { return cfg_; }

 private:
  enum class State : uint8_t { Idle, Attack, Decay };

  Cfg cfg_;
  std::array<EnvCfg, N> env_;
  std::array<float, N> values_;
  std::array<State, N> states_;
};

}  // namespace zlkm