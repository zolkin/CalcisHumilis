#pragma once

#include <assert.h>
#include <math.h>

#include "audio/DJFilter.h"
#include "dsp/Util.h"
#include "mod/ADEnvelopes.h"

namespace zlkm::ui {

class InputMapper {
 public:
  using RawValue = int16_t;
  using ValueToSetRaw = void*;
  using MapAndSetFuncRaw = void (*)(RawValue, ValueToSetRaw);
  using RevMapFuncRaw = RawValue (*)(ValueToSetRaw);
  static const RawValue kMaxRawValue = 4095;

  InputMapper() = default;
  InputMapper(MapAndSetFuncRaw func, RevMapFuncRaw revFunc,
              ValueToSetRaw valToSet)
      : mapFunc_(func), revMapFunc_(revFunc), valToSet_(valToSet) {
    assert(func != nullptr);
    assert(revFunc != nullptr);
    assert(valToSet_ != nullptr);
  }

  void mapAndSet(int16_t value) { mapFunc_(value, valToSet_); }
  RawValue reverseMap() const { return revMapFunc_(valToSet_); }

 private:
  static constexpr void noopMap_(RawValue, ValueToSetRaw) { /* no-op */ }
  static constexpr RawValue noopRevMap_(ValueToSetRaw) {
    return kMaxRawValue / 2;
  }

  MapAndSetFuncRaw mapFunc_ = noopMap_;
  RevMapFuncRaw revMapFunc_ = noopRevMap_;
  ValueToSetRaw valToSet_ = nullptr;
};

template <class ParamT, class HandlerT>
class InputMapperMixin {
 public:
  using IM = InputMapper;
  // Factory: pass a pointer to the target parameter to receive mapped value
  static IM make(ParamT* target) {
    return IM(&mapFunc_, reverseMapFunc_, target);
  }

 private:
  static void mapFunc_(int16_t value, void* cfg) {
    HandlerT::mapAndSet(value, *reinterpret_cast<ParamT*>(cfg));
  }

  static IM::RawValue reverseMapFunc_(void* cfg) {
    return HandlerT::reverseMap(*reinterpret_cast<ParamT*>(cfg));
  }
};

// Sticky ends shaping (keeps edges easy to hit on physical controls)
static inline float stickEnds(float f) {
  static constexpr float stickyEnds = 0.05f;
  static constexpr float reducedRangeScale = 1.f / (1.f - 2.f * stickyEnds);
  if (f < stickyEnds) return 0.f;
  if (f > 1.f - stickyEnds) return 1.f;
  return f * reducedRangeScale;
}

// Inverse of stickEnds() for the central region; clamps at edges
static inline float invStickEnds(float y) {
  static constexpr float stickyEnds = 0.05f;
  static constexpr float reducedRangeScale = 1.f / (1.f - 2.f * stickyEnds);
  if (y <= 0.f) return 0.f;
  if (y >= 1.f) return 1.f;
  return y / reducedRangeScale;
}

template <class T, class Lim>
class LinearMapper : public InputMapperMixin<T, LinearMapper<T, Lim>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, T& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    out = Lim::outMin() + x * (Lim::outMax() - Lim::outMin());
  }

  static int16_t reverseMap(const T& in) {
    float x = (in - Lim::outMin()) / (Lim::outMax() - Lim::outMin());
    x = invStickEnds(x);
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return static_cast<int16_t>(x * float(IM::kMaxRawValue));
  }
};

// Exponential (power) mapper
template <class T, class Lim, class ExpPolicy>
class ExpPowMapper
    : public InputMapperMixin<T, ExpPowMapper<T, Lim, ExpPolicy>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, T& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = powf(x, ExpPolicy::EXP);
    x = stickEnds(x);
    out = Lim::outMin() + x * (Lim::outMax() - Lim::outMin());
  }
  static int16_t reverseMap(const T& in) {
    // Forward: raw->x -> pow -> stickEnds -> scale
    float y = (in - Lim::outMin()) / (Lim::outMax() - Lim::outMin());
    float z = invStickEnds(y);
    float x = powf(z, 1.0f / ExpPolicy::EXP);
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return int16_t(x * float(IM::kMaxRawValue));
  }
};

