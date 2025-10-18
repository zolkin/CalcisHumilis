#pragma once

#include <JLED.h>
#include <U8g2lib.h>

#include <array>

#include "CalcisHumilis.h"
#include "hw/Screen.h"
#include "hw/screensavers/SaverMux.h"
#include "hw/screensavers/StarField.h"
#include "hw/screensavers/ThroughTheStars.h"
#include "platform/boards/Current.h"
#include "platform/platform.h"
#include "ui/TabControl.h"
#include "ui/UiTypes.h"  // aliases: ScreenSSD
#include "ui/assets/ring16x16_48.h"
#include "util/IdleTimer.h"

namespace zlkm::ui {

template <class Selection>
class View {
 public:
  using ScreenT = zlkm::ch::ScreenSSD;
  using ScreenSavers = hw::ssaver::SaverMux<U8G2, hw::ssaver::StarField,
                                            hw::ssaver::ThroughTheStars>;
  using PinSource = typename ::zlkm::platform::boards::current::PinSource;
  using SaverCfg = typename ScreenSavers::Cfg;
  using Calcis = zlkm::ch::Calcis;
  using CalcisTR = zlkm::ch::CalcisTR;
  using CalcisCfg = zlkm::ch::Calcis::Cfg;
  using Feedback = typename Calcis::Feedback;
  using PinDefs = typename ::zlkm::platform::boards::current::PinDefs;
  using Pin = typename ::zlkm::platform::boards::current::PinId;
  template <size_t N>
  using PinArray = std::array<Pin, N>;

  struct Cfg {
    uint32_t fps = 60;
    CalcisCfg* pCfg = nullptr;
  };

  View(Selection& selection, PinSource& dev, const Cfg& cfg,
       Feedback* fb = nullptr)
      : screen_({}),
        selection_(selection),
        cfg_(cfg),
        saver_(SaverCfg()),
        pinDev_(dev),
        triggerLED_((uint8_t)PinDefs::LED_TRIGGER.pin().value),
        clippingLED_((uint8_t)PinDefs::LED_CLIPPING.pin().value),
        fb_(fb) {
    pinDev_.setPinsMode(PinDefs::LEDS, zlkm::hw::io::PinMode::Output);
    pinDev_.writePins(PinDefs::LEDS, false);
    // Construct button manager on the expander
    updateTabLEDs_();
    lastUpdateMs_ = millis();
    updateInterval_ = 1000 / (cfg_.fps ? cfg_.fps : 60);
  }

  // Call on UI core at free cadence; internally gated to ~fps
  void update(const zlkm::util::IdleTimer& idle) {
    ZLKM_PERF_SCOPE("View::update");
    const uint32_t now = millis();
    if (now - lastUpdateMs_ < updateInterval_) return;
    lastUpdateMs_ = now;

    using namespace zlkm::dsp;
    if (cfg_.pCfg->trigCounter != lastTrigCounter_) {
      lastTrigCounter_ = cfg_.pCfg->trigCounter;
      const uint16_t fadeMs =
          rateToMs(cfg_.pCfg->envs[Calcis::EnvAmp].decay, CalcisTR::SR);
      triggerLED_.FadeOff(fadeMs);
    }

    // Render
    {
      ZLKM_PERF_SCOPE("screen update");
      screen_.update([&](U8G2& g) {
        using namespace zlkm::ui::assets;
        if (saver_.step(now, idle.isIdle(now), g)) {
          return;
        }
        // Fetch current page values
        const uint8_t tabIdx = selection_.currentTabIndex();
        const uint8_t pageIdx = selection_.currentPageIndex();
        const auto& t = selection_.tabAt(tabIdx);
        const auto& page = t.pages[pageIdx];

        // Draw encoder rings in corners (bitmap atlas)
        const int w = screen_.width();
        const int h = screen_.height();
        const int margin = 3;
        const int rw = RING16_W;
        const int rh = RING16_H;
        auto lvl = [](int raw) -> uint8_t {
          int v = (raw * RING16_STEPS + 2047) / 4095;
          if (v > RING16_STEPS) v = RING16_STEPS;
          return static_cast<uint8_t>(v);
        };
        g.setBitmapMode(1);  // transparent
        g.drawXBMP(margin, margin, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[0])));
        g.drawXBMP(w - margin - rw, margin, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[1])));
        g.drawXBMP(margin, h - margin - rh, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[2])));
        g.drawXBMP(w - margin - rw, h - margin - rh, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[3])));

        // Centered text and page info
        g.setFont(u8g2_font_5x8_tf);
        const char* title = "CalcisHumilis";
        int tw = g.getStrWidth(title);
        int tx = (w - tw) / 2;
        int ty = h / 2 - 2;
        g.drawStr(tx, ty, title);

        char buf[64];
        const uint8_t tabTotal = Selection::count();
        const uint8_t pageTotal = selection_.currentTabPageCount();
        snprintf(buf, sizeof(buf), "Tab %u/%u", (unsigned)(tabIdx + 1),
                 (unsigned)tabTotal);
        int tabw = g.getStrWidth(buf);
        g.drawStr((w - tabw) / 2, ty + 14, buf);

        snprintf(buf, sizeof(buf), "Page %u/%u", (unsigned)(pageIdx + 1),
                 (unsigned)pageTotal);
        int pw = g.getStrWidth(buf);
        g.drawStr((w - pw) / 2, ty + 28, buf);
      });
    }
    tickLED_();
  }

 private:
  void updateTabLEDs_() {
    const uint8_t activeTab = selection_.currentTabIndex();
    for (uint8_t i = 0; i < 4; ++i) {
      const bool state = (i == activeTab);
      pinDev_.writeGroupPin(PinDefs::LEDS.group(), PinDefs::LEDS[i], state);
    }
  }

  void tickLED_() {
    // Update LEDs animations
    ZLKM_PERF_SCOPE("View::tickLED_");
    updateTabLEDs_();
    triggerLED_.Update();
    clippingLED_.Update();
    if (fb_ && saturationCounter_ < fb_->saturationCounter) {
      clippingLED_.FadeOff(80);
      saturationCounter_ = fb_->saturationCounter;
    }
  }

  // Procedural ring renderer removed in favor of precomputed bitmaps

  ScreenT screen_;
  Selection& selection_;
  ScreenSavers saver_;
  Cfg cfg_{};
  uint32_t lastUpdateMs_ = 0;
  uint32_t updateInterval_ = 0;
  PinSource& pinDev_;
  // Moved from UI: LEDs
  JLed triggerLED_;
  JLed clippingLED_;
  // Optional feedback for clipping detection
  Feedback* fb_{};
  int saturationCounter_ = 0;
  int lastTrigCounter_ = 0;
};

}  // namespace zlkm::ui
