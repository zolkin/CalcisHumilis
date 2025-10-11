#pragma once

#include <assert.h>
#include <math.h>

#include "audio/DJFilter.h"
#include "dsp/Util.h"

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
    x = stickEnds(x);
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
    float x = (in - Lim::outMin()) / (Lim::outMax() - Lim::outMin());
    x = stickEnds(x);
    x = powf(x, 1.0f / ExpPolicy::EXP);
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
    float dB = 20.f * log10f(in);
    float x = (dB - Lim::outMin()) / (Lim::outMax() - Lim::outMin());
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
    float ms = MsLim::msMin() + x * (MsLim::msMax() - MsLim::msMin());
    out = zlkm::dsp::msToRate(ms, float(SR));
  }

  static int16_t reverseMap(const float& in) {
    float ms = zlkm::dsp::rateToMs(in, static_cast<float>(SR));
    float x = (ms - MsLim::msMin()) / (MsLim::msMax() - MsLim::msMin());
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
    x = stickEnds(x);
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

}  // namespace zlkm::ui

// Linear float mapper factory (usage: ZLKM_UI_LIN_FMAPPER(min,max)(&cfg))
#define ZLKM_UI_LIN_FMAPPER(MIN, MAX, VAL)                                \
  [&]() {                                                                 \
    struct _Limits##__LINE__ {                                            \
      static constexpr float outMin() { return (MIN); }                   \
      static constexpr float outMax() { return (MAX); }                   \
    };                                                                    \
    return ::zlkm::ui::LinearMapper<float, _Limits##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_EXP_FMAPPER(MIN, MAX, VAL)                         \
  [&]() {                                                          \
    struct _LimitsE##__LINE__ {                                    \
      static constexpr float outMin() { return (MIN); }            \
      static constexpr float outMax() { return (MAX); }            \
    };                                                             \
    struct _ExpPol##__LINE__ {                                     \
      static constexpr float EXP = 1.6f;                           \
    };                                                             \
    return ::zlkm::ui::ExpPowMapper<float, _LimitsE##__LINE__,     \
                                    _ExpPol##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_EXP_FMAPPER_EX(MIN, MAX, EXPONENT, VAL)             \
  [&]() {                                                           \
    struct _LimitsE2##__LINE__ {                                    \
      static constexpr float outMin() { return (MIN); }             \
      static constexpr float outMax() { return (MAX); }             \
    };                                                              \
    struct _ExpPol2##__LINE__ {                                     \
      static constexpr float EXP = (EXPONENT);                      \
    };                                                              \
    return ::zlkm::ui::ExpPowMapper<float, _LimitsE2##__LINE__,     \
                                    _ExpPol2##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_DB_FMAPPER(DB_MIN, DB_MAX, VAL)             \
  [&]() {                                                   \
    struct _DbL##__LINE__ {                                 \
      static constexpr float outMin() { return (DB_MIN); }  \
      static constexpr float outMax() { return (DB_MAX); }  \
    };                                                      \
    return ::zlkm::ui::DbMapper<_DbL##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_RATE_FMAPPER(MS_MIN, MS_MAX, SR, VAL)                 \
  [&]() {                                                             \
    struct _RateL##__LINE__ {                                         \
      static constexpr float msMin() { return (MS_MIN); }             \
      static constexpr float msMax() { return (MS_MAX); }             \
    };                                                                \
    return ::zlkm::ui::RateMapper<_RateL##__LINE__, (SR)>::make(VAL); \
  }()

#define ZLKM_UI_INT_MAPPER(MIN, MAX, VAL)                     \
  [&]() {                                                     \
    struct _IntL##__LINE__ {                                  \
      static constexpr float outMin() { return (MIN); }       \
      static constexpr float outMax() { return (MAX); }       \
    };                                                        \
    return ::zlkm::ui::IntMapper<_IntL##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_BOOL_MAPPER_TH(THRESHOLD, VAL)                    \
  [&]() {                                                         \
    struct _BoolThr##__LINE__ {                                   \
      static constexpr float thresh() { return (THRESHOLD); }     \
    };                                                            \
    return ::zlkm::ui::BoolMapper<_BoolThr##__LINE__>::make(VAL); \
  }()

#define ZLKM_UI_BOOL_MAPPER(VAL) ZLKM_UI_BOOL_MAPPER_TH(0.5f, VAL)

// Filter mapper macros: pass pointer to DJFilterTPT<SR>::Cfg
#define ZLKM_UI_FILTER_CUT_MAPPER(SR, MIN01, MAX01, CFG_PTR)                \
  [&]() {                                                                   \
    struct _FL##__LINE__ {                                                  \
      static constexpr float outMin() { return (MIN01); }                   \
      static constexpr float outMax() { return (MAX01); }                   \
    };                                                                      \
    return ::zlkm::ui::FilterCutMapper<(SR), _FL##__LINE__>::make(CFG_PTR); \
  }()

#define ZLKM_UI_FILTER_RES_MAPPER(SR, MIN01, MAX01, CFG_PTR)                \
  [&]() {                                                                   \
    struct _FR##__LINE__ {                                                  \
      static constexpr float outMin() { return (MIN01); }                   \
      static constexpr float outMax() { return (MAX01); }                   \
    };                                                                      \
    return ::zlkm::ui::FilterResMapper<(SR), _FR##__LINE__>::make(CFG_PTR); \
  }()

#define ZLKM_UI_FILTER_MORPH_MAPPER(SR, MIN01, MAX01, CFG_PTR)                \
  [&]() {                                                                     \
    struct _FM##__LINE__ {                                                    \
      static constexpr float outMin() { return (MIN01); }                     \
      static constexpr float outMax() { return (MAX01); }                     \
    };                                                                        \
    return ::zlkm::ui::FilterMorphMapper<(SR), _FM##__LINE__>::make(CFG_PTR); \
  }()

#define ZLKM_UI_FILTER_DRIVE_MAPPER(SR, MIN01, MAX01, CFG_PTR)                \
  [&]() {                                                                     \
    struct _FD##__LINE__ {                                                    \
      static constexpr float outMin() { return (MIN01); }                     \
      static constexpr float outMax() { return (MAX01); }                     \
    };                                                                        \
    return ::zlkm::ui::FilterDriveMapper<(SR), _FD##__LINE__>::make(CFG_PTR); \
  }()
