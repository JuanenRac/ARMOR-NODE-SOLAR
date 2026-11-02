// ARMOR-NODE-SOLAR - host tests of the lateness of the interrupt that times the edges of an emulated serial line: the record of it, the limits (docs/UART_JITTER_LIMITS.json) and, on a
// simulated interrupt, that a line the verdict calls fine is read byte for byte while one that is far past the limit is not.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Usage: test_isr_jitter [path of docs/UART_JITTER_LIMITS.json]
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../core/isr_jitter.hpp"
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

static jitter::Limits g_limits;

struct Seen { std::vector<std::uint8_t> bytes; std::uint32_t framing_errors = 0; jitter::Recorder lateness; };

// What the interrupt does with the edges of `sent` on a line of `baud`: it notes each one `lateness(index)` microseconds after it happened. The record keeps those latenesses.
template <typename Lateness>
static Seen play(const std::vector<std::uint8_t>& sent, unsigned baud, Lateness lateness) {
  const double bit = 1000000.0 / baud;
  Seen seen;
  softuart::Receiver receiver(baud);
  double t = 10000.0;
  bool level = true;
  std::size_t index = 0;
  for (const std::uint8_t byte : sent) {
    for (const bool b : softuart::frame_bits(byte)) {
      if (b != level) {
        level = b;
        const std::int64_t late = lateness(index++);
        seen.lateness.add(late);
        receiver.edge(static_cast<std::uint64_t>(t + static_cast<double>(late) + 0.5), level);
      }
      t += bit;
    }
    receiver.poll(static_cast<std::uint64_t>(t), seen.bytes);   // the task looks every byte or so
  }
  receiver.poll(static_cast<std::uint64_t>(t + 10 * bit), seen.bytes);
  seen.framing_errors = receiver.framing_errors();
  return seen;
}

static std::uint64_t g_seed = 1;
static std::uint32_t next_random() { g_seed = g_seed * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<std::uint32_t>(g_seed >> 33); }
static std::vector<std::uint8_t> traffic(std::size_t n) { std::vector<std::uint8_t> out; for (std::size_t i = 0; i < n; ++i) out.push_back(static_cast<std::uint8_t>(next_random())); return out; }

static void test_the_record() {
  jitter::Recorder r;
  CHECK(r.count() == 0 && r.spread_us() == 0.0 && r.mean() == 0.0 && r.percentile(0.5) == 0);
  for (int v = 1; v <= 1000; ++v) r.add(v);
  CHECK(r.count() == 1000 && r.min() == 1 && r.max() == 1000 && r.spread_us() == 999.0 && r.mean() == 500.5);
  CHECK(r.percentile(0.0) == 1 && r.percentile(1.0) == 1000 && r.percentile(0.5) >= 500 && r.percentile(0.5) <= 501 && r.percentile(0.99) >= 989 && r.percentile(0.99) <= 991);
  r.reset();
  CHECK(r.count() == 0);
  // more values than are kept: the extremes and the mean are still of all of them
  for (std::size_t i = 0; i < jitter::Recorder::kKept + 10; ++i) r.add(i == jitter::Recorder::kKept + 5 ? 5000 : 3);
  CHECK(r.count() == jitter::Recorder::kKept + 10 && r.max() == 5000 && r.min() == 3);
}

static void test_the_limits_file(const std::string& path) {
  jitter::Limits parsed;
  std::ifstream file(path);
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  CHECK(!text.empty() && jitter::limits_from_json(text, parsed));
  g_limits = parsed;
  CHECK(jitter::valid(g_limits) && g_limits.fail <= 0.5);
  jitter::Limits kept{0.2, 0.4}, out = kept;
  CHECK(!jitter::limits_from_json("{\"warn\":0.4,\"fail\":0.3}", out) && out.warn == kept.warn);   // not in order
  CHECK(!jitter::limits_from_json("{\"warn\":0.3,\"fail\":0.6}", out));                        // above half a bit
  CHECK(!jitter::limits_from_json("{\"warn\":0,\"fail\":0.3}", out) && !jitter::limits_from_json("{\"warn\":\"a\",\"fail\":0.3}", out) && !jitter::limits_from_json("{\"warn\":0.3}", out) && !jitter::limits_from_json("[]", out) && !jitter::limits_from_json("", out));
  CHECK(jitter::limits_from_json("{\"warn\":0.25,\"fail\":0.4}", out) && out.warn == 0.25 && out.fail == 0.4);
}

