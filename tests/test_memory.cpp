// ARMOR-NODE-SOLAR - how much memory the readings of the battery stacks take, measured on a computer, and the limits it may not grow past.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Four ports, each with a stack of eight modules of 15 or 16 cells, are read by the real Poller from a stand-in console; every byte allocated is counted. The computer's pointers are
// 64 bits wide, so the figures are above those of the ESP32-S3: they are limits against growth, not the board's figures.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "../core/poller.hpp"
#include "../core/psram_scope.hpp"
#include "pylontech_real.hpp"

namespace {
long g_live = 0, g_small = 0, g_peak = 0;   // bytes now, those in blocks the board's sdkconfig would put in the internal memory (up to 4096), and the most so far
constexpr std::size_t kHead = 16;
int g_failures = 0;
}  // namespace
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++g_failures; } } while (0)

void* operator new(std::size_t size) {
  auto* block = static_cast<unsigned char*>(std::malloc(size + kHead));
  if (block == nullptr) std::abort();
  *reinterpret_cast<std::size_t*>(block) = size;
  g_live += static_cast<long>(size);
  if (size <= 4096) g_small += static_cast<long>(size);
  if (g_live > g_peak) g_peak = g_live;
  return block + kHead;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept {
  if (memory == nullptr) return;
  auto* block = static_cast<unsigned char*>(memory) - kHead;
  const std::size_t size = *reinterpret_cast<std::size_t*>(block);
  g_live -= static_cast<long>(size);
  if (size <= 4096) g_small -= static_cast<long>(size);
  std::free(block);
}
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

using namespace armor;

constexpr int kStacks = 4;
static int kModules = 8;   // measured with eight and with sixteen; the limits below are for eight and double with sixteen
// The limits for ONE port with eight modules. What it keeps between two cycles (the poller and the last whole message), in all and in small blocks; the message; the most one cycle adds.
constexpr long kKeptPerStack = 96 * 1024;
constexpr long kSmallPerStack = 24 * 1024;
constexpr std::size_t kMessageBytes = 16 * 1024;
constexpr long kCyclePeak = 112 * 1024;

static std::string answer(const std::string& command, int cells) {
  std::string out = command + "\r\n@\r\n";
  if (command == "pwr") {
    out += "Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb Time B.V.St B.T.St MosTempr M.T.St SysAlarm.St\r\n";
    for (int i = 1; i <= kModules; ++i) out += std::to_string(i) + " 49872 -1280 22000 20000 25000 3330 3348 Dischg Normal Normal Normal 88% 2018-11-14 15:12:04 Normal Normal 23000 Normal Normal\r\n";
  } else if (command.rfind("bat", 0) == 0) {
    for (int i = 0; i < cells; ++i) out += std::to_string(i) + " 3340 -1000 22000 Dischg Normal Normal Normal 88% 66550 mAH N\r\n";
  } else if (command.rfind("info", 0) == 0) {
    out += "Device name : US3000C\r\nBoard version : V10R04\r\nMain Soft version : B68.2.0.0\r\nSoft version : V2.6\r\nSpecification : 48V/74AH\r\nCell Number : 15\r\nMax Dischg Curr : -55000mA\r\n"
           "Max Charge Curr : 53000mA\r\nConsole Port rate : 115200\r\n";
  } else if (command.rfind("stat", 0) == 0) {
    out += kRealStat;
  } else if (command.rfind("data", 0) == 0) {
    out += kRealDataEvent;
  } else if (command.rfind("soh", 0) == 0) {
    out += kRealSoh;
  }
  return out + "Command completed successfully\r\n$$\r\npylon>";
}

// One whole cycle of a port: its message, or "" when none came.
static std::string cycle(solar::Poller& poller, int cells, std::uint64_t& now) {
  for (int turn = 0; turn < 4000; ++turn) {
    const std::vector<std::uint8_t> tx = poller.next_tx(now);
    now += 40;
    if (!tx.empty()) {
      std::string command(tx.begin(), tx.end());
      while (!command.empty() && command.front() == '\r') command.erase(0, 1);
      while (!command.empty() && command.back() == '\r') command.pop_back();
      const std::string reply = answer(command, cells);
      poller.on_rx(reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size(), now);
    }
    std::string topic, payload;
    if (poller.take_message(now, topic, payload)) return payload;
  }
  return "";
}

struct Port {
  std::unique_ptr<solar::Poller> poller;
  std::shared_ptr<const std::string> reading;
  std::uint64_t now = 1000;
};

static void measure(int cells) {
  std::vector<Port> ports(kStacks);
  const long start = g_live, small_start = g_small;
  for (int s = 0; s < kStacks; ++s) {
    ports[s].poller = std::make_unique<solar::Poller>(config::Kind::kPylontech, "solar-1", "bank-" + std::to_string(s + 1), 5, 0);   // 0: every battery that answers
    long after_second = 0;
    std::size_t message = 0;
    for (int round = 0; round < 4; ++round) {
      const long before = g_live;
      g_peak = g_live;
      {
        std::string payload = cycle(*ports[s].poller, cells, ports[s].now);
        message = payload.size();
        CHECK(!payload.empty() && message <= kMessageBytes * static_cast<std::size_t>(kModules / 8));
        ports[s].reading = std::make_shared<const std::string>(std::move(payload));
      }
      ports[s].now += 6000;
      CHECK(g_peak - before <= kCyclePeak * (kModules / 8));
      if (round == 1) after_second = g_live;
    }
    CHECK(ports[s].poller->modules().size() == static_cast<std::size_t>(kModules));
    CHECK(ports[s].poller->modules().back().cells_v.size() == static_cast<std::size_t>(cells));
    CHECK(g_live - after_second <= 8 * 1024);   // nothing piles up from one cycle to the next (the message's buffer may differ by its growth)
    const long kept = g_live - start, small = g_small - small_start;
    std::printf("%d batteries of %d cells, %d port(s): %ld bytes kept (%ld in blocks of up to 4096), message %zu bytes\n", kModules, cells, s + 1, kept, small, message);
    CHECK(kept <= kKeptPerStack * (s + 1) * (kModules / 8));
    CHECK(small <= kSmallPerStack * (s + 1) * (kModules / 8));
  }
  // the counters of `stat` name the catalogue's own text: no string of their own for each module
  for (const auto& counter : ports[0].poller->modules().front().counters) {
    bool from_catalogue = false;
    for (const auto& known : solar::pylontech::stat_catalog()) from_catalogue = from_catalogue || counter.first.data() == known.key;
    CHECK(from_catalogue);
  }
  CHECK(!ports[0].poller->modules().front().counters.empty());
}

static void test_totals_are_whole_or_absent() {
  std::vector<solar::pylontech::Module> modules(2);
  for (auto& m : modules) { m.present = true; m.voltage_v = 50.0; m.capacity_ah = 40.0; m.full_capacity_ah = 74.0; }
  solar::pylontech::Stack stack = solar::pylontech::summarise(modules);
  CHECK(stack.capacity_ah == 80.0 && stack.full_capacity_ah == 148.0 && stack.energy_kwh > 3.99 && stack.energy_kwh < 4.01);
  modules[1].capacity_ah = -1;   // its charge was not read: half a sum is not the stack's charge
  stack = solar::pylontech::summarise(modules);
  CHECK(stack.capacity_ah < 0 && stack.energy_kwh < 0 && stack.full_capacity_ah == 148.0);
  modules[1].full_capacity_ah = -1;
  CHECK(solar::pylontech::summarise(modules).full_capacity_ah < 0);
}

static void test_the_tasks_that_use_the_external_memory() {
  static memory::ScopeRegistry<int*, 3> tasks;
  int a = 0, b = 0, c = 0, d = 0;
  CHECK(tasks.size() == 0 && !tasks.contains(&a) && !tasks.contains(nullptr) && !tasks.add(nullptr));
  CHECK(tasks.add(&a) && !tasks.add(&a) && tasks.add(&b) && tasks.add(&c) && !tasks.add(&d) && tasks.size() == 3);
  tasks.remove(&b);
  CHECK(!tasks.contains(&b) && tasks.contains(&c) && tasks.add(&d) && tasks.size() == 3);
  tasks.remove(&b);
  tasks.remove(nullptr);
  CHECK(tasks.size() == 3);
}

int main() {
  measure(15);
  measure(16);
  kModules = 16;
  measure(15);
  kModules = 8;
  test_totals_are_whole_or_absent();
  test_the_tasks_that_use_the_external_memory();
  if (g_failures != 0) { std::printf("%d failure(s)\n", g_failures); return 1; }
  std::printf("memory: ok\n");
  return 0;
}
