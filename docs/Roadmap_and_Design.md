# CalcisHumilis Roadmap and Design (Oct 15, 2025)

This document turns the high-level plan into concrete, code-referenced steps. It’s intended to guide small, incremental PR-sized changes.

## 1) Board portability and feature sets

Goal: Prepare for multiple boards without churn: RP2350 (Olimex Pico2 XL/XXL) and WeAct STM32H562.

What we already have
- Board abstraction with per-board definitions: `src/platform/boards/pico2.h` (PinDefs and PinSource via PinMux) and selector `src/platform/boards/Current.h` used by `UI`, `View`, and `Screen`.
- Platform abstraction for time: `src/platform/platform.h` with millis/micros shims.

Planned structure (minimize ifdefs)
- Board profiles (compile-time):
  - [x] Add `src/platform/boards/pico2.h` with role-based constants (PinDefs) and a PinSource (PinMux of GpioPins + MCP expander).
  - [x] Add `src/platform/boards/olimex_pico2xl.h` with GPIO-only PinSource and per-board pin roles.
  - [ ] Add `src/platform/boards/stm32h562.h` with equivalent PinDefs and PinSource (likely GPIO-only initially).
  - [x] Selector header: `src/platform/boards/Current.h` aliases the active board; make selection configurable via a build flag (e.g., `-DBOARD_STM32H562`), falling back to Pico 2 by default.
  - Optionally, keep a thin compatibility layer for legacy `pins.h` if needed (deprecated).

- Pin handlers / adapters:
  - [x] Keep `ButtonManager` and `QuadManagerIO` unchanged. They operate against a device exposing `setPinMode(pin)`, `readPins<K>(idxs)`, and `writePin(pin, bool)`. We now pass the unified PinSource (PinMux) as the device.
  - ~~For expander removal: swap `using PinExpander = hw::io::Mcp23017Pins` with `hw::io::GpioPins<N>` in `UI` and `View`.~~
    - Replaced with a unified `PinSource` that multiplexes GPIO and expander transparently; `UI` and `View` consume `boards::current::PinSource` and `PinDefs`.

- Extended vs cut-down feature sets (prefer templates over ifdefs):
  - Provide light wrapper types instead of `#if`: e.g., `using ScreenSaversT = ScreenSaversEnabled;` vs `ScreenSaversNoop;` chosen via a single `using` in a small selector header.
  - For profiler: macro already no-ops when `PROFILE` off; keep as-is.
  - For optional LEDs/savers: introduce tiny types with the same interface so call sites remain compile-time polymorphic without scattering `#if`s.

Acceptance / steps
 - [x] Create `src/platform/boards/pico2.h` with PinDefs and PinSource; add `boards/Current.h` selector and use it in modules.
 - [x] Create `src/platform/boards/olimex_pico2xl.h` with GPIO-only PinSource and per-board pin roles.
 - [ ] Create `src/platform/boards/stm32h562.h` with equivalent PinDefs and PinSource.
 - [x] Update `UI.h`/`View.h`/`Screen.h` to use `boards::current::{PinSource, PinDefs}`; raw pins for third-party libs via `.pin()`.
 - [x] Smoke test on hardware (Pico 2): LEDs blink, buttons/encoders tick, U8g2 renders.
 - [x] Smoke test on hardware (Olimex Pico2‑XL): LEDs, buttons, encoders, I2S audio (PCM5100), SSD1309 screen.
 - [ ] Smoke test on hardware (STM32H562): pending once board profile exists.

## 2) Streamline UI-changed parameter processing

Goal: Only propagate and interpolate parameters that changed; fall back to snapshots when the queue is saturated.

Current references
- Controller changes `page.rawPos[i]` and calls `page.mappers[i].mapAndSet(raw)` in `Controller::update()` (see `src/ui/Controller.h`).
- Audio thread consumes a complete config snapshot via `SafeFilterParams` and other fields (see `src/CalcisHumilis_impl.hh`).

Plan
- Change detection ring buffer on UI core:
  - Add a small lock-free (single-producer) ring buffer of param change events: `{param_id, value, timestamp}`.
  - Only push when raw value crosses a quantized threshold to avoid spam (e.g., deltaRaw > K or time-based throttling).
  - If the buffer is full, set a flag to trigger the current fallback (copy full snapshot).

