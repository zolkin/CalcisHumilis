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
  using CurBoard = ::zlkm::platform::boards::Current;
  using PinSource = CurBoard::PinSource;
  using Pin = CurBoard::PinId;
  using SaverCfg = typename ScreenSavers::Cfg;
  using Calcis = zlkm::ch::Calcis;
  using CalcisTR = zlkm::ch::CalcisTR;
  using CalcisCfg = zlkm::ch::Calcis::Cfg;
  using Feedback = typename Calcis::Feedback;

  static Pin::ValueType getPin(const Pin& pin) {
    return zlkm::hw::io::getPin(pin).value;
  }

  struct Cfg {
    uint32_t fps = 60;
    CalcisCfg* pCfg = nullptr;
  };

  static CurBoard::PinSource& pins() { return CurBoard::pins(); }

  View(Selection& selection, const Cfg& cfg, Feedback* fb)
      : screen_({}),
        selection_(selection),
        cfg_(cfg),
        saver_(SaverCfg()),
        triggerLED_(getPin(CurBoard::LED_TRIGGER)),
        clippingLED_(getPin(CurBoard::LED_CLIPPING)),
        fb_(fb) {
    assert(cfg_.pCfg != nullptr && "View requires valid Calcis config");

    pins().setPinsMode(CurBoard::LEDS, zlkm::hw::io::PinMode::Output);
    pins().writePins(CurBoard::LEDS, false);
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

        // Draw encoder rings in one row at the bottom (bitmap atlas)
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
        const int rings = 4;
        const int ringY = h - margin - rh;
        // Evenly distribute ring centers across full width
        auto ring_x_at_index = [&](int i) -> int {
          // Center position for ring i in [0..rings-1]
          int cx = ((2 * i + 1) * w) / (2 * rings);
          return cx - (rw / 2);
        };
        g.drawXBMP(ring_x_at_index(0), ringY, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[0])));
        g.drawXBMP(ring_x_at_index(1), ringY, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[1])));
        g.drawXBMP(ring_x_at_index(2), ringY, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[2])));
        g.drawXBMP(ring_x_at_index(3), ringY, rw, rh,
                   ring16x16_frame(lvl(page.rawPos[3])));

        // Text and page info, kept above the ring row
        g.setFont(u8g2_font_5x8_tf);
        const char* title = "CalcisHumilis";
        std::array<char, 64> buf{};
        const uint8_t tabTotal = Selection::count();
        const uint8_t pageTotal = selection_.currentTabPageCount();

        // 1) Per-parameter short labels above each ring from selection pages
        const auto& pageRef = t.pages[pageIdx];
        const int yLabel = ringY - 2;  // baseline just above rings
        for (int i = 0; i < rings; ++i) {
          const char* label = pageRef.labels[i];
          if (!label || label[0] == '\0') continue;
          int cx = ((2 * i + 1) * w) / (2 * rings);
          int lw = g.getStrWidth(label);
          g.drawStr(cx - lw / 2, yLabel, label);
        }

        // 2) Single info row: "Tab x/y  Page x/y" above labels
        int yInfo = yLabel - 10;  // one row above labels
        snprintf(buf.data(), buf.size(), "Tab %u/%u  Page %u/%u",
                 (unsigned)(tabIdx + 1), (unsigned)tabTotal,
                 (unsigned)(pageIdx + 1), (unsigned)pageTotal);
        int iw = g.getStrWidth(buf.data());
        g.drawStr((w - iw) / 2, yInfo, buf.data());

        // 3) Title at the top (if space permits)
        int yTitle = yInfo - 12;
        if (yTitle < 8) {
          int delta = 8 - yTitle;
          yTitle += delta;
          yInfo += delta;
          // yLabel stays relative to rings to preserve spacing
        }
        int tw = g.getStrWidth(title);
        g.drawStr((w - tw) / 2, yTitle, title);
      });
    }
    tickLED_();
  }

 private:
  void updateTabLEDs_() {
    const uint8_t activeTab = selection_.currentTabIndex();
    for (uint8_t i = 0; i < 4; ++i) {
      const bool state = (i == activeTab);
      pins().writeGroupPin(CurBoard::LEDS, i, state);
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
  // Moved from UI: LEDs
  JLed triggerLED_;
  JLed clippingLED_;
  // Optional feedback for clipping detection
  Feedback* fb_{};
  int saturationCounter_ = 0;
  int lastTrigCounter_ = 0;
};

}  // namespace zlkm::ui