static void test_the_verdict() {
  const jitter::Limits limits{0.30, 0.45};
  const auto record_with_spread = [](std::int64_t spread, std::size_t n = 200) { jitter::Recorder r; for (std::size_t i = 0; i < n; ++i) r.add(i == 0 ? 0 : (i == 1 ? spread : 1)); return r; };
  // 19200 baud: a bit is 52.08 us; 30 % is 15.6 us, 45 % is 23.4 us
  CHECK(jitter::judge(record_with_spread(10), 19200, limits) == jitter::Verdict::kOk);
  CHECK(jitter::judge(record_with_spread(15), 19200, limits) == jitter::Verdict::kOk);
  CHECK(jitter::judge(record_with_spread(16), 19200, limits) == jitter::Verdict::kMarginal);
  CHECK(jitter::judge(record_with_spread(23), 19200, limits) == jitter::Verdict::kMarginal);
  CHECK(jitter::judge(record_with_spread(24), 19200, limits) == jitter::Verdict::kTooMuch);
  // the same spread is nothing at a slow speed
  CHECK(jitter::judge(record_with_spread(24), 9600, limits) == jitter::Verdict::kOk && jitter::judge(record_with_spread(24), 2400, limits) == jitter::Verdict::kOk);
  CHECK(jitter::judge(record_with_spread(10, 99), 19200, limits) == jitter::Verdict::kNoData && jitter::judge(jitter::Recorder{}, 9600, limits) == jitter::Verdict::kNoData && jitter::judge(record_with_spread(10), 0, limits) == jitter::Verdict::kNoData);
  CHECK(std::string(jitter::verdict_name(jitter::Verdict::kOk)) == "ok" && std::string(jitter::verdict_name(jitter::Verdict::kTooMuch)) == "too_much");
  // which speed a lateness allows
  CHECK(jitter::fastest_safe_baud(record_with_spread(10), limits) == 19200);
  CHECK(jitter::fastest_safe_baud(record_with_spread(40), limits) == 4800);    // 40 us is 38 % of a bit at 9600 (marginal) and 19 % at 4800
  CHECK(jitter::fastest_safe_baud(record_with_spread(200), limits) == 1200);   // 200 us is 48 % of a bit at 2400 and 24 % at 1200
  CHECK(jitter::fastest_safe_baud(record_with_spread(5000), limits) == 0);
  CHECK(jitter::bit_us(9600) > 104.1 && jitter::bit_us(9600) < 104.2 && jitter::bit_us(0) == 0.0);
}

// A line the verdict calls fine (or marginal) is read exactly, whatever the interrupt did within that spread.
static void test_a_line_that_passes_is_read_exactly() {
  int ok = 0, marginal = 0, too_much = 0;
  for (const unsigned baud : {2400u, 9600u, 19200u}) {
    const double bit = 1000000.0 / baud;
    for (int run = 0; run < 60; ++run) {
      g_seed = 1000 + static_cast<std::uint64_t>(run) * 7919 + baud;
      const std::vector<std::uint8_t> sent = traffic(120);
      const double worst_late = bit * (0.05 + 0.65 * (run % 20) / 19.0);   // from 5 % to 70 % of a bit
      const int model = run % 3;
      const auto lateness = [&](std::size_t index) -> std::int64_t {
        switch (model) {
          case 0: return static_cast<std::int64_t>(next_random() % (static_cast<std::uint32_t>(worst_late) + 1));                                  // anywhere in the range
          case 1: return (next_random() % 10 == 0) ? static_cast<std::int64_t>(worst_late) : 2;                                                      // mostly quick, now and then held up by the radio
          default: return index % 2 == 0 ? 0 : static_cast<std::int64_t>(worst_late);                                                              // the worst order: alternating
        }
      };
      const Seen seen = play(sent, baud, lateness);
      const jitter::Verdict verdict = jitter::judge(seen.lateness, baud, g_limits);
      if (verdict == jitter::Verdict::kOk) ++ok; else if (verdict == jitter::Verdict::kMarginal) ++marginal; else ++too_much;
      if (verdict == jitter::Verdict::kOk || verdict == jitter::Verdict::kMarginal) CHECK(seen.bytes == sent && seen.framing_errors == 0);
    }
  }
  CHECK(ok > 30 && marginal > 5 && too_much > 30);   // the runs cover the three cases
}

// And the limit is not absurdly careful: an interrupt that is late by 0.7 of a bit at every other edge of the worst line is misread.
static void test_a_line_far_past_the_limit_is_misread() {
  for (const unsigned baud : {2400u, 9600u, 19200u}) {
    const double bit = 1000000.0 / baud;
    const std::vector<std::uint8_t> sent(100, 0x55);
    const Seen seen = play(sent, baud, [&](std::size_t index) -> std::int64_t { return index % 2 == 0 ? 0 : static_cast<std::int64_t>(0.7 * bit); });
    CHECK(jitter::judge(seen.lateness, baud, g_limits) == jitter::Verdict::kTooMuch);
    CHECK(seen.bytes != sent);
  }
}

int main(int argc, char** argv) {
  test_the_record();
  test_the_limits_file(argc > 1 ? argv[1] : "");
  test_the_verdict();
  test_a_line_that_passes_is_read_exactly();
  test_a_line_far_past_the_limit_is_misread();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