- Audio core consumption:
  - On audio core, drain up to `N` events per block; apply only those changes.
  - If fallback flag observed, consume one full snapshot and clear the flag.
  - Interpolate per-changed parameter over a small window (e.g., 16–64 samples) to smooth steps; unchanged params remain steady.

- Controller loop rate limiting / sync:
  - Add a small scheduler in UI core to cap controller updates to audio block cadence (e.g., one controller update per audio block or per `k` blocks).
  - Options grounded in current code:
    - Time-based: in `UI::update()` (see `src/ui/UI.h`), only call `controller_.update()` when `(now - lastCtlMs) >= ctlPeriodMs` where `ctlPeriodMs` derives from audio block rate.
    - Mid-loop invoke: call `controller_.update()` after `sampler_.update()` but before `view_.update()`; or move to a timer on UI thread if available.

- Minimal interfaces
  - Define param IDs centrally (enum or constexpr table) so both UI and audio agree on IDs.
  - Provide tiny helpers: `applyParamChange(cfg, id, value)` and `interpolateParam(state, id, toValue, frames)`.

Acceptance / steps
- [ ] Add `src/app/ParamChangeQueue.h` (SPSC ring buffer, no heap).
- [ ] Emit changes from `Controller::update()` only when raw crosses quantized steps.
- [ ] Add audio-side consumer (in `CalcisHumilis_impl.hh` loop) to apply up to N changes per block; otherwise use current snapshot mechanism.
- [ ] Interpolate only changed params (existing code already supports smooth filter updates via `SafeFilterParams`; extend similarly for others if needed).

## 3) Consolidate parameter pages

Goal: Pages users understand quickly: source, filter, modulation, FX (optional).

References
- Page wiring currently in `UI::initSpecs()` (see `src/ui/UI.h`).

Plan
- Define four page groups in `initSpecs()`:
  1. Source (Swarm now; later engines will select alternative page sets)
  2. Filter (already in place via `FilterMapper`)
  3. Modulation (envelopes, depth/rate/intensity, targets)
  4. FX (optional; delay/drive/reverb placeholders)
- Keep mappers built with current macros (`ZLKM_UI_*_MAPPER(...)`).

Acceptance / steps
- [ ] Update `initSpecs()` to group parameters per above.
- [ ] Add TODO markers for engine-based conditional pages.

## 4) Per-parameter widgets in UI

Goal: Draw meaningful widgets for the selected parameter(s).

References
- Current view draws four ring bitmaps and textual info (see `src/ui/View.h`).

Plan
- Add widget interfaces inside `View` (virtuals OK here):
  - Either simple free functions, or a tiny abstract base `IWidget` with `virtual void draw(U8G2&, const Rect&) const = 0;` and concrete implementations.
  - Helpers: `drawFilterWidget(U8G2&, const SafeFilterParams&, rect)`, `drawEnvWidget(U8G2&, const EnvState&, rect)`.
  - Start simple: for Filter, draw cutoff dot/line across a log-x axis, and a resonance marker; for Env, draw ADS shape from current rates.
- As a screensaver/default view, consider a small oscilloscope using downsampled audio (UI core) or feedback metrics; guard under `FEAT_SCOPE`.

Acceptance / steps
- [ ] Add widget helpers to `View.h` and call them when not idle.
- [ ] Optional scope screensaver under feature flag.

## 5) New sound engines

Goal: Add FM next, then experimental stereo-bounce.

References
- Current engine: Swarm oscillator (see `src/audio/MorphOsc.h`, used via `UI::initSpecs()` bindings).

Plan
- Abstract engine selection:
  - Introduce `enum class Engine { Swarm, FM, Bounce }` in config.
  - Provide per-engine config structs; the top-level `Calcis::Cfg` holds a tagged union or distinct sub-structs plus an active engine.
- FM engine:
  - Minimal 2–4 operator with fixed algorithm to start; reuse `SafeFilterParams` downstream unchanged.
  - Add mapper pages under “Source” when Engine=FM.
- Stereo-bounce (fun engine):
  - Procedural motion mapping to stereo panning and timbre; fits as a separate “Source” variant.

Acceptance / steps
- [ ] Add `Engine` tag to config.
- [ ] Introduce `audio/engines/FM.h` with a tiny render() integrating into the existing audio loop.
- [ ] Populate UI pages conditionally based on active engine.

