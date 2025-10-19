#pragma once

#include <array>

#include "ui/InputMapper.h"
#include "ui/UiTypes.h"

namespace zlkm::ui {

// UI-level parameter page and tab structures.
template <size_t ROTARY_COUNT_>
struct ParameterPageT {
  static constexpr int ROTARY_COUNT = static_cast<int>(ROTARY_COUNT_);
  std::array<zlkm::ui::InputMapper, ROTARY_COUNT> mappers{};
  // Raw UI state for each rotary (0..4095)
  std::array<int16_t, ROTARY_COUNT> rawPos{};
  // Short parameter labels for each rotary (const char* string literals only)
  std::array<const char*, ROTARY_COUNT> labels{};
};

template <size_t PAGE_COUNT_, size_t ROTARY_COUNT_>
struct ParameterTabT {
  static constexpr int PAGE_COUNT = static_cast<int>(PAGE_COUNT_);
  static constexpr int ROTARY_COUNT = static_cast<int>(ROTARY_COUNT_);

  std::array<ParameterPageT<ROTARY_COUNT_>, PAGE_COUNT> pages{};
  uint8_t pageCount = 0;    // runtime active pages (<= PAGE_COUNT)
  uint8_t currentPage = 0;  // runtime selection (< pageCount)
};

// Note: Prefer using the templated forms directly to make sizes explicit.
template <int N, int PAGE_COUNT_, int ROTARY_COUNT_>
struct ParameterTabControlT {
  static constexpr int TAB_COUNT = N;
  static constexpr int PAGE_COUNT = PAGE_COUNT_;
  static constexpr int ROTARY_COUNT = ROTARY_COUNT_;

  using PTab = ::zlkm::ui::ParameterTabT<PAGE_COUNT, ROTARY_COUNT>;

  std::array<PTab, N> tabs{};
  uint8_t currentTab{0};

  // Counts and indices
  static constexpr uint8_t count() { return static_cast<uint8_t>(N); }
  uint8_t currentTabIndex() const { return currentTab; }
  uint8_t currentPageIndex() const { return tabs[currentTab].currentPage; }
  uint8_t currentTabPageCount() const { return tabs[currentTab].pageCount; }

  // Tab selection
  void setCurrentTab(uint8_t idx) {
    uint8_t nextTab = static_cast<uint8_t>(idx % N);
    if (tabs[nextTab].pageCount > 0) {
      currentTab = nextTab;
    }
  }

  // Page navigation
  void nextPageInCurrentTab() {
    auto& t = tabs[currentTab];
    if (t.pageCount > 0) {
      t.currentPage = static_cast<uint8_t>((t.currentPage + 1) % t.pageCount);
    } else {
      t.currentPage = 0;
    }
  }

  // Accessors to tabs
  PTab& tabAt(uint8_t i) { return tabs[i]; }
  const PTab& tabAt(uint8_t i) const { return tabs[i]; }
};

// No back-compat alias; use ParameterTabControlT with explicit sizes.

}  // namespace zlkm::ui