// dB-range mapper: MIN..MAX in dB mapped to linear amplitude
template <class Lim>
class DbMapper : public InputMapperMixin<float, DbMapper<Lim>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, float& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    float dB = Lim::outMin() + x * (Lim::outMax() - Lim::outMin());
    out = powf(10.f, dB * 0.05f);
  }

  static int16_t reverseMap(const float& in) {
    // Guard against non-positive input
    float amp = in <= 0.f ? powf(10.f, Lim::outMin() * 0.05f) : in;
    float dB = 20.f * log10f(amp);
    float x = (dB - Lim::outMin()) / (Lim::outMax() - Lim::outMin());
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return static_cast<int16_t>(x * float(IM::kMaxRawValue));
  }
};

// Envelope rate mapper: 0..1 -> [msMin..msMax] -> rate given SR
template <class MsLim, int SR>
class RateMapper : public InputMapperMixin<float, RateMapper<MsLim, SR>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, float& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    float ms = MsLim::outMin() + x * (MsLim::outMax() - MsLim::outMin());
    out = zlkm::dsp::msToRate(ms, float(SR));
  }

  static int16_t reverseMap(const float& in) {
    float ms = zlkm::dsp::rateToMs(in, static_cast<float>(SR));
    float x = (ms - MsLim::outMin()) / (MsLim::outMax() - MsLim::outMin());
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return int16_t(x * float(IM::kMaxRawValue));
  }
};

// Integer mapper (nearest) in [outMin..outMax]
template <class Lim>
class IntMapper : public InputMapperMixin<int, IntMapper<Lim>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, int& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    float v = Lim::outMin() + x * (Lim::outMax() - Lim::outMin());
    int iv = static_cast<int>(v + (v >= 0 ? 0.5f : -0.5f));
    int lo = static_cast<int>(Lim::outMin());
    int hi = static_cast<int>(Lim::outMax());
    if (iv < lo) iv = lo;
    if (iv > hi) iv = hi;
    out = iv;
  }

  static int16_t reverseMap(const int& in) {
    float x = (static_cast<float>(in) - Lim::outMin()) /
              (Lim::outMax() - Lim::outMin());
    x = invStickEnds(x);
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return int16_t(x * float(IM::kMaxRawValue));
  }
};

// Boolean threshold mapper
template <class Thr>
class BoolMapper : public InputMapperMixin<bool, BoolMapper<Thr>> {
  using IM = InputMapper;

 public:
  static void mapAndSet(int16_t raw, bool& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    out = (x >= Thr::thresh());
  }

  static int16_t reverseMap(const bool& in) {
    return in ? IM::kMaxRawValue : 0;
  }
};

template <int SR>
struct FilterMapper {
  using IM = InputMapper;
  using ParamT = typename ::zlkm::audio::SafeFilterParams<SR>;

  static IM makeCutoff(ParamT& t) {
    struct SetCutoff {
      static void set(ParamT& p, float x) { p.setCutoff01(x); }
      static float get(const ParamT& p) { return p.cutoff01(); }
    };
    return makeMapper_<SetCutoff>(t);
  }

  static IM makeResonance(ParamT& t) {
    struct SetResonance {
      static void set(ParamT& p, float x) { p.setRes01(x); }
      static float get(const ParamT& p) { return p.res01(); }
    };
    return makeMapper_<SetResonance>(t);
  }

  static IM makeMorph(ParamT& t) {
    struct SetMorph {
      static void set(ParamT& p, float x) { p.setMorph01(x); }
      static float get(const ParamT& p) { return p.morph01(); }
    };
    return makeMapper_<SetMorph>(t);
  }

  static IM makeDrive(ParamT& t) {
    struct SetDrive {
      static void set(ParamT& p, float x) { p.setDrive01(x); }
      static float get(const ParamT& p) { return p.drive01(); }
    };
    return makeMapper_<SetDrive>(t);
  }

 private:
  template <class ParamSetter>
  static IM makeMapper_(ParamT& target) {
    return IM(
        [](int16_t raw, IM::ValueToSetRaw params) {
          float x = float(raw) / float(IM::kMaxRawValue);
          x = stickEnds(x);
          ParamSetter::set(*reinterpret_cast<ParamT*>(params), x);
        },
        [](IM::ValueToSetRaw params) {
          float v = ParamSetter::get(*reinterpret_cast<ParamT*>(params));
          return int16_t((v * float(IM::kMaxRawValue)));
        },
        &target);
  }
};

