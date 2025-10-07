#pragma once

#include <Arduino.h>
#include <JLED.h>
#include <U8g2lib.h>

#include <array>
#include <tuple>
#include <type_traits>

#include "hw/Screen.h"
#include "hw/io/ButtonManager.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"  // aliases: ScreenSSD, TabButtons, PinExpander

namespace zlkm::ui {

template <class Selection>
class View {
 public:
  using ScreenT = zlkm::ch::ScreenSSD;
  using PinExpander = zlkm::ch::PinExpander;
  using TabButtons = zlkm::ch::TabButtons;

  struct Cfg {
    TabButtons::Cfg tabBtns;
    uint32_t fps = 60;
  };

  View(PinExpander* pins, Selection* selection, const Cfg& cfg)
      : pinExp_(*pins),
        tabBtns_(pinExp_, cfg.tabBtns),
        screen_({}),
        selection_(selection),
        cfg_(cfg) {
    // map LEDs to pins 0..3 on expander for example
    for (int i = 0; i < 4; ++i) tabLedPins_[i] = i;
    pinExp_.writeAll(0);
    lastUpdateMs_ = millis();
    updateLeds_();
  }

  // Call on UI core at free cadence; internally gated to ~fps
  void update() {
    const uint32_t now = millis();
    const uint32_t interval = 1000 / (cfg_.fps ? cfg_.fps : 60);
    if (now - lastUpdateMs_ < interval) return;
    lastUpdateMs_ = now;

    // Buttons for tab/page
    auto rep = tabBtns_.tick();
    for (int i = 0; i < (int)rep.rising.size(); ++i) {
      if (!rep.rising.test(i)) continue;
      if ((uint8_t)i == selection_->currentTab.load()) {
        // advance page in the current tab
        advancePage_();
      } else {
        selection_->currentTab.store((uint8_t)i);
        updateLeds_();
      }
    }

    // Render
    screen_.update([&](U8G2& g) {
      g.setFont(u8g2_font_6x12_tf);
      g.drawStr(0, 12, "Calcis Humilis");
      g.drawFrame(0, 0, screen_.width(), screen_.height());
    });
  }

 private:
  // Tuple visitation helper by runtime index
  template <typename Tuple, typename F, size_t... Is>
  static inline bool visitTupleAt_(Tuple& t, size_t i, F&& f,
                                   std::index_sequence<Is...>) {
    return ((i == Is ? (f(std::get<Is>(t)), true) : false) || ...);
  }
  template <typename Tuple, typename F>
  static inline bool visitTupleAt(Tuple& t, size_t i, F&& f) {
    return visitTupleAt_(
        t, i, std::forward<F>(f),
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
  }

  void advancePage_() {
    const uint8_t tab = selection_->currentTab.load();
    // Determine number of pages for current tab and increment
    visitTupleAt(selection_->tabs, tab, [&](auto& t) {
      using TabT = std::decay_t<decltype(t)>;
      constexpr uint8_t kPages = std::tuple_size_v<typename TabT::PagesTuple>;
      uint8_t cur = t.currentPage.load();
      if constexpr (kPages > 0) {
        cur = (uint8_t)((cur + 1) % kPages);
      } else {
        cur = 0;
      }
      t.currentPage.store(cur);
    });
    updateLeds_();
  }

  void updateLeds_() {
    const uint8_t active = selection_->currentTab.load();
    for (uint8_t i = 0; i < 4; ++i)
      pinExp_.writePin(tabLedPins_[i], i == active);
  }

  PinExpander& pinExp_;
  TabButtons tabBtns_;
  ScreenT screen_;
  Selection* selection_;
  Cfg cfg_{};
  uint32_t lastUpdateMs_ = 0;
  std::array<uint8_t, 4> tabLedPins_{};
};

}  // namespace zlkm::ui
