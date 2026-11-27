// ARMOR-NODE-SOLAR - the panel that does not answer: the rules by which the node, when it is asked to (system.web_recovery, off by default), restarts itself. No hardware.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A node whose web server is stuck (a request that never ends, no socket or no memory to take a connection) can keep reporting to the broker for days with nobody able to open its panel. With
// the setting on, the node asks itself every few seconds: a request to its own plain port over the loopback address, and a piece of work handed to the task of the HTTPS server. When one of the
// two has gone unanswered without a break for the time of the setting, the node restarts, keeps why in the memory that survives a software reset and says it at the next start. Never more than
// three such restarts in an hour of running time, and never while something that must not be cut is going on (a firmware upload or download, an operation on a battery).
#pragma once
#include <cstdint>
#include <string>

namespace armor::webrecovery {

constexpr int kMinAfterS = 60, kMaxAfterS = 3600, kDefaultAfterS = 300;
constexpr std::uint32_t kProbeEveryMs = 15u * 1000u;
constexpr std::uint64_t kGraceMs = 120u * 1000u;          // after a start nothing is judged: the network and the servers are still coming up
constexpr std::uint64_t kHeldSayEveryMs = 600u * 1000u;   // with the limit used up, how often it is said again

constexpr std::uint32_t kPlainSilent = 1u;   // the node's own request to port 80 got no HTTP answer
constexpr std::uint32_t kTlsSilent = 2u;     // the task of the HTTPS server did not run the work it was handed

enum class Action {
  kNone,
  kRestart,   // restart now: it has been silent for the whole time
  kHeld,      // it should, and the limit of restarts is used up: say it and stay up
};

class Watch {
 public:
  explicit Watch(int after_s) : after_ms_(static_cast<std::uint64_t>(after_s < kMinAfterS ? kMinAfterS : after_s > kMaxAfterS ? kMaxAfterS : after_s) * 1000u) {}

  // One look. `silent` is what did not answer (0: all did). `excused` while something that explains a silence, or that must not be cut, is going on.
  Action look(std::uint64_t now_ms, std::uint32_t silent, bool excused, bool may_restart) {
    if (now_ms < kGraceMs || excused || silent == 0) {
      failing_ = false;
      said_ = false;
      return Action::kNone;
    }
    if (!failing_) {
      failing_ = true;
      since_ms_ = now_ms;
      silent_ = 0;
    }
    silent_ |= silent;
    if (now_ms - since_ms_ < after_ms_) return Action::kNone;
    if (may_restart) return Action::kRestart;
    if (said_ && now_ms - said_ms_ < kHeldSayEveryMs) return Action::kNone;
    said_ = true;
    said_ms_ = now_ms;
    return Action::kHeld;
  }
  std::uint32_t silent() const { return silent_; }
  std::uint32_t silent_for_s(std::uint64_t now_ms) const { return failing_ ? static_cast<std::uint32_t>((now_ms - since_ms_) / 1000u) : 0u; }

 private:
  std::uint64_t after_ms_;
  std::uint64_t since_ms_ = 0, said_ms_ = 0;
  std::uint32_t silent_ = 0;
  bool failing_ = false, said_ = false;
};

// What is kept across the restart, with a check number so that the garbage of a power-on is not taken for a record.
constexpr std::uint32_t kRecordMagic = 0x57524331u;
struct Record {
  std::uint32_t magic = 0;
  std::uint32_t silent = 0;
  std::uint32_t silent_for_s = 0;
  std::uint32_t check = 0;
};
inline std::uint32_t record_check(const Record& r) { return ((r.magic ^ 0x9E3779B9u) + r.silent * 2654435761u) ^ (r.silent_for_s * 16777619u); }
inline Record make_record(std::uint32_t silent, std::uint32_t silent_for_s) {
  Record r;
  r.magic = kRecordMagic;
  r.silent = silent;
  r.silent_for_s = silent_for_s;
  r.check = record_check(r);
  return r;
}
inline bool record_valid(const Record& r) { return r.magic == kRecordMagic && r.check == record_check(r) && r.silent >= 1 && r.silent <= 3; }

inline const char* silent_name(std::uint32_t silent) { return silent == (kPlainSilent | kTlsSilent) ? "http, https" : silent == kTlsSilent ? "https" : "http"; }

// The error line of the log (the health report takes it to the server, which shows it as the last error of the node).
inline std::string restart_text(const Record& r) { return "web recovery: restarted after " + std::to_string(r.silent_for_s) + " s without an answer (" + silent_name(r.silent) + ")"; }

}  // namespace armor::webrecovery
