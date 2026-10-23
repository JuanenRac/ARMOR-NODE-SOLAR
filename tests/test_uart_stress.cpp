// ARMOR-NODE-SOLAR - a stress test of the emulated (software) UART: a stream of bytes on the line, the edge ring the interrupt fills, and a task that drains the ring in turns while
// writes to the flash stop it for tens of milliseconds. It plays the pieces of main/uart_ports.cpp that are not the chip (the ring, the draining, the arithmetic of core/soft_uart.hpp)
// against a clock made up for the test; the interrupt keeps running during a stall, as the one in the firmware does (it runs from the chip's own memory).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

#include "../core/soft_uart.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace armor;

struct Edge { std::uint64_t time_us; bool level; };

// The edges a byte stream makes on the line, each moved by up to `jitter_us` (repeatable), with `gap_bits` of idle line after every byte.
static std::vector<Edge> edges_of(const std::vector<std::uint8_t>& bytes, unsigned baud, std::uint64_t start_us, int jitter_us, int gap_bits, std::uint64_t& end_us) {
  const double bit = 1000000.0 / baud;
  std::uint64_t seed = 987654321;
  const auto jitter = [&]() { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; return jitter_us == 0 ? 0 : static_cast<int>((seed >> 33) % (2 * jitter_us + 1)) - jitter_us; };
  std::vector<Edge> edges;
  double t = static_cast<double>(start_us);
  bool level = true;
  for (const std::uint8_t byte : bytes) {
    for (const bool b : softuart::frame_bits(byte)) {
      if (b != level) { level = b; edges.push_back({static_cast<std::uint64_t>(t + jitter()), level}); }
      t += bit;
    }
    t += gap_bits * bit;
  }
  end_us = static_cast<std::uint64_t>(t);
  return edges;
}

// The ring of main/uart_ports.cpp: `slots` places, one always empty; an edge that finds it full is counted and lost.
struct Ring {
  explicit Ring(std::size_t places) : slots(places) {}
  std::size_t slots;
  std::deque<Edge> q;
  std::uint32_t overruns = 0;
  void push(const Edge& e) { if (q.size() + 1 >= slots) { ++overruns; return; } q.push_back(e); }
};

struct Result {
  std::vector<std::uint8_t> got;
  std::uint32_t overruns = 0, framing_errors = 0;
};

// Plays `edges` into a ring of `slots`; the task drains it and polls the receiver every `period_ms`, except while a write stops it: every `stall_every_ms` it stops for `stall_ms`.
static Result play(const std::vector<Edge>& edges, unsigned baud, std::size_t slots, std::uint64_t end_us, unsigned period_ms, unsigned stall_every_ms, unsigned stall_ms) {
  Ring ring(slots);
  softuart::Receiver receiver(baud);
  Result r;
  std::size_t next = 0;
  std::uint64_t task_free_at_us = 0, next_stall_us = static_cast<std::uint64_t>(stall_every_ms) * 1000ULL;
  for (std::uint64_t now = 0; now <= end_us + 200000; now += period_ms * 1000ULL) {
    if (stall_ms != 0 && now >= next_stall_us && now >= task_free_at_us) {
      task_free_at_us = now + static_cast<std::uint64_t>(stall_ms) * 1000ULL;
      next_stall_us = task_free_at_us + static_cast<std::uint64_t>(stall_every_ms) * 1000ULL;
    }
    const std::uint64_t at = std::max(now, task_free_at_us);   // the interrupt runs all the time; the task only when it is not stopped
    // the interrupt fills the ring up to the moment the task looks again, and the task then takes everything from it
    while (next < edges.size() && edges[next].time_us <= at) ring.push(edges[next++]);
    for (const Edge& e : ring.q) receiver.edge(e.time_us, e.level);
    ring.q.clear();
    receiver.poll(at, r.got);
  }
  r.overruns = ring.overruns;
  r.framing_errors = receiver.framing_errors();
  return r;
}

// Bytes that change the level a lot: the worst a line can do is 0x55 without a gap (one edge per bit); real traffic is anything.
static std::vector<std::uint8_t> worst(std::size_t n) { return std::vector<std::uint8_t>(n, 0x55); }
static std::vector<std::uint8_t> traffic(std::size_t n) {
  std::vector<std::uint8_t> out;
  std::uint64_t seed = 42;
  for (std::size_t i = 0; i < n; ++i) { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; out.push_back(static_cast<std::uint8_t>(seed >> 40)); }
  return out;
}

