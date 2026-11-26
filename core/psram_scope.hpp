// ARMOR-NODE-SOLAR - which tasks keep their data in the external memory (PSRAM): a small, lock-free list of tasks. No hardware.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// main/psram_new.cpp asks it on every C++ allocation, also during the start-up before any task exists, so it must work zero-initialised, take no lock and allocate nothing.
#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace armor::memory {

template <typename Handle, std::size_t N>
class ScopeRegistry {
 public:
  // True when the task was added by this call (the caller then removes it). False when it was already there, or there is no room (its data then stays where it was).
  bool add(Handle task) {
    if (task == Handle{} || contains(task)) return false;
    for (auto& slot : slots_) {
      Handle empty{};
      if (slot.compare_exchange_strong(empty, task)) { ++used_; return true; }
    }
    return false;
  }
  void remove(Handle task) {
    if (task == Handle{}) return;
    for (auto& slot : slots_) {
      Handle expected = task;
      if (slot.compare_exchange_strong(expected, Handle{})) { --used_; return; }
    }
  }
  bool contains(Handle task) const {
    if (task == Handle{}) return false;
    for (const auto& slot : slots_) if (slot.load() == task) return true;
    return false;
  }
  unsigned size() const { return used_.load(); }

 private:
  std::array<std::atomic<Handle>, N> slots_{};
  std::atomic<unsigned> used_{0};
};

}  // namespace armor::memory
