#pragma once

#include <Arduino.h>
#include <JLED.h>
#include <U8g2lib.h>

#include <array>

#include "CalcisHumilis.h"
#include "hw/Screen.h"
#include "hw/screensavers/SaverMux.h"
#include "hw/screensavers/StarField.h"
#include "hw/screensavers/ThroughTheStars.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"  // aliases: ScreenSSD

namespace zlkm::ui {

template <class Selection>
class View {
 public:
  using ScreenT = zlkm::ch::ScreenSSD;
  using ScreenSavers = hw::ssaver::SaverMux<U8G2, hw::ssaver::StarField,
                                            hw::ssaver::ThroughTheStars>;
  using PinExpander = zlkm::hw::io::Mcp23017Pins;
  using SaverCfg = typename ScreenSavers::Cfg;
  using Calcis = zlkm::ch::Calcis;
  using CalcisTR = zlkm::ch::CalcisTR;
  using CalcisCfg = zlkm::ch::Calcis::Cfg;
  using Feedback = typename Calcis::Feedback;

  struct Cfg {
    std::array<uint8_t, Selection::TAB_COUNT> ledPins_{};
    uint32_t fps = 60;
    CalcisCfg* pCfg = nullptr;
  };

  View(Selection& selection, PinExpander& exp, const Cfg& cfg,
       Feedback* fb = nullptr)
      : screen_({}),
        selection_(selection),
        cfg_(cfg),
        saver_(SaverCfg()),
        pinExp_(exp),
        triggerLED_(27),
        clippingLED_(28),
        fb_(fb) {
    for (int i = 0; i < 4; ++i) {
      pinExp_.setPinMode(cfg_.ledPins_[i], zlkm::hw::io::PinMode::Output);
      pinExp_.writePin(cfg_.ledPins_[i], false);
    }
    // Construct button manager on the expander
    updateTabLEDs_();
    lastUpdateMs_ = millis();
    updateInterval_ = 1000 / (cfg_.fps ? cfg_.fps : 60);
  }

  // Call on UI core at free cadence; internally gated to ~fps
  void update(bool activity) {
    ZLKM_PERF_SCOPE("View::update");
    const uint32_t now = millis();
    if (now - lastUpdateMs_ < updateInterval_) return;
    lastUpdateMs_ = now;

    using namespace zlkm::dsp;
    if (cfg_.pCfg->trigCounter != lastTrigCounter_) {
      lastTrigCounter_ = cfg_.pCfg->trigCounter;
      onTrigger(rateToMs(cfg_.pCfg->envs[Calcis::EnvAmp].decay, CalcisTR::SR));
    }

    if (activity) {
      saver_.noteActivity(now);
    }
    // Render
    screen_.update([&](U8G2& g) {
      if (saver_.step(now, g)) {
        return;
      }
      g.setFont(u8g2_font_6x12_tf);
      g.drawStr(1, 12, "Calcis Humilis");
      // Show current tab/page (1-based) and totals
      char buf[64];
      const uint8_t tabIdx = selection_.currentTabIndex();
      const uint8_t tabTotal = Selection::count();
      const uint8_t pageIdx = selection_.currentPageIndex();
      const uint8_t pageTotal = selection_.currentTabPageCount();
      snprintf(buf, sizeof(buf), "Tab %u/%u  Page %u/%u",
               (unsigned)(tabIdx + 1), (unsigned)tabTotal,
               (unsigned)(pageIdx + 1), (unsigned)pageTotal);
      g.drawStr(1, 24, buf);
      g.drawFrame(0, 0, screen_.width(), screen_.height());
    });
    updateTabLEDs_();
    // Tick LEDs at view rate (including clipping detection)
    tickLED_();
    // Update LEDs animations
    triggerLED_.Update();
    clippingLED_.Update();
  }

  // Called when a trigger rising edge occurs
  void onTrigger(uint32_t fadeMs) { triggerLED_.FadeOff(fadeMs); }

  // Called when clipping/saturation is detected
  void onClipping(uint32_t fadeMs = 80) { clippingLED_.FadeOff(fadeMs); }

 private:
  void updateTabLEDs_() {
    const uint8_t active = selection_.currentTabIndex();
    for (uint8_t i = 0; i < 4; ++i)
      pinExp_.writePin(cfg_.ledPins_[i], i == active);
  }

  void tickLED_() {
    if (fb_ && saturationCounter_ < fb_->saturationCounter) {
      onClipping(80);
      saturationCounter_ = fb_->saturationCounter;
    }
  }

  ScreenT screen_;
  Selection& selection_;
  ScreenSavers saver_;
  Cfg cfg_{};
  uint32_t lastUpdateMs_ = 0;
  uint32_t updateInterval_ = 0;
  PinExpander& pinExp_;
  // Moved from UI: LEDs
  JLed triggerLED_;
  JLed clippingLED_;
  // Optional feedback for clipping detection
  Feedback* fb_{};
  int saturationCounter_ = 0;
  int lastTrigCounter_ = 0;
};

}  // namespace zlkm::ui
