// ARMOR-NODE-SOLAR - why a port that hears bytes reads nothing: what those bytes look like, and the one line the node says about it.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A port is "garbled" when bytes arrive and none of them is an answer. What they look like says where to look:
//   flat    nearly all 0x00 or 0xFF: nothing is driving the receive line (a loose or missing wire, no level converter, the equipment off);
//   echo    what was sent comes back: transmit and receive are joined, or the cable mirrors them;
//   noise   a text protocol hears mostly unprintable bytes: the speed, the level or the polarity is wrong;
//   text    readable text that is not the expected answer: another kind of equipment, or another mode of it;
//   frames  a binary protocol hears bytes that never make a whole frame: speed, polarity or another protocol.
// The log line is short on purpose: the node's health report carries the last error line in 96 characters, and the server reads this one back (ARMOR-SERVER, node_health.ts).
// It touches no hardware, so it is tested on a computer. Nothing here has run on a board.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace armor::solar::portdiag {

constexpr std::size_t kClassifiedBytes = 256;   // how much of what was heard last is looked at
constexpr std::size_t kSampleBytes = 6;         // how much of it is shown

// The cause, or "" when nothing was heard.
inline const char* classify(const std::vector<std::uint8_t>& heard, const std::vector<std::uint8_t>& sent, bool text_protocol) {
  if (heard.empty()) return "";
  std::size_t flat = 0, printable = 0;
  for (const std::uint8_t b : heard) {
    if (b == 0x00 || b == 0xFF) ++flat;
    if ((b >= 0x20 && b < 0x7F) || b == '\r' || b == '\n' || b == '\t') ++printable;
  }
  if (flat * 10 >= heard.size() * 9) return "flat";
  if (!sent.empty() && heard.size() >= sent.size() && std::equal(sent.begin(), sent.end(), heard.end() - static_cast<std::ptrdiff_t>(sent.size()))) return "echo";
  if (!text_protocol) return "frames";
  return printable * 10 < heard.size() * 7 ? "noise" : "text";
}

// The last bytes heard, as hexadecimal.
inline std::string sample_hex(const std::vector<std::uint8_t>& heard) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = heard.size() > kSampleBytes ? heard.size() - kSampleBytes : 0; i < heard.size(); ++i) {
    if (!out.empty()) out += ' ';
    out += kDigits[heard[i] >> 4];
    out += kDigits[heard[i] & 15];
  }
  return out;
}

// "port 3 unreadable flat 115200bd rx16 tx15 inv n230 [00 00 00 00 00 00]" ("inv" only when the line is inverted).
inline std::string log_line(int port, const std::string& cause, int baud, int rx, int tx, bool invert, std::uint32_t bytes_rx, const std::string& sample) {
  return "port " + std::to_string(port) + " unreadable " + cause + " " + std::to_string(baud) + "bd rx" + std::to_string(rx) + " tx" + std::to_string(tx) + (invert ? " inv" : "") + " n" +
         std::to_string(bytes_rx) + " [" + sample + "]";
}

// When to say it. The health report only carries the LAST error line, once a minute, so the ports take turns: never two lines closer than kGapMs (each one stays the last
// long enough to be reported), and one port again no sooner than kRepeatMs while it stays unreadable. With many ports unreadable the turn goes to the one that has waited
// longest (one never said goes first), so none is left out: a port asks again and again while it fails, and a request older than kWantMs no longer holds the turn.
class LogSchedule {
 public:
  static constexpr std::uint64_t kGapMs = 90'000;
  static constexpr std::uint64_t kRepeatMs = 5ULL * 60ULL * 1000ULL;
  static constexpr std::uint64_t kWantMs = 2ULL * 60ULL * 1000ULL;
  static constexpr std::size_t kPorts = 16;

  // Asks for the turn of `port`, which is unreadable now. True: say it.
  bool take(std::size_t port, std::uint64_t now_ms) {
    if (port >= kPorts) return false;
    wants_[port] = true;
    wanted_ms_[port] = now_ms;
    if (any_ && now_ms - any_ms_ < kGapMs) return false;
    if (!eligible(port, now_ms)) return false;
    for (std::size_t other = 0; other < kPorts; ++other) {
      if (other != port && wants_[other] && now_ms - wanted_ms_[other] < kWantMs && eligible(other, now_ms) && waited_since(other) < waited_since(port)) return false;
    }
    said_[port] = any_ = true;
    at_ms_[port] = any_ms_ = now_ms;
    return true;
  }
  // The port reads again: if it fails once more, that is said as soon as the gap allows.
  void clear(std::size_t port) { if (port < kPorts) said_[port] = wants_[port] = false; }

 private:
  bool eligible(std::size_t port, std::uint64_t now_ms) const { return !said_[port] || now_ms - at_ms_[port] >= kRepeatMs; }
  std::uint64_t waited_since(std::size_t port) const { return said_[port] ? at_ms_[port] + 1 : 0; }   // 0: never said

  std::array<bool, kPorts> said_{}, wants_{};
  std::array<std::uint64_t, kPorts> at_ms_{}, wanted_ms_{};
  bool any_ = false;
  std::uint64_t any_ms_ = 0;
};

}  // namespace armor::solar::portdiag
