#pragma once

#include <assert.h>
#include <math.h>

#include "audio/processors/DJFilter.h"
#include "dsp/Util.h"
#include "math/Util.h"
#include "mod/ADEnvelopes.h"

// Desired usage:
// using TraitName = ZLKM_PARAM_TRAITS(Type, Min, Max, Default);
// e.g.:
// ParameterSpecs.h
// using CutoffTraits = ZLKM_PARAM_TRAITS(float, 20.f, 20000.f, 440.f);
// using ResonanceTraits = ZLKM_PARAM_TRAITS(float, 0.1f, 10.f, 1.f);
// UI.h/_impl.hpp:
// page[x].mappers[y] = CutoffTraits::makeMapper(&cfg.cutoff);
// Modulation usage:
// auto slot1 = modMatrix.addModulation(envelopes[1],
// CutoffTraits::makeModifier(&cfg.cutoff)); auto slot2 =
// modMatrix.addModulation(lfos[2], CutoffTraits::makeModifier(&cfg.cutoff));
// auto slot3 = modMatrix.addModulation(cvs[0], slot2.depthModifier());
// auto slot4 = modMatrix.addModulation(lfos[1], lfos[2].freqModifier());

namespace zlkm::mod {

class ParamInputMapper {
 public:
  using RawValue = int16_t;
  using ValueToSetRaw = void*;
  using MapAndSetFuncRaw = void (*)(RawValue, ValueToSetRaw);
  using RevMapFuncRaw = RawValue (*)(ValueToSetRaw);
  static const RawValue kMaxRawValue = 4095;

