#pragma once

#include <atomic>
#include <tuple>
#include <utility>

namespace zlkm::ui {

// Generic variadic Tab type: holds a tuple of Pages and the current page index.
template <class... Pages>
struct Tab {
  using PagesTuple = std::tuple<Pages...>;
  static constexpr uint8_t pageCount = sizeof...(Pages);
  PagesTuple pages{};
  std::atomic<uint8_t> currentPage{0};
};

// Generic variadic TabControl: holds a tuple of Tabs and the current tab index.
template <class... Tabs>
struct TabControl {
  using TabsTuple = std::tuple<Tabs...>;
  static constexpr uint8_t tabCount = sizeof...(Tabs);
  TabsTuple tabs{};
  std::atomic<uint8_t> currentTab{0};
};

}  // namespace zlkm::ui