## 6) Voltage control (Plaits-style)

Hardware
- Normalization probe: one probe GPIO fanned out through ~10 kΩ per-channel resistors into the CV input nodes, with a diode-based bias/clamp network (as seen in Plaits). This biasing creates a detectable signature on unpatched inputs without requiring switch jacks.
- Single Vref and MCP6004 for buffering/scaling (components on order).

Software
- ADC via DMA if available (RP2350/STM32H5) or polled fallback; double-buffered capture.
- Simple filtering: EMA or small FIR per CV channel; optional decimation.
- Normalization detection: drive a small square wave on the probe GPIO and detect its presence on each CV ADC channel; use correlation or a simpler phase/magnitude check tailored to the diode-bias network; add hysteresis for patched/unpatched.
- Processing in audio loop: push CV-derived param changes into the same ring buffer; respect rate limit.

Acceptance / steps
- [ ] Add `src/hw/cv/CvReader.h` (DMA-capable with polled fallback) + `CvChannelState` with smoothing.
- [ ] Add `src/hw/cv/NormProbe.h` (probe GPIO driver + per-channel correlation and hysteresis; native test with synthetic sequences).
- [ ] Bind one CV channel to a param via existing InputMapper to validate end-to-end.

### VOCT (1 V/Oct) input

Hardware

![VOCT input bias/conditioning](images/voct_input.png)

- VOCT jack is conditioned by a diode-based bias/clamp and resistor network, then buffered by MCP6004 before the ADC (see schematic above).
- Aim for a clean, low-impedance source into the ADC and predictable biasing when unpatched (for normalization detection).

Software
- Calibration: compute linear calibration for ADC → volts: `V = a * adc + b` using two known notes (e.g., C2 and C4) during a simple calibration mode.
- Pitch mapping:
  - Semitone offset: `semitones = 12 * V` (relative to 0V reference), or use `note = note0 + 12 * V`.
  - Frequency: `f = f_ref * 2^V`; integrate with `Calcis::cycles(f)` to set oscillator base frequency (`cyclesPerSample`).
  - Keep smoothing very light or S/H-based to avoid glide; snap when near semitone boundaries if desired.
- Integration: feed VOCT changes into the ParamChangeQueue; interpolate minimally on the audio side for click-free updates.

Acceptance / steps
- [ ] Add `src/app/cv/Voct.h` with calibration state `{a,b}` and helpers `adcToVolts()`, `voltsToHz()`, `voltsToSemitones()`.
- [ ] Add a native test for Voct mapping: two-point calibration → expected semitone mapping and octave linearity.
- [ ] Wire one CvReader channel to update `cfg.cyclesPerSample` via `Calcis::cycles(f)`.

## 7) Ideas / experiments

- Filters: add LP/HP variants and separate Drive into FX; explore self-oscillation paths.
- Input mapping gestures: long-turn acceleration, push-and-turn modes, tab-modifier keys.
- Display: evaluate yellow-scale OLED; abstract `ScreenSSD` creation so pin and controller variants are swappable.

Acceptance / steps
- [ ] Add a second filter preset in UI pages toggled by a simple switch; keep DSP unchanged initially.
- [ ] Add an acceleration curve in `Controller::update()` for large encoder deltas.
- [ ] Add a `Screen` factory hook to select alternate OLED model under a single typedef or small selector.

---

## References (by file)
- `src/platform/pins.h`: current central pins used across UI and Screen.
- `src/platform/platform.h`: millis/micros shims.
- `src/ui/UI.h`: pin usage, initSpecs() page definitions, component wiring.
- `src/ui/View.h`: tab LEDs, ring UI, screensavers, and LED animations.
- `src/ui/Controller.h`: parameter changes and `seedFromCfg()` inverse mapping.
- `src/hw/io/ButtonManager.h`, `src/hw/io/QuadManagerIO.h`: reusable IO primitives.
- `src/audio/DJFilter.h`: filter and `SafeFilterParams` bridge.
- `src/CalcisHumilis_impl.hh`: audio loop integration points for parameter consumption.

## Notes
- Keep native tests (`pio test -e native`) as the fast validation loop. New pieces (ParamChangeQueue, widget helpers) should have focused tests where possible.
- Avoid dynamic allocation and virtuals in hot paths. Prefer compile-time selection and SPSC queues.