// Envelope input mapper: maps UI controls to EnvCfg fields
struct EnvCurveMapper {
  using IM = InputMapper;
  // Curve [0..1]: interpolate cLin,cSquare across Log(2,-1)->Lin(1,0)->Exp(0,1)
  static IM make(mod::EnvCfg& e) {
    using namespace zlkm::mod;
    return IM(
        [](int16_t raw, IM::ValueToSetRaw p) {
          auto* curve = reinterpret_cast<EnvCurve*>(p);
          curve->setCurve01(float(raw) / float(IM::kMaxRawValue));
        },
        [](IM::ValueToSetRaw p) {
          auto* curve = reinterpret_cast<EnvCurve*>(p);
          return int16_t((curve->getShape() * float(IM::kMaxRawValue)));
        },
        &e.curve);
  }
};
}  // namespace zlkm::ui

// Helper macros to define limit and exponent structs for mappers
#define _ZLKM_MIN_MAX_NAME(NAME) _Limits##NAME
#define _ZLKM_EXP_NAME(NAME) _ExpPol##NAME

#define _ZLKM_DEFINE_MIN_MAX(LMIN, LMAX, NAME)         \
  struct _ZLKM_MIN_MAX_NAME(NAME) {                    \
    static constexpr float outMin() { return (LMIN); } \
    static constexpr float outMax() { return (LMAX); } \
  }

#define _ZLKM_DEFINE_EXP(EXP, NAME)     \
  struct _ZLKM_EXP_NAME(NAME) {         \
    static constexpr float EXP = (EXP); \
  };

// Linear float mapper factory (usage: ZLKM_UI_LIN_FMAPPER(min,max)(&cfg))
#define ZLKM_UI_LIN_FMAPPER(MIN, MAX, VAL)                                  \
  [&]() {                                                                   \
    _ZLKM_DEFINE_MIN_MAX(MIN, MAX, FLIN);                                   \
    return ::zlkm::ui::LinearMapper<float, _ZLKM_MIN_MAX_NAME(FLIN)>::make( \
        VAL);                                                               \
  }()

// Exponential float mapper factory (usage: ZLKM_UI_EXP_FMAPPER(min,max)(&cfg))
#define ZLKM_UI_EXP_FMAPPER_EX(MIN, MAX, EXP, VAL)                    \
  [&]() {                                                             \
    _ZLKM_DEFINE_MIN_MAX(MIN, MAX, FEXP);                             \
    _ZLKM_DEFINE_EXP(EXP, FEXP);                                      \
    return ::zlkm::ui::ExpPowMapper<float, _ZLKM_MIN_MAX_NAME(FEXP),  \
                                    _ZLKM_EXP_NAME(FEXP)>::make(VAL); \
  }()

#define ZLKM_UI_EXP_FMAPPER(MIN, MAX, VAL) \
  ZLKM_UI_EXP_FMAPPER_EX(MIN, MAX, 1.6f, VAL)

// dB-range mapper factory (usage: ZLKM_UI_DB_FMAPPER(dbMin,dbMax)(&cfg))
#define ZLKM_UI_DB_FMAPPER(DB_MIN, DB_MAX, VAL)                       \
  [&]() {                                                             \
    _ZLKM_DEFINE_MIN_MAX(DB_MIN, DB_MAX, DB_L);                       \
    return ::zlkm::ui::DbMapper<_ZLKM_MIN_MAX_NAME(DB_L)>::make(VAL); \
  }()

// Rate mapper factory
#define ZLKM_UI_RATE_FMAPPER(MS_MIN, MS_MAX, SR, VAL)                          \
  [&]() {                                                                      \
    _ZLKM_DEFINE_MIN_MAX(MS_MIN, MS_MAX, RateL);                               \
    return ::zlkm::ui::RateMapper<_ZLKM_MIN_MAX_NAME(RateL), (SR)>::make(VAL); \
  }()

// Integer mapper factory
#define ZLKM_UI_INT_MAPPER(MIN, MAX, VAL)                              \
  [&]() {                                                              \
    _ZLKM_DEFINE_MIN_MAX(MIN, MAX, IntL);                              \
    return ::zlkm::ui::IntMapper<_ZLKM_MIN_MAX_NAME(IntL)>::make(VAL); \
  }()

// Boolean threshold mapper factory
#define ZLKM_UI_BOOL_MAPPER_TH(THRESHOLD, VAL)                    \
  [&]() {                                                         \
    struct _BoolThr##__LINE__ {                                   \
      static constexpr float thresh() { return (THRESHOLD); }     \
    };                                                            \
    return ::zlkm::ui::BoolMapper<_BoolThr##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_BOOL_MAPPER(VAL) ZLKM_UI_BOOL_MAPPER_TH(0.5f, VAL)