static void test_the_budget_arithmetic() {
  CHECK(softuart::max_stall_ms(19200, 1024) == 53 && softuart::max_stall_ms(9600, 1024) == 106 && softuart::max_stall_ms(2400, 1024) == 426);
  CHECK(softuart::max_stall_ms(0, 1024) == 0 && softuart::max_stall_ms(9600, 1) == 0 && softuart::max_stall_ms(9600, 0) == 0);
  CHECK(softuart::slots_needed(19200, 50) >= 961 && softuart::slots_needed(19200, 50) <= 963);
  for (const unsigned baud : {1200u, 2400u, 9600u, 19200u})
    for (const unsigned stall : {10u, 25u, 50u, 100u}) CHECK(softuart::max_stall_ms(baud, softuart::slots_needed(baud, stall)) >= stall);   // the two ways round agree
  CHECK(softuart::max_stall_ms(softuart::kMaxBaud, 1024) >= 50);   // the ring of the firmware outlasts a flash write at the fastest speed
}

static void test_a_stream_is_whole_through_flash_writes() {
  for (const unsigned baud : {2400u, 9600u, 19200u}) {
    for (const bool hard : {false, true}) {
      const std::vector<std::uint8_t> sent = hard ? worst(300) : traffic(300);
      std::uint64_t end = 0;
      const std::vector<Edge> edges = edges_of(sent, baud, 5000, 0, 0, end);
      // a stall that the ring outlasts (with a margin), every 120 ms, as when the settings are saved over and over
      const unsigned stall = std::min(40u, softuart::max_stall_ms(baud, 1024) * 8 / 10);
      const Result r = play(edges, baud, 1024, end, 5, 120, stall);
      CHECK(r.overruns == 0);
      CHECK(r.got == sent);
      CHECK(r.framing_errors == 0);
    }
  }
}

static void test_the_edges_of_a_busy_interrupt() {
  // every edge late or early by up to a tenth of a bit: the interrupt of a node with Wi-Fi up
  for (const unsigned baud : {9600u, 19200u}) {
    const std::vector<std::uint8_t> sent = traffic(400);
    std::uint64_t end = 0;
    const int jitter = static_cast<int>(1000000 / baud / 10);
    const std::vector<Edge> edges = edges_of(sent, baud, 3000, jitter, 0, end);
    const Result r = play(edges, baud, 1024, end, 5, 150, 30);
    CHECK(r.got == sent && r.framing_errors == 0 && r.overruns == 0);
  }
}

static void test_a_stall_longer_than_the_ring_is_counted_and_the_line_recovers() {
  const unsigned baud = 19200;
  // a worst-case stream first, with a stall of twice what the ring holds: edges are lost and counted ...
  const std::vector<std::uint8_t> sent = worst(200);
  std::uint64_t end = 0;
  const std::vector<Edge> edges = edges_of(sent, baud, 5000, 0, 0, end);
  const unsigned too_long = softuart::max_stall_ms(baud, 1024) * 2;
  const Result lost = play(edges, baud, 1024, end, 5, 20, too_long);
  CHECK(lost.overruns > 0);
  CHECK(lost.got.size() < sent.size() || lost.framing_errors > 0);   // not silently whole
  // ... and the same line, a pause later, carrying a good message, comes through whole: the overflow does not leave the receiver out of step
  const std::vector<std::uint8_t> message{'(', 'N', 'A', 'K', 0x73, 0x73, '\r'};
  std::uint64_t message_end = 0;
  const std::vector<Edge> after = edges_of(message, baud, end + 400000, 0, 0, message_end);
  std::vector<Edge> all = edges;
  all.insert(all.end(), after.begin(), after.end());
  const Result recovered = play(all, baud, 1024, message_end, 5, 20, too_long);
  CHECK(recovered.got.size() >= message.size());
  CHECK(recovered.got.size() >= message.size() && std::equal(message.begin(), message.end(), recovered.got.end() - static_cast<std::ptrdiff_t>(message.size())));
}

static void test_a_larger_ring_rides_out_a_longer_stall() {
  const unsigned baud = 19200;
  const std::vector<std::uint8_t> sent = worst(300);
  std::uint64_t end = 0;
  const std::vector<Edge> edges = edges_of(sent, baud, 5000, 0, 0, end);
  const Result small = play(edges, baud, 1024, end, 5, 100, 90);
  const Result large = play(edges, baud, softuart::slots_needed(baud, 100), end, 5, 100, 90);
  CHECK(small.overruns > 0);
  CHECK(large.overruns == 0 && large.got == sent);
}

int main() {
  test_the_budget_arithmetic();
  test_a_stream_is_whole_through_flash_writes();
  test_the_edges_of_a_busy_interrupt();
  test_a_stall_longer_than_the_ring_is_counted_and_the_line_recovers();
  test_a_larger_ring_rides_out_a_longer_stall();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