  ParamInputMapper() = default;
  ParamInputMapper(MapAndSetFuncRaw func, RevMapFuncRaw revFunc,
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

class ParamModulator {
 public:
  using ModValue = float;
  using ValueToSetRaw = void*;
  using ModAndSetFuncRaw = void (*)(ModValue, ValueToSetRaw);

  ParamModulator() = default;
  ParamModulator(ModAndSetFuncRaw func, ValueToSetRaw valToSet)
      : modFunc_(func), valToSet_(valToSet) {
    assert(func != nullptr);
    assert(valToSet_ != nullptr);
  }

  void modAndSet(ModValue value) { modFunc_(value, valToSet_); }

 private:
  static constexpr void noopMod_(ModValue, ValueToSetRaw) { /* no-op */ }

  ModAndSetFuncRaw modFunc_ = noopMod_;
  ValueToSetRaw valToSet_ = nullptr;
};

template <class ParamT, class HandlerT>
class ParamMapModMixin {
 public:
  using IM = ParamInputMapper;
  using MOD = ParamModulator;
  // Factory: pass a pointer to the target parameter to receive mapped value
  static IM mapper(ParamT* target) {
    return IM(&mapFunc_, reverseMapFunc_, target);
  }

  MOD modulator(ParamT* target) { return MOD(&modFunc_, target); }

 private:
  static void mapFunc_(int16_t value, void* cfg) {
    HandlerT::mapAndSet(value, *reinterpret_cast<ParamT*>(cfg));
  }

  static IM::RawValue reverseMapFunc_(void* cfg) {
    return HandlerT::reverseMap(*reinterpret_cast<ParamT*>(cfg));
  }

  static void modFunc_(float value, void* cfg) {
    HandlerT::modAndSet(value, *reinterpret_cast<ParamT*>(cfg));
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
  return math::clamp01(y) / reducedRangeScale;
}

template <class T, class Lim>
class LinearMapMod : public ParamMapModMixin<T, LinearMapMod<T, Lim>> {
  using IM = ParamInputMapper;

 public:
  static void mapAndSet(int16_t raw, T& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    out = Lim::outMin() + x * (Lim::range());
  }

  static int16_t reverseMap(const T& in) {
    float x = (in - Lim::outMin()) / (Lim::range());
    x = invStickEnds(x);
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return static_cast<int16_t>(x * float(IM::kMaxRawValue));
  }

  static void modAndSet(float modValue, T& out) {
    out = Lim::clamp(out + modValue * Lim::range());
  }
};

// dB-range mapper: MIN..MAX in dB mapped to linear amplitude
template <class Lim>
class DbMapMod : public ParamMapModMixin<float, DbMapMod<Lim>> {
  using IM = ParamInputMapper;

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
    float x = (dB - Lim::outMin()) / (Lim::range());
    if (x < 0.f) x = 0.f;
    if (x > 1.f) x = 1.f;
    return static_cast<int16_t>(x * float(IM::kMaxRawValue));
  }

  static void modAndSet(float modValue, float& out) {
    // modValue is in dB units
    float dB = 20.f * log10f(out);
    dB += modValue * Lim::range();
    out = Lim::clamp(powf(10.f, dB * 0.05f));
  }
};

// Envelope rate mapper: 0..1 -> [msMin..msMax] -> rate given SR
template <class Lim, int SR>
class RateMapMod : public ParamMapModMixin<float, RateMapMod<Lim, SR>> {
  using IM = ParamInputMapper;

 public:
  static void mapAndSet(int16_t raw, float& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    out = Lim::outMin() + x * (Lim::range());
  }

  static int16_t reverseMap(const float& in) {
    float x = (in - Lim::outMin()) / (Lim::range());
    return int16_t(x * float(IM::kMaxRawValue));
  }

  static void modAndSet(float modValue, float& out) {
    // modValue is in ms units
    out = Lim::clamp(out + modValue * (Lim::range()));
  }
};

// Integer mapper (nearest) in [outMin..outMax]
template <class Lim>
class IntMapMod : public ParamMapModMixin<int, IntMapMod<Lim>> {
  using IM = ParamInputMapper;

 public:
  static void mapAndSet(int16_t raw, int& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    float v = Lim::outMin() + x * Lim::range();
    int iv = static_cast<int>(v + (v >= 0 ? 0.5f : -0.5f));
    out = Lim::clamp(iv);
  }

  static int16_t reverseMap(const int& in) {
    float x = (static_cast<float>(in) - Lim::outMin()) / (Lim::range());
    x = invStickEnds(x);
    return int16_t(x * float(IM::kMaxRawValue));
  }

  static void modAndSet(float modValue, int& out) {
    float v = static_cast<float>(out) + modValue * (Lim::range());
    int iv = static_cast<int>(v + (v >= 0 ? 0.5f : -0.5f));
    out = Lim::clamp(iv);
  }
};

// Boolean threshold mapper
template <class Thr>
class BoolMapMod : public ParamMapModMixin<bool, BoolMapMod<Thr>> {
  using IM = ParamInputMapper;

 public:
  static void mapAndSet(int16_t raw, bool& out) {
    float x = float(raw) / float(IM::kMaxRawValue);
    x = stickEnds(x);
    out = (x >= Thr::thresh());
  }

  static int16_t reverseMap(const bool& in) {
    return in ? IM::kMaxRawValue : 0;
  }

  static void modAndSet(float modValue, bool& out) {
    if (modValue >= Thr::thresh()) {
      out = !out;
    }
  }
};

// Envelope input mapper: maps UI controls to EnvCfg fields
struct EnvCurveMapper {
  using IM = ParamInputMapper;
  // Curve [0..1]: interpolate cLin,cSquare across
  // Log(2,-1)->Lin(1,0)->Exp(0,1)
  static IM make(mod::EnvCfg& e) {
    using namespace zlkm::mod;
    return IM(
        [](int16_t raw, IM::ValueToSetRaw p) {
          auto* curve = reinterpret_cast<EnvCurve*>(p);
          curve->setCurve01(float(raw) / float(IM::kMaxRawValue));
        },
        [](IM::ValueToSetRaw p) {
          auto* curve = reinterpret_cast<EnvCurve*>(p);
          return int16_t((curve->getCurve01() * float(IM::kMaxRawValue)));
        },
        &e.curve);
  }
};
}  // namespace zlkm::mod

// functions instead of constants here to be able to define it in function scope
// Names chosen so no macro clashes occur
#define ZLKM_DEFINE_MIN_MAX_DEFAULT(TYPE, LMIN, LMAX, LDEFAULT)     \
  struct {                                                          \
    static constexpr TYPE outMin() { return (LMIN); }               \
    static constexpr TYPE outMax() { return (LMAX); }               \
    static constexpr TYPE defaultValue() { return (LDEFAULT); }     \
    static constexpr TYPE range() { return (outMax() - outMin()); } \
    static constexpr TYPE clamp(TYPE v) {                           \
      return ::zlkm::math::clamp<TYPE>(v, outMin(), outMax());      \
    }                                                               \
  }

#define ZLKM_DEFINE_MIN_MAX(TYPE, LMIN, LMAX) \
  ZLKM_DEFINE_MIN_MAX_DEFAULT(TYPE, LMIN, LMAX, LMIN)

#define ZLKM_DEFINE_FMIN_MAX(LMIN, LMAX) ZLKM_DEFINE_MIN_MAX(float, LMIN, LMAX)

#define ZLKM_MIN_MAX_DEFAULT_LIN_PARAM(TYPE, LMIN, LMAX, LDEFAULT, NAME) \
  using NAME##Limits =                                                   \
      ZLKM_DEFINE_MIN_MAX_DEFAULT(TYPE, LMIN, LMAX, LDEFAULT);           \
  using NAME = ::zlkm::mod::LinearMapper<float, NAME##Limits>;

// Linear float mapper factory (usage: ZLKM_UI_LIN_FMAPPER(min,max)(&cfg))
#define ZLKM_UI_LIN_FMAPPER(MIN, MAX, VAL)                      \
  [&]() {                                                       \
    using FLim = ZLKM_DEFINE_FMIN_MAX(MIN, MAX);                \
    return ::zlkm::mod::LinearMapMod<float, FLim>::mapper(VAL); \
  }()

#define ZLKM_UI_EXP_FMAPPER(MIN, MAX, VAL) \
  ZLKM_UI_EXP_FMAPPER_EX(MIN, MAX, 1.6f, VAL)

// dB-range mapper factory (usage: ZLKM_UI_DB_FMAPPER(dbMin,dbMax)(&cfg))
#define ZLKM_UI_DB_FMAPPER(DB_MIN, DB_MAX, VAL)         \
  [&]() {                                               \
    using DBLim = ZLKM_DEFINE_FMIN_MAX(DB_MIN, DB_MAX); \
    return ::zlkm::mod::DbMapMod<DBLim>::make(VAL);     \
  }()

// Rate mapper factory
#define ZLKM_UI_RATE_FMAPPER(MS_MIN, MS_MAX, SR, VAL)                    \
  [&]() {                                                                \
    using RateL = ZLKM_DEFINE_FMIN_MAX(zlkm::dsp::msToRate(MS_MIN, SR),  \
                                       zlkm::dsp::msToRate(MS_MAX, SR)); \
    return ::zlkm::mod::RateMapMod<RateL, (SR)>::mapper(VAL);            \
  }()

// Integer mapper factory
#define ZLKM_UI_INT_MAPPER(MIN, MAX, VAL)             \
  [&]() {                                             \
    using IntL = ZLKM_DEFINE_MIN_MAX(int, MIN, MAX);  \
    return ::zlkm::mod::IntMapMod<IntL>::mapper(VAL); \
  }()

// Boolean threshold mapper factory
#define ZLKM_UI_BOOL_MAPPER_TH(THRESHOLD, VAL)                       \
  [&]() {                                                            \
    struct _BoolThr##__LINE__ {                                      \
      static constexpr float thresh() { return (THRESHOLD); }        \
    };                                                               \
    return ::zlkm::mod::BoolMapMod<_BoolThr##__LINE__>::mapper(VAL); \
  }()

#define ZLKM_UI_BOOL_MAPPER(VAL) ZLKM_UI_BOOL_MAPPER_TH(0.5f, VAL)
