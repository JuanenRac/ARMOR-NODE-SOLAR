// ARMOR-NODE-SOLAR - the node's own `operator new`: the external memory for the tasks that asked (main/psram_new.hpp), malloc as before for everything else.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include "psram_new.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>
extern "C" {
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}
#include "core/psram_scope.hpp"

namespace armor::memory {
namespace {
ScopeRegistry<TaskHandle_t, 24> g_tasks;   // zero-initialised: usable before any constructor has run
}  // namespace

void keep_data_in_psram() { g_tasks.add(xTaskGetCurrentTaskHandle()); }
ExternalScope::ExternalScope() : added_(g_tasks.add(xTaskGetCurrentTaskHandle())) {}
ExternalScope::~ExternalScope() { if (added_) g_tasks.remove(xTaskGetCurrentTaskHandle()); }

}  // namespace armor::memory

// The array and the no-throw forms of the standard library call this one. `operator delete` stays: free() takes memory of both kinds. When nothing is left the node's new-handler
// (main/node_guard.cpp) runs, as with the library's own operator.
void* operator new(std::size_t size) {
  if (size == 0) size = 1;
  for (;;) {
    const bool external = armor::memory::g_tasks.size() != 0 && armor::memory::g_tasks.contains(xTaskGetCurrentTaskHandle());
    void* memory = external ? heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT) : std::malloc(size);
    if (memory != nullptr) return memory;
    const std::new_handler handler = std::get_new_handler();
    if (handler == nullptr) std::abort();
    handler();
  }
}
