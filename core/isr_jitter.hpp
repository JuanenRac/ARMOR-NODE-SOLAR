// ARMOR-NODE-SOLAR - the lateness of the interrupt that times the edges of an emulated serial line, and whether a line of a given speed can bear it. Pure: tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// An emulated port notes the time of every edge in an interrupt (core/soft_uart.hpp). The interrupt is never early, but it can be late, by a different amount at each edge (the radio of
// Wi-Fi, a flash write or another interrupt holds the processor). A byte is read from the times of the edges counted from its start bit, and every one of them is late by its own amount, so
// what matters is the DIFFERENCE between the lateness of the start edge and that of any later edge of the same byte: the byte is read right as long as that difference stays below half a bit
// (the instant the line is looked at is half a bit from the edge on either side). The spread of the lateness (the latest minus the earliest ever seen) is an upper bound of that difference.
//
// This file keeps the record of the lateness (on a computer from a simulated interrupt, on a bench from a loopback: the node's own transmitter wired to a receiving pin gives the time each edge
// was made and the time the interrupt saw it) and says whether the speed is safe. The limits are in docs/UART_JITTER_LIMITS.json, not here.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "json.hpp"

namespace armor::jitter {

// Fractions of a bit: below `warn` the speed is fine, up to `fail` it works but with little to spare, above it a byte can be misread. Half a bit is what the arithmetic allows; the limit
// is under it for the whole microseconds the times are counted in and for the edges of the line that are not exact.
struct Limits {
  double warn = 0.30;
  double fail = 0.45;
};
inline bool valid(const Limits& l) { return std::isfinite(l.warn) && std::isfinite(l.fail) && l.warn > 0.0 && l.warn < l.fail && l.fail <= 0.5; }

// Reads {"warn": 0.30, "fail": 0.45}. False (and `out` unchanged) for a document that is not that or that is not a valid pair.
inline bool limits_from_json(std::string_view text, Limits& out) {
  json::Value document;
  if (!json::parse(text, document) || !document.is_object()) return false;
  const json::Value* warn = document.get("warn");
  const json::Value* fail = document.get("fail");
  if (warn == nullptr || fail == nullptr || !warn->is_number() || !fail->is_number()) return false;
  const Limits made{warn->number, fail->number};
  if (!valid(made)) return false;
  out = made;
  return true;
}

inline double bit_us(unsigned baud) { return baud == 0 ? 0.0 : 1000000.0 / static_cast<double>(baud); }

// The lateness of the interrupt (microseconds) at every edge seen. Keeps the extremes and the mean always; the individual values (for the percentiles) up to `kKept`.
class Recorder {
 public:
  static constexpr std::size_t kKept = 65536;

  void add(std::int64_t lateness_us) {
    if (count_ == 0) min_ = max_ = lateness_us;
    min_ = std::min(min_, lateness_us);
    max_ = std::max(max_, lateness_us);
    sum_ += static_cast<double>(lateness_us);
    ++count_;
    if (kept_.size() < kKept) kept_.push_back(lateness_us);
  }
  void reset() { *this = Recorder{}; }

  std::size_t count() const { return count_; }
  std::int64_t min() const { return min_; }
  std::int64_t max() const { return max_; }
  double mean() const { return count_ == 0 ? 0.0 : sum_ / static_cast<double>(count_); }
  // The latest minus the earliest ever seen.
  double spread_us() const { return count_ == 0 ? 0.0 : static_cast<double>(max_ - min_); }
  // The value under which `fraction` (0..1) of the kept values lie.
  std::int64_t percentile(double fraction) const {
    if (kept_.empty()) return 0;
    std::vector<std::int64_t> sorted = kept_;
    const std::size_t index = static_cast<std::size_t>(std::floor(std::min(1.0, std::max(0.0, fraction)) * static_cast<double>(sorted.size() - 1) + 0.5));
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(index), sorted.end());
    return sorted[index];
  }

 private:
  std::size_t count_ = 0;
  std::int64_t min_ = 0, max_ = 0;
  double sum_ = 0.0;
  std::vector<std::int64_t> kept_;
};

enum class Verdict { kNoData, kOk, kMarginal, kTooMuch };

// Whether a line of `baud` can bear this lateness: the spread, as a fraction of a bit, against the limits. A record that is too short to say anything (fewer than 100 edges) says so.
inline Verdict judge(const Recorder& record, unsigned baud, const Limits& limits) {
  if (record.count() < 100 || baud == 0) return Verdict::kNoData;
  const double fraction = record.spread_us() / bit_us(baud);
  if (fraction <= limits.warn) return Verdict::kOk;
  if (fraction <= limits.fail) return Verdict::kMarginal;
  return Verdict::kTooMuch;
}
inline const char* verdict_name(Verdict v) {
  switch (v) {
    case Verdict::kOk: return "ok";
    case Verdict::kMarginal: return "marginal";
    case Verdict::kTooMuch: return "too_much";
    case Verdict::kNoData: return "no_data";
  }
  return "no_data";
}

// The fastest of the speeds of the family that this lateness can bear (spread within `limits.warn` of a bit), or 0 when not even the slowest does.
inline unsigned fastest_safe_baud(const Recorder& record, const Limits& limits) {
  unsigned best = 0;
  for (const unsigned baud : {1200u, 2400u, 4800u, 9600u, 19200u}) {
    if (judge(record, baud, limits) == Verdict::kOk) best = baud;
  }
  return best;
}

}  // namespace armor::jitter
