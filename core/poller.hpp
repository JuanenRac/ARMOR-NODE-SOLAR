// ARMOR-NODE-SOLAR - what a serial port does with the equipment on it: when to ask, what to ask, how to read the answers and when a reading is whole.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One Poller per port. It touches no hardware: the firmware feeds it the bytes that arrive and the time, and sends the bytes it returns, so the whole
// exchange (the order of the requests, the timeouts, a reply that is refused or garbled, a battery with several modules) is tested on a computer with a
// stand-in for the equipment.
//
//   Voltronic / MPP Solar inverter:  QPIGS (the readings), QMOD (the mode), QPIWS (the warnings), each answered before the next is asked. A reading is
//                                    published only when all three came back whole: a missing warning list must not look like "no warnings".
//                                    Three dialects: "pi30" (that one), "revo" (QPIGS arranged differently, no warnings asked, replies that may end with a
//                                    one-byte checksum), "pi00" (the same words with NO CRC, as the oldest units want them) and "pi18" (^P005GS, ^P006MOD,
//                                    ^P005FWS) and "pi17" (^P003GS, ^P003PS, ^P004MOD, ^P003WS, no CRC). PI16 hybrids answer "pi00". "auto" (the default) asks in pi30, then without CRC, then in pi18 and pi17, and
//                                    keeps to the one that answered; a reply that ends with the checksum instead of the CRC makes it revo.
//   Pylontech battery (console):     `pwr` (the table of the modules), then for every module that is there `bat <n>` (its cells and remaining charge), and every half hour
//                                    `info <n>` (model, rated capacity) and `stat <n>` (cycles), whose answers are kept in between. The extras are optional: if they do not
//                                    answer, the reading goes out without them, and a module still without its sheet is asked again (one per cycle).
//   ANT-BMS battery (serial):        one request, one whole frame back (core/ant_bms.hpp). Its firmware speaks one of two protocols: the port asks in the old one
//                                    and, when nothing answers, in the new one, and keeps to the one that answered (it looks again after three misses).
//   Raw:                             nothing is asked; what arrives is kept so it can be looked at (a protocol that is not decoded yet).
//   Other protocols:                 Modbus RTU inverters, DALY, JBD and JK battery boards and Victron VE.Direct each have a driver (core/driver_*.hpp, listed in
//                                    core/solar_protocols.hpp); the poller only moves their bytes and times their questions.
//
// Nothing here has run against an inverter or a battery.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ant_bms.hpp"
#include "port_diag.hpp"
#include "pylontech.hpp"
#include "solar_config.hpp"
#include "solar_json.hpp"
#include "solar_protocols.hpp"
#include "voltronic.hpp"
#include "voltronic_pi18.hpp"

namespace armor::solar {

// The last bytes a port received, to look at in the panel: kept as they came, shown as hexadecimal with the printable characters beside them.
class RawLog {
 public:
  static constexpr std::size_t kCapacity = 2048;
  void add(const std::uint8_t* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
      bytes_[head_] = data[i];
      head_ = (head_ + 1) % kCapacity;
      if (size_ < kCapacity) ++size_;
    }
    total_ += length;
  }
  std::size_t total() const { return total_; }
  // The last `count` bytes kept, oldest first.
  std::vector<std::uint8_t> tail(std::size_t count) const {
    std::vector<std::uint8_t> out;
    const std::size_t n = count < size_ ? count : size_;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.push_back(bytes_[(head_ + kCapacity - n + i) % kCapacity]);
    return out;
  }
  // The kept bytes, oldest first, sixteen to a line: "48 65 6C 6C 6F 0D  |Hello.|".
  std::string dump(std::size_t max_lines = 128) const {
    std::string out;
    const std::size_t start = (head_ + kCapacity - size_) % kCapacity;
    std::size_t first = 0;
    const std::size_t lines = (size_ + 15) / 16;
    if (lines > max_lines) first = (lines - max_lines) * 16;
    for (std::size_t line = first; line < size_; line += 16) {
      std::string hex, text;
      for (std::size_t i = line; i < line + 16 && i < size_; ++i) {
        const std::uint8_t b = bytes_[(start + i) % kCapacity];
        static const char kDigits[] = "0123456789ABCDEF";
        hex += kDigits[b >> 4];
        hex += kDigits[b & 15];
        hex += ' ';
        text += (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
      }
      while (hex.size() < 48) hex += ' ';
      out += hex + " |" + text + "|\n";
    }
    return out;
  }
 private:
  std::array<std::uint8_t, kCapacity> bytes_{};
  std::size_t head_ = 0, size_ = 0, total_ = 0;
};

struct PortStats {
  std::uint32_t bytes_rx = 0, bytes_tx = 0;
  std::uint32_t replies_ok = 0;       // whole, checked answers
  std::uint32_t replies_bad = 0;      // bytes that arrived but were not a whole, checked answer (wrong CRC, "NAK", not a table)
  std::uint32_t timeouts = 0;         // a request nobody answered in time
  std::uint32_t readings = 0;         // whole readings made (each one becomes a message)
  std::string last_error;             // "", "timeout", "crc", "nak", "format"
};

class Poller {
 public:
  static constexpr int kMaxModules = 16;   // a console answers for up to sixteen, and so does the message; with no limit in the settings all of them are asked
  static constexpr std::uint64_t kExtrasRetryMs = 5ULL * 60ULL * 1000ULL;   // after kExtrasQuickTries unanswered tries, a module still without its sheet is asked again this often
  static constexpr int kExtrasQuickTries = 3;                               // and before that, in the next cycles that come to it
  static constexpr std::uint64_t kQuietMs = 300;                            // after a question that timed out, the next one waits for this much silence (a late answer is not its answer)
  static constexpr std::uint64_t kSettleMaxMs = 3000;                       // but never longer than this
  static constexpr std::uint64_t kVoltronicTimeoutMs = 2500;
  static constexpr std::uint64_t kConsoleTableTimeoutMs = 5000;
  static constexpr std::uint64_t kConsoleExtraTimeoutMs = 3000;
  static constexpr std::uint64_t kConsoleSlowTimeoutMs = 10000;   // the first time a missing sheet is asked again: a console shows no prompt while it still waits for the module that was asked
  static constexpr std::uint64_t kAntTimeoutMs = 2000;
  static constexpr std::uint64_t kDriverTimeoutMs = 3000;   // a driver's question (the longest answer, JK's, is about 300 bytes)
  static constexpr int kGiveUpTries = 3;                    // QMOD or QPIWS that has never answered this many cycles is done without
  static constexpr std::size_t kDiagnosticBytes = 48;       // how much of the last question and answer the panel shows
  static constexpr std::uint64_t kExtrasRefreshMs = 30ULL * 60ULL * 1000ULL;   // how long the identity and the history of a module are kept
  static constexpr std::uint64_t kRatingsEveryMs = 10ULL * 60ULL * 1000ULL;    // an inverter's ratings and settings (QPIRI) are asked again this often: they can be changed at the unit
  static constexpr std::uint64_t kEnergyEveryMs = 5ULL * 60ULL * 1000ULL;      // and the energy the panels have made in all (QET)
  static constexpr int kIdentityTries = 2;                                    // an inverter that does not know QID, QVFW, QPIRI or QET is asked twice and then left alone

  // `cells_per_cycle`: a Pylontech stack of several modules takes long to read cell by cell on a line that other ports share; with a number here (1 to 8) each cycle reads the
  // cells of that many modules, in rotation, and the others keep the cells of their last turn. 0 (the default): every module every cycle.
  Poller(config::Kind kind, std::string node_id, std::string device, int poll_s, int max_modules, const std::string& dialect = "auto", int cells_per_cycle = 0)
      : kind_(kind), node_id_(std::move(node_id)), device_(std::move(device)), poll_ms_(static_cast<std::uint64_t>(poll_s) * 1000ULL), max_modules_(max_modules > 0 ? max_modules : kMaxModules),
        cells_per_cycle_(cells_per_cycle > 0 ? cells_per_cycle : 0) {
    if (dialect == "pi18") { dialect_ = Dialect::kPi18; auto_ = false; }
    else if (dialect == "revo") { dialect_ = Dialect::kRevo; auto_ = false; }
    else if (dialect == "pi41") { dialect_ = Dialect::kPi41; auto_ = false; }
    else if (dialect == "pi34") { dialect_ = Dialect::kPi34; auto_ = false; }
    else if (dialect == "pi00") { dialect_ = Dialect::kPi00; auto_ = false; }
    else if (dialect == "pi17") { dialect_ = Dialect::kPi17; auto_ = false; }
    else if (dialect == "pi30") auto_ = false;
    known_ = !auto_;
    driver_ = protocols::make_driver(kind, dialect);
  }

  // The bytes to send now, or none. Call it often (every few tens of milliseconds).
  std::vector<std::uint8_t> next_tx(std::uint64_t now_ms) {
    if (kind_ == config::Kind::kRaw) return {};
    if (step_ != Step::kIdle && now_ms >= deadline_ms_) on_timeout(now_ms);
    if (!pending_.empty()) return still_settling(now_ms) ? std::vector<std::uint8_t>() : take_pending();
    // a reading that has not been taken yet is not overwritten by the next cycle
    if (step_ == Step::kIdle && !ready_ && now_ms >= next_cycle_ms_) begin_cycle(now_ms);
    if (pending_.empty() || still_settling(now_ms)) return {};
    return take_pending();
  }

  // ---- what the panel shows to tell why a port does not read ----
  // Three lines put before the raw dump: what was asked last (its name and its bytes in hexadecimal), what has been counted and the last error.
  std::string diagnosis() const {
    if (kind_ == config::Kind::kRaw) return "";
    return "asked: " + asking() + (tx_plain_ ? " (no CRC)" : "") + " | " + (last_tx_.empty() ? std::string("nothing sent yet") : last_sent_hex()) + "\n" + tried() + "bytes sent " + std::to_string(stats_.bytes_tx) + ", heard " +
           std::to_string(stats_.bytes_rx) + ", answers " + std::to_string(stats_.replies_ok) + ", refused " + std::to_string(stats_.replies_bad) + ", silences " +
           std::to_string(stats_.timeouts) + "\nlast error: " + (stats_.last_error.empty() ? std::string("none") : stats_.last_error) + "\n";
  }
  std::string last_sent_hex() const { return hex_of(last_tx_); }
  static std::string hex_of(const std::vector<std::uint8_t>& bytes) {
    std::string out;
    static const char kDigits[] = "0123456789ABCDEF";
    for (const std::uint8_t b : bytes) {
      if (!out.empty()) out += ' ';
      out += kDigits[b >> 4];
      out += kDigits[b & 15];
    }
    return out;
  }
  // While "auto" has not found the dialect: every first question it sends in turn, byte by byte (built again here, nothing is stored).
  std::string tried() const {
    if (kind_ != config::Kind::kVoltronic || !auto_ || known_) return "";
    std::vector<std::uint8_t> frame;
    voltronic::build_command("QPIGS", frame);
    std::string out = "automatic, in turn (now " + std::string(dialect_name()) + "): pi30 = " + hex_of(frame);
    voltronic::build_plain_command("QPIGS", frame);
    out += "; pi00, no CRC = " + hex_of(frame);
    voltronic::pi18::build_command("GS", frame);
    out += "; pi18 = " + hex_of(frame);
    voltronic::pi17::build_command("GS", frame);
    return out + "; pi17 = " + hex_of(frame) + "\n";
  }
  std::string asking() const {
    if (driver_) return driver_->command();
    std::string text;
    std::size_t first = 0;   // a console question may start with a bare Enter that wakes the console
    while (first < last_tx_.size() && last_tx_[first] == '\r') ++first;
    // a Voltronic question is its letters, a CRC (two bytes that may look like text) and CR
    const std::size_t tail = tx_plain_ ? 1 : 3;   // CR, and the CRC when there is one
    if (kind_ == config::Kind::kVoltronic && last_tx_.size() > tail && last_tx_[0] != '^') { text.assign(last_tx_.begin(), last_tx_.end() - static_cast<std::ptrdiff_t>(tail)); return text; }
    for (std::size_t i = first; i < last_tx_.size(); ++i) { const std::uint8_t b = last_tx_[i]; if (b < 0x20 || b >= 0x7F || text.size() >= 24) break; text += static_cast<char>(b); }
    return text;
  }
  // Which of the two messages this port's readings become.
  bool is_inverter() const { return driver_ ? driver_->inverter() : kind_ == config::Kind::kVoltronic; }

  // Optional readings of an inverter in the standard dialect, asked after the three that make the message and never at the cost of them: the second PV input (QPIGS2; an
  // inverter that has only one answers NAK, and after two misses it is not asked again) and the units of a parallel system (QPGS0 to QPGS<n-1>; a unit that is not there is skipped).
  void set_optional(bool pv2, int parallel_units) { pv2_wanted_ = pv2; parallel_ = parallel_units < 0 ? 0 : parallel_units > 10 ? 10 : parallel_units; }
  // Whether the unit is asked what it says about itself (its serial number, firmware, ratings and settings, and the energy the panels have made in all): off until the firmware turns it on.
  void set_identity(bool wanted) { identity_wanted_ = wanted; }
  // The further readings, all of them questions that only read, off until the firmware turns them on: `pwrsys` of a Pylontech stack, the energy counters, flags, defaults,
  // current lists and battery of a Voltronic inverter (one of those questions per cycle), and register 184 of an SMG II (which says whether its other registers mean what we read).
  void set_extended(bool wanted) { extended_wanted_ = wanted; if (driver_) driver_->set_layout_guard(wanted); }
  static constexpr std::uint64_t kExtendedEveryMs = 5ULL * 60ULL * 1000ULL;      // counters and the battery as the inverter sees it
  static constexpr std::uint64_t kExtendedRarelyMs = 30ULL * 60ULL * 1000ULL;    // flags, defaults and the current lists; also how long a refused `pwrsys` is left alone
  static constexpr int kExtendedTries = 2;
  // Where the requests this poller sends (tx = true) and the bytes it hears are copied, for the capture of the traffic (core/frame_capture.hpp); empty = nowhere. Only the polling goes through here.
  using CaptureSink = std::function<void(bool tx, const std::uint8_t* data, std::size_t length)>;
  void set_capture(CaptureSink sink) { capture_ = std::move(sink); }
  // Whether the second PV input has answered (in the last cycle), and what it said.
  bool has_pv2() const { return has_pv2_; }

  // Whether an exchange is under way (a request waits for its answer, or one is about to go out): a line shared with other ports keeps its channel on this one until it is over.
  bool busy() const { return step_ != Step::kIdle || !pending_.empty(); }
  // Gives up an exchange that is under way (a shared line is taken from this port in the middle of it): what it had is dropped, so its next turn begins clean.
  void abort(std::uint64_t now_ms) { if (busy()) abandon(now_ms); }
  // Whether the next cycle is due and nothing stands in its way (a reading not taken yet is not overwritten). A raw port never asks, so it is never due.
  bool due(std::uint64_t now_ms) const { return kind_ != config::Kind::kRaw && step_ == Step::kIdle && !ready_ && pending_.empty() && now_ms >= next_cycle_ms_; }
  // Whether a whole reading waits to be taken.
  bool ready() const { return ready_; }

  // Bytes that arrived.
  void on_rx(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    if (length == 0) return;
    stats_.bytes_rx += static_cast<std::uint32_t>(length);
    last_rx_ms_ = now_ms;
    raw_.add(data, length);
    if (capture_) capture_(false, data, length);
    if (kind_ == config::Kind::kVoltronic) rx_voltronic(data, length, now_ms);
    else if (kind_ == config::Kind::kPylontech) rx_pylontech(data, length, now_ms);
    else if (kind_ == config::Kind::kAnt) rx_ant(data, length, now_ms);
    else if (driver_) rx_driver(data, length, now_ms);
  }

  // A whole reading, if one is ready: the topic and the JSON payload (timestamped with the node's clock, `wall_ms`). False when none is.
  bool take_message(std::uint64_t wall_ms, std::string& topic, std::string& payload) {
    if (!ready_) return false;
    ready_ = false;
    topic = solar::topic(node_id_, device_);
    if (topic.empty()) return false;
    if (driver_ && driver_->inverter()) { InverterExtras extras; extras.warnings_unknown = !driver_->out().warnings_known; if (const char* maker = protocol_maker(kind_)) { extras.manufacturer = maker; extras.manufacturer_deduced = true; } payload = inverter_json(node_id_, device_, wall_ms, mode_, status_, warnings_, &extras); }
    else if (kind_ == config::Kind::kVoltronic) { InverterExtras extras; if (extended_wanted_) extras.extended = &more_; extras.warnings_unknown = dialect_ == Dialect::kRevo || dialect_ == Dialect::kPi41 || dialect_ == Dialect::kPi34 || !warnings_read_; extras.has_pv2 = has_pv2_; extras.pv2 = pv2_; extras.units = units_; extras.serial = serial_; extras.firmware = firmware_; extras.has_ratings = have_ratings_; extras.ratings = ratings_; extras.has_energy = have_energy_; extras.energy_total_wh = energy_wh_; payload = inverter_json(node_id_, device_, wall_ms, mode_, status_, warnings_, &extras); }
    else {   // a Pylontech stack or an ANT-BMS (one module)
      const int present = last_present(modules_);
      if (present > highest_seen_) highest_seen_ = present;
      payload = battery_json(node_id_, device_, wall_ms, modules_, present > 0 ? highest_seen_ : 0, &system_);   // with none there: the empty stack
    }
    ++stats_.readings;
    last_reading_ms_ = last_rx_ms_;
    return true;
  }

  // "waiting" (no cycle has finished yet), "reporting", "silent" (nothing is coming), "garbled" (bytes come, none is an answer), "listening" (a raw port that hears bytes).
  std::string state(std::uint64_t now_ms) const {
    if (kind_ == config::Kind::kRaw) return stats_.bytes_rx == 0 ? "silent" : (now_ms - last_rx_ms_ < 10000 ? "listening" : "silent");
    if (slave_module_ > 0 || (driver_ && driver_->layout_unsupported())) return "garbled";   // it answers, but not with what can be read
    const std::uint64_t window = poll_ms_ * 3 + 8000;
    if (stats_.readings > 0 && now_ms - last_reading_ms_ <= window) return "reporting";
    if (stats_.bytes_rx == 0) return now_ms < poll_ms_ + 6000 && stats_.readings == 0 ? "waiting" : "silent";
    if (stats_.replies_ok == 0) return "garbled";
    return stats_.readings > 0 ? "silent" : "garbled";
  }

  // Why a port that is "garbled" reads nothing (core/port_diag.hpp): "flat", "echo", "noise", "text", "frames"; "" while it reads, hears nothing or has not failed often enough to say.
  std::string unreadable_cause(std::uint64_t now_ms) const {
    if (slave_module_ > 0) return "slave";   // one whole sheet of one battery is enough to say
    if (driver_ && driver_->layout_unsupported()) return "layout";   // it answers, with another register map
    if (kind_ == config::Kind::kRaw || stats_.timeouts + stats_.replies_bad < kGiveUpTries || state(now_ms) != "garbled") return "";
    const bool text = kind_ == config::Kind::kPylontech || kind_ == config::Kind::kVoltronic || kind_ == config::Kind::kVedirect;
    return portdiag::classify(raw_.tail(portdiag::kClassifiedBytes), last_tx_, text);
  }
  // The place in its stack of the battery whose console this port is on, when that is not the first one (0 otherwise).
  int slave_module() const { return slave_module_; }
  const pylontech::SlaveSheet& slave_sheet() const { return slave_sheet_; }
  std::string heard_sample() const { return portdiag::sample_hex(raw_.tail(portdiag::kSampleBytes)); }

  // What the last reading said, for the summary of the whole system (core/solar_system.hpp): the inverter's numbers and the modules of a battery stack.
  const voltronic::Status& status() const { return status_; }
  double pv_power_w() const { return status_.pv_w + (has_pv2_ ? pv2_.power_w : 0.0); }   // the same figure the inverter's message carries as pv_w
  const std::vector<pylontech::Module>& modules() const { return modules_; }

  const PortStats& stats() const { return stats_; }
  const RawLog& raw() const { return raw_; }
  config::Kind kind() const { return kind_; }
  // What the last ANT-BMS frame said besides the message (empty for any other kind, and until a frame was read): "old", "new" and the MOSFET and balancer codes.
  // For an inverter port: the dialect it speaks ("pi30", "revo", "pi18"), once it is known.
  std::string detail() const {
    if (kind_ == config::Kind::kVoltronic) return known_ ? dialect_name() : "";
    if (kind_ != config::Kind::kAnt || !ant_seen_) return "";
    return std::string(ant_reading_.protocol == ant::Protocol::kNew ? "new" : "old") + " charge=" + std::to_string(ant_reading_.charge_mos) + " discharge=" +
           std::to_string(ant_reading_.discharge_mos) + " balancer=" + std::to_string(ant_reading_.balancer) + " cells=" + std::to_string(ant_reading_.cells) +
           (ant_reading_.protecting ? " protecting" : "");
  }
  const std::string& device() const { return device_; }
  // Whether this is an ANT-BMS port that has found its BMS speaking the newer protocol (the only one whose settings can be read).
  bool ant_new_protocol() const { return kind_ == config::Kind::kAnt && ant_known_ && ant_protocol_ == ant::Protocol::kNew; }
  // The moment the node last heard something on this port, on the clock that was given (0: never).
  std::uint64_t last_rx_ms() const { return last_rx_ms_; }

 private:
  enum class Step { kIdle, kQpigs, kQmod, kQpiws, kQpigs2, kQpgs, kQid, kQvfw, kQpiri, kQet, kGs, kPs, kMod, kFws, kPwr, kBat, kInfo, kStat, kEvent, kSoh, kSheet, kSystem, kExtended, kAnt, kDriver };
  // The extra questions of an inverter, in the order they are asked. Those from kQed to kQlm need the date QT gives.
  enum class More { kQflag, kQdi, kQmchgcr, kQmuchgcr, kQbms, kQt, kQed, kQem, kQey, kQld, kQlm, kQly, kQlt, kCount };
  enum class Dialect { kPi30, kRevo, kPi18, kPi41, kPi34, kPi00, kPi17 };
  // What `info`, `stat`, `data event` and `soh` said of one module. at_ms: the last full round; tried_ms and misses: the tries while something is still missing.
  struct Extras { std::string model; double full_capacity_ah = -1; int cycles = -1, health_percent = -1; std::vector<std::pair<std::string_view, long long>> counters; pylontech::Event last_event; pylontech::DataPlate plate; std::string manufacturer; int cell_count = -1, console_baud = -1; std::string board_version, main_soft_version, soft_version; double max_charge_a = -1, max_discharge_a = -1; std::vector<int> soh_counts, soh_abnormal; std::uint64_t at_ms = 0, tried_ms = 0; int misses = 0; bool valid = false; };

  // Whether the sheet of a module is still missing: its rated capacity (`info`) or its history (`stat`).
  static bool incomplete(const Extras& e) { return e.full_capacity_ah < 0 || (e.cycles < 0 && e.counters.empty()); }
  // What was kept from earlier answers goes into whatever this module has not said now.
  // The maker a protocol tells by itself: a protocol that only one maker's equipment speaks. The Voltronic dialects and the Modbus maps are sold under many names: nothing is said.
  static const char* protocol_maker(config::Kind kind) {
    return kind == config::Kind::kAnt ? "ANT" : kind == config::Kind::kDaly ? "Daly" : kind == config::Kind::kJbd ? "JBD" : kind == config::Kind::kJk ? "JK"
           : kind == config::Kind::kVedirect ? "Victron Energy" : nullptr;
  }
  static void fill_from_kept(pylontech::Module& m, const Extras& e) {
    if (m.model.empty()) m.model = e.model;
    if (m.manufacturer.empty()) m.manufacturer = e.manufacturer;
    if (m.full_capacity_ah < 0) m.full_capacity_ah = e.full_capacity_ah;
    if (m.cycles < 0) m.cycles = e.cycles;
    if (m.health_percent < 0) m.health_percent = e.health_percent;
    if (m.counters.empty()) m.counters = e.counters;
    if (!m.last_event.valid) m.last_event = e.last_event;
    if (m.plate.chip == nullptr) m.plate = e.plate;   // all of it or none: a serial number is not mixed with another answer
    if (m.cell_count < 0) m.cell_count = e.cell_count;
    if (m.board_version.empty()) m.board_version = e.board_version;
    if (m.main_soft_version.empty()) m.main_soft_version = e.main_soft_version;
    if (m.soft_version.empty()) m.soft_version = e.soft_version;
    if (m.max_charge_a < 0) m.max_charge_a = e.max_charge_a;
    if (m.max_discharge_a < 0) m.max_discharge_a = e.max_discharge_a;
    if (m.console_baud < 0) m.console_baud = e.console_baud;
    if (m.soh_counts.empty()) { m.soh_counts = e.soh_counts; m.soh_abnormal = e.soh_abnormal; }
  }

  // A console that was still answering when its question timed out is let finish: the next question waits for silence, and its time to be answered starts when it goes out.
  bool still_settling(std::uint64_t now_ms) {
    if (!settling_) return false;
    if (now_ms - settle_started_ms_ < kSettleMaxMs && now_ms - last_rx_ms_ < kQuietMs) { deadline_ms_ = now_ms + console_timeout_ms_; return true; }
    settling_ = false;
    return false;
  }


  std::vector<std::uint8_t> take_pending() {
    std::vector<std::uint8_t> out;
    out.swap(pending_);
    stats_.bytes_tx += static_cast<std::uint32_t>(out.size());
    last_tx_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(out.size() < kDiagnosticBytes ? out.size() : kDiagnosticBytes));
    if (capture_) capture_(true, out.data(), out.size());
    return out;
  }

  void send(std::string_view text, std::uint64_t now_ms, std::uint64_t timeout_ms) {
    pending_.assign(text.begin(), text.end());
    console_timeout_ms_ = timeout_ms;
    deadline_ms_ = now_ms + timeout_ms;
  }
  void send_query(const char* text, std::uint64_t now_ms) {
    tx_plain_ = dialect_ == Dialect::kPi00;
    if (dialect_ == Dialect::kPi18) voltronic::pi18::build_command(text, pending_);
    else if (dialect_ == Dialect::kPi17) voltronic::pi17::build_command(text, pending_);
    else if (tx_plain_) voltronic::build_plain_command(text, pending_);
    else voltronic::build_command(text, pending_);
    deadline_ms_ = now_ms + kVoltronicTimeoutMs;
  }
  const char* dialect_name() const { return dialect_ == Dialect::kPi18 ? "pi18" : dialect_ == Dialect::kRevo ? "revo" : dialect_ == Dialect::kPi41 ? "pi41" : dialect_ == Dialect::kPi34 ? "pi34" : dialect_ == Dialect::kPi00 ? "pi00" : dialect_ == Dialect::kPi17 ? "pi17" : "pi30"; }

  void begin_cycle(std::uint64_t now_ms) {
    cycle_started_ms_ = now_ms;
    buffer_.clear();
    framer_ = voltronic::ReplyFramer();
    framer18_ = voltronic::pi18::ReplyFramer();
    if (driver_) {
      step_ = Step::kDriver;
      pending_ = driver_->begin();
      deadline_ms_ = now_ms + kDriverTimeoutMs;
    } else if (kind_ == config::Kind::kAnt) {
      step_ = Step::kAnt;
      ant_framer_.clear();
      pending_ = ant::request(ant_protocol_);
      deadline_ms_ = now_ms + kAntTimeoutMs;
    } else if (kind_ == config::Kind::kVoltronic) {
      mode_ = 0;
      warnings_.clear();
      has_pv2_ = false;
      warnings_read_ = false;
      units_.clear();
      if (dialect_ == Dialect::kPi18 || dialect_ == Dialect::kPi17) { step_ = Step::kGs; send_query("GS", now_ms); }
      else { step_ = Step::kQpigs; send_query("QPIGS", now_ms); }
    } else {
      step_ = Step::kPwr;
      system_ = {};
      modules_.clear();
      module_at_ = 0;
      caught_up_ = false;
      catching_up_ = false;
      // A console that has been quiet for a while may swallow its first command: at the start and after a silence a bare Enter goes first, to wake it up.
      send(wake_ ? "\rpwr\r" : "pwr\r", now_ms, kConsoleTableTimeoutMs);
    }
  }

  void abandon(std::uint64_t now_ms) {
    step_ = Step::kIdle;
    pending_.clear();
    buffer_.clear();
    next_cycle_ms_ = now_ms + poll_ms_;
  }

  void finish(std::uint64_t now_ms) {
    step_ = Step::kIdle;
    ready_ = true;
    if (kind_ == config::Kind::kPylontech) for (pylontech::Module& m : modules_) { pylontech::estimate_remaining(m); pylontech::deduce_maker(m); }
    else if (const char* maker = protocol_maker(kind_)) for (pylontech::Module& m : modules_) { m.manufacturer = maker; m.manufacturer_deduced = true; }
    next_cycle_ms_ = cycle_started_ms_ + poll_ms_;
    if (next_cycle_ms_ < now_ms) next_cycle_ms_ = now_ms;
    buffer_.clear();
  }

  void fail(const char* error, std::uint64_t now_ms) {
    ++stats_.replies_bad;
    stats_.last_error = error;
    abandon(now_ms);
  }

  // ---- Voltronic ----
  // Both dialects' framers listen: while the dialect is not known, a whole reply of the other one is what tells it.
  void rx_voltronic(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    for (std::size_t i = 0; i < length && step_ != Step::kIdle; ++i) {
      std::vector<std::uint8_t> frame;
      framer_.set_revo(dialect_ == Dialect::kRevo);
      if (framer_.feed(data[i], frame)) on_pi30_frame(frame, now_ms);
      else if (framer18_.feed(data[i], frame)) on_pi18_frame(frame, now_ms);
    }
  }

  // The dialect answered: it is the one from now on (an inverter that only knows it can be met again after three misses).
  void learn(Dialect dialect) {
    if (dialect == Dialect::kPi30) { crc_reply_seen_ = false; pi00_crc_replies_ = false; }
    if (!auto_) return;
    dialect_ = dialect;
    known_ = true;
    misses_ = 0;
  }
  // The order "auto" asks in: with CRC, without it, PI18, PI17.
  static Dialect next_dialect(Dialect dialect) {
    return dialect == Dialect::kPi30 ? Dialect::kPi00 : dialect == Dialect::kPi00 ? Dialect::kPi18 : dialect == Dialect::kPi18 ? Dialect::kPi17 : Dialect::kPi30;
  }
  // An answer that says nothing certain while the dialect is looked for: the cycle is given up and `dialect` is asked soon, without being taken as the one.
  void try_next(Dialect dialect, const char* why, std::uint64_t now_ms) {
    dialect_ = dialect;
    fail(why, now_ms);
    next_cycle_ms_ = now_ms + 100;
  }
  // A reply to a question without CRC. False when there is nothing more to do with it; otherwise `ok` and `text` are what the rest reads.
  bool take_pi00(const std::vector<std::uint8_t>& frame, bool looking, bool& ok, std::string& text, std::uint64_t now_ms) {
    if (ok) {
      // It came with a CRC. A refusal while looking, or the first such answer, may be a plain PI30 unit that missed the first question: PI30 is asked once more.
      if (looking && (text == "NAK" || !crc_reply_seen_)) { crc_reply_seen_ = true; try_next(Dialect::kPi30, "format", now_ms); return false; }
      if (text != "NAK") pi00_crc_replies_ = true;   // from here on a reply of this unit that lost its CRC is a damaged one
      return true;
    }
    ok = !pi00_crc_replies_ && voltronic::parse_reply_plain(frame.data(), frame.size(), text);
    if (looking && ok && text == "NAK") { try_next(Dialect::kPi18, "nak", now_ms); return false; }
    return true;
  }
  // A whole reply of the other dialect arrived while looking for it: start over in that one, soon.
  void switch_to(Dialect dialect, std::uint64_t now_ms) {
    learn(dialect);
    abandon(now_ms);
    next_cycle_ms_ = now_ms + 100;
  }

  void on_pi30_frame(const std::vector<std::uint8_t>& frame, std::uint64_t now_ms) {
    std::string text;
    const bool looking = auto_ && !known_;
    bool crc_ok = voltronic::parse_reply(frame.data(), frame.size(), text);   // for pi00: whether the reply is usable
    if (dialect_ == Dialect::kPi18 || dialect_ == Dialect::kPi17) {
      if (looking && crc_ok) switch_to(Dialect::kPi30, now_ms);
      return;
    }
    if (dialect_ == Dialect::kPi00 && !take_pi00(frame, looking, crc_ok, text, now_ms)) return;
    if (step_ == Step::kQpigs2 || step_ == Step::kQpgs) { on_optional(crc_ok, text, now_ms); return; }
    if (is_identity_step()) { on_identity(crc_ok, text, now_ms); return; }
    if (step_ == Step::kExtended) { on_extended(crc_ok, text, now_ms); return; }
    if (!crc_ok) {
      // only the REVO dialect ends a reply with the checksum: a reply that fits it (and no CRC) makes the port REVO
      std::string revo_text;
      if (dialect_ != Dialect::kPi00 && (dialect_ == Dialect::kRevo || looking) && voltronic::parse_reply_revo(frame.data(), frame.size(), revo_text)) {
        // While the dialect is being looked for, a reply with no CRC that only fits the one-byte checksum counts as REVO if it also reads as the reading of a REVO inverter: one corrupted standard
        // reply in 256 fits the checksum by chance, and must not make the port a REVO one.
        decltype(status_) probe{};
        if (dialect_ != Dialect::kRevo && step_ == Step::kQpigs && !voltronic::parse_qpigs_revo(revo_text, probe)) { fail("crc", now_ms); return; }
        text = revo_text;
        learn(Dialect::kRevo);
      } else {
        // an answer that reads as QPIGS but has no CRC: the unit may be a PI00 one, so it is asked without CRC next (nothing is taken from this one)
        std::string plain;
        voltronic::Status probe;
        if (looking && step_ == Step::kQpigs && voltronic::parse_reply_plain(frame.data(), frame.size(), plain) &&
            (voltronic::parse_qpigs(plain, probe) || voltronic::parse_qpigs_pi00(plain, probe))) try_next(Dialect::kPi00, "crc", now_ms);
        else fail("crc", now_ms);
        return;
      }
    }
    if (text == "NAK") {
      learn(dialect_);
      if (step_ == Step::kQmod) { mode_missed("nak", now_ms); return; }
      if (step_ == Step::kQpiws) { warnings_missed("nak", now_ms); return; }
      fail("nak", now_ms);
      return;
    }
    const bool revo = dialect_ == Dialect::kRevo;
    if (step_ == Step::kQpigs) {
      pi00_layout_ = dialect_ == Dialect::kPi00 && voltronic::parse_qpigs_pi00(text, status_, &pi16_);
      const bool read = pi00_layout_ ? true
                        : revo ? voltronic::parse_qpigs_revo(text, status_)
                        : dialect_ == Dialect::kPi41 ? voltronic::parse_qpigs_pi41(text, status_)
                        : dialect_ == Dialect::kPi34 ? voltronic::parse_qpigs_pi34(text, status_)
                                                     : voltronic::parse_qpigs(text, status_);
      if (!read) {
        if (looking && dialect_ == Dialect::kPi00) try_next(Dialect::kPi18, "format", now_ms);
        else fail("format", now_ms);
        return;
      }
      learn(dialect_);
      ++stats_.replies_ok;
      if (dialect_ == Dialect::kPi34) { stats_.last_error.clear(); finish(now_ms); return; }   // a charge controller is not asked for QMOD
      step_ = Step::kQmod;
      send_query("QMOD", now_ms);
    } else if (step_ == Step::kQmod) {
      if (!(dialect_ == Dialect::kPi00 ? voltronic::parse_qmod_pi00(text, mode_) : voltronic::parse_qmod(text, mode_))) { mode_missed("format", now_ms); return; }
      mode_ever_ = true;
      ++stats_.replies_ok;
      after_mode(now_ms);
    } else if (step_ == Step::kQpiws) {
      std::vector<std::string> flags;
      if (!(pi00_layout_ ? voltronic::parse_qpiws_pi00(text, flags, pi16_) : voltronic::parse_qpiws(text, flags))) { warnings_missed("format", now_ms); return; }
      // the names are those of the PI30 documents: another family's flags mean other things and are not named
      if (dialect_ != Dialect::kPi41) { warnings_ = flags; warnings_read_ = true; }
      warnings_ever_ = true;
      ++stats_.replies_ok;
      stats_.last_error.clear();
      after_main_readings(now_ms);
    }
  }

  // QMOD and QPIWS: an inverter that has answered one before and misses it once loses the cycle (a missing warning list must not look like "no warnings"). One that has never
  // answered it, cycle after cycle, would never be read at all: from the kGiveUpTries-th time the reading goes out without it and says so (mode "unknown", warnings_known false).
  void mode_missed(const char* why, std::uint64_t now_ms) {
    if (mode_ever_ || ++mode_misses_ < kGiveUpTries) { fail(why, now_ms); return; }
    ++stats_.replies_bad;
    stats_.last_error = why;
    after_mode(now_ms);
  }
  void after_mode(std::uint64_t now_ms) {
    if (dialect_ == Dialect::kRevo) { stats_.last_error.clear(); finish(now_ms); return; }   // the REVO dialect has no warning list to ask for
    step_ = Step::kQpiws;
    send_query("QPIWS", now_ms);
  }
  void warnings_missed(const char* why, std::uint64_t now_ms) {
    if (warnings_ever_ || ++warnings_misses_ < kGiveUpTries) { fail(why, now_ms); return; }
    ++stats_.replies_bad;
    stats_.last_error = why;
    warnings_.clear();
    after_main_readings(now_ms);
  }

  // ---- the drivers of the other protocols ----
  void rx_driver(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    std::vector<std::uint8_t> next;
    for (std::size_t i = 0; i < length && step_ == Step::kDriver; ++i) {
      const drivers::Rx result = driver_->feed(data[i], next);
      if (result == drivers::Rx::kNext) { ++stats_.replies_ok; pending_ = next; deadline_ms_ = now_ms + kDriverTimeoutMs; }
      else if (result == drivers::Rx::kDone) { ++stats_.replies_ok; driver_done(now_ms); }
      else if (result == drivers::Rx::kBad) fail(driver_->error(), now_ms);
    }
  }
  void driver_done(std::uint64_t now_ms) {
    const drivers::Out& out = driver_->out();
    if (driver_->inverter()) { status_ = out.status; mode_ = out.mode; warnings_ = out.warnings; has_pv2_ = false; }
    else modules_ = out.modules;
    stats_.last_error.clear();
    finish(now_ms);
  }

  // ---- the optional readings (standard dialect only) ----
  bool want_pv2() const { return pv2_wanted_ && !pv2_absent_ && dialect_ == Dialect::kPi30 && known_; }
  // The three readings that make the message are in: the second PV input and the parallel units are asked if they are wanted, and the reading goes out either way.
  void after_main_readings(std::uint64_t now_ms) {
    if (want_pv2()) { step_ = Step::kQpigs2; send_query("QPIGS2", now_ms); return; }
    begin_units(now_ms);
  }
  void begin_units(std::uint64_t now_ms) {
    unit_at_ = 0;
    if (parallel_ > 0 && dialect_ == Dialect::kPi30 && known_) { ask_unit(now_ms); return; }
    next_identity(now_ms);
  }
  void ask_unit(std::uint64_t now_ms) {
    if (unit_at_ >= parallel_) { next_identity(now_ms); return; }
    step_ = Step::kQpgs;
    send_query(("QPGS" + std::to_string(unit_at_)).c_str(), now_ms);
  }
  // What the unit says about itself, asked after the readings and never at the cost of them (standard dialect only): its serial number and firmware once, its ratings and settings every
  // ten minutes (QPIRI), and the energy the panels have made in all every five (QET). A question that gets no good answer twice in a row is dropped, and the reading goes out either way.
  bool is_identity_step() const { return step_ == Step::kQid || step_ == Step::kQvfw || step_ == Step::kQpiri || step_ == Step::kQet; }
  void next_identity(std::uint64_t now_ms) {
    if ((dialect_ != Dialect::kPi30 && dialect_ != Dialect::kPi00) || !known_) { finish(now_ms); return; }
    if (!identity_wanted_) { next_extended(now_ms); return; }
    if (serial_.empty() && qid_tries_ < kIdentityTries) { step_ = Step::kQid; send_query("QID", now_ms); return; }
    if (firmware_.empty() && qvfw_tries_ < kIdentityTries) { step_ = Step::kQvfw; send_query("QVFW", now_ms); return; }
    if (qpiri_tries_ < kIdentityTries && (!have_ratings_ || now_ms - ratings_at_ms_ >= kRatingsEveryMs)) { step_ = Step::kQpiri; send_query("QPIRI", now_ms); return; }
    if (qet_tries_ < kIdentityTries && (!have_energy_ || now_ms - energy_at_ms_ >= kEnergyEveryMs)) { step_ = Step::kQet; send_query("QET", now_ms); return; }
    next_extended(now_ms);
  }
  // One extra question per cycle, the first one that is due: the cycle grows by one exchange at most. A question that gets no good answer twice in a row is not asked again.
  void next_extended(std::uint64_t now_ms) {
    if (!extended_wanted_ || dialect_ != Dialect::kPi30) { finish(now_ms); return; }
    static constexpr const char* kNames[] = {"QFLAG", "QDI", "QMCHGCR", "QMUCHGCR", "QBMS", "QT", "QED", "QEM", "QEY", "QLD", "QLM", "QLY", "QLT"};
    static constexpr std::size_t kDatePart[] = {0, 0, 0, 0, 0, 0, 8, 6, 4, 8, 6, 4, 0};   // how much of the date goes after the command
    for (std::size_t i = 0; i < static_cast<std::size_t>(More::kCount); ++i) {
      Asked& a = asked_[i];
      const std::uint64_t every = i <= static_cast<std::size_t>(More::kQmuchgcr) ? kExtendedRarelyMs : kExtendedEveryMs;
      if (a.misses >= kExtendedTries || (a.at_ms != 0 && now_ms - a.at_ms < every)) continue;
      if (kDatePart[i] != 0 && more_.day.empty()) continue;
      more_at_ = i;
      step_ = Step::kExtended;
      send_query((kNames[i] + more_.day.substr(0, kDatePart[i])).c_str(), now_ms);
      return;
    }
    finish(now_ms);
  }
  void extended_missed(std::uint64_t now_ms) {
    Asked& a = asked_[more_at_];
    ++a.misses;
    a.at_ms = a.misses >= kExtendedTries ? now_ms : 0;   // the second try comes in the next cycle
  }
  void on_extended(bool crc_ok, const std::string& text, std::uint64_t now_ms) {
    bool good = crc_ok && text != "NAK";
    if (good) {
      const More which = static_cast<More>(more_at_);
      if (which == More::kQflag) { good = voltronic::parse_qflag(text, more_.on, more_.off); if (good) more_.has_flags = true; }
      else if (which == More::kQdi) { good = voltronic::parse_qdi(text, more_.defaults); if (good) more_.has_defaults = true; }
      else if (which == More::kQmchgcr) good = voltronic::parse_current_list(text, more_.charge_options);
      else if (which == More::kQmuchgcr) good = voltronic::parse_current_list(text, more_.ac_charge_options);
      else if (which == More::kQbms) { good = voltronic::parse_qbms(text, more_.bms); if (good) more_.has_bms = true; }
      else if (which == More::kQt) {
        std::string day;
        good = voltronic::parse_qt(text, day);
        if (good && day != more_.day) {   // another day: what was counted for the one before is not today's, and the counters are asked again
          for (std::size_t i = 0; i < 6; ++i) more_.energy_wh[i] = -1;
          for (std::size_t i = static_cast<std::size_t>(More::kQed); i <= static_cast<std::size_t>(More::kQly); ++i) asked_[i].at_ms = 0;
          more_.day = day;
        }
      } else good = voltronic::parse_qet(text, more_.energy_wh[more_at_ - static_cast<std::size_t>(More::kQed)]);
    }
    if (good) { ++stats_.replies_ok; asked_[more_at_].misses = 0; asked_[more_at_].at_ms = now_ms == 0 ? 1 : now_ms; }
    else extended_missed(now_ms);
    finish(now_ms);
  }
  void identity_missed() {
    if (step_ == Step::kQid) ++qid_tries_;
    else if (step_ == Step::kQvfw) ++qvfw_tries_;
    else if (step_ == Step::kQpiri) ++qpiri_tries_;
    else if (step_ == Step::kQet) ++qet_tries_;
  }
  void on_identity(bool crc_ok, const std::string& text, std::uint64_t now_ms) {
    bool good = crc_ok && text != "NAK";
    if (good) {
      if (step_ == Step::kQid) { good = voltronic::parse_qid(text, serial_); }
      else if (step_ == Step::kQvfw) { good = voltronic::parse_qvfw(text, firmware_); }
      else if (step_ == Step::kQpiri) { good = voltronic::parse_qpiri(text, ratings_); if (good) { have_ratings_ = true; ratings_at_ms_ = now_ms; qpiri_tries_ = 0; } }
      else if (step_ == Step::kQet) { good = voltronic::parse_qet(text, energy_wh_); if (good) { have_energy_ = true; energy_at_ms_ = now_ms; qet_tries_ = 0; } }
    }
    if (good) ++stats_.replies_ok;
    else identity_missed();
    // a refresh that failed is tried again at the next due time, not at every cycle
    if (!good && step_ == Step::kQpiri && have_ratings_) ratings_at_ms_ = now_ms;
    if (!good && step_ == Step::kQet && have_energy_) energy_at_ms_ = now_ms;
    next_identity(now_ms);
  }
  void miss_pv2(std::uint64_t now_ms) {
    has_pv2_ = false;
    if (++pv2_misses_ >= 2) pv2_absent_ = true;
    begin_units(now_ms);
  }
  // An answer to QPIGS2 or QPGS<n>: anything but a good one skips that step, and never spoils the reading.
  void on_optional(bool crc_ok, const std::string& text, std::uint64_t now_ms) {
    if (step_ == Step::kQpigs2) {
      voltronic::Pv2 v;
      if (crc_ok && text != "NAK" && voltronic::parse_qpigs2(text, v)) { pv2_ = v; has_pv2_ = true; pv2_misses_ = 0; ++stats_.replies_ok; begin_units(now_ms); }
      else miss_pv2(now_ms);
      return;
    }
    voltronic::ParallelUnit u;
    if (crc_ok && text != "NAK" && voltronic::parse_qpgs(text, u)) { u.number = unit_at_; units_.push_back(u); ++stats_.replies_ok; }
    ++unit_at_;
    ask_unit(now_ms);
  }

  void on_pi18_frame(const std::vector<std::uint8_t>& frame, std::uint64_t now_ms) {
    voltronic::pi18::Kind kind = voltronic::pi18::Kind::kData;
    std::string payload;
    const bool ok = voltronic::pi18::parse_reply(frame.data(), frame.size(), kind, payload);
    if (dialect_ != Dialect::kPi18 && dialect_ != Dialect::kPi17) {
      if (auto_ && !known_ && ok) switch_to(Dialect::kPi18, now_ms);
      return;
    }
    if (!ok) { fail("crc", now_ms); return; }
    if (kind == voltronic::pi18::Kind::kNak) { learn(dialect_); fail("nak", now_ms); return; }
    if (kind != voltronic::pi18::Kind::kData) { fail("format", now_ms); return; }
    if (dialect_ == Dialect::kPi17) { on_pi17_data(payload, now_ms); return; }
    if (step_ == Step::kGs) {
      if (!voltronic::pi18::parse_gs(payload, status_)) { fail("format", now_ms); return; }
      learn(Dialect::kPi18);
      ++stats_.replies_ok;
      step_ = Step::kMod;
      send_query("MOD", now_ms);
    } else if (step_ == Step::kMod) {
      if (!voltronic::pi18::parse_mod(payload, mode_)) { fail("format", now_ms); return; }
      ++stats_.replies_ok;
      step_ = Step::kFws;
      send_query("FWS", now_ms);
    } else if (step_ == Step::kFws) {
      if (!voltronic::pi18::parse_fws(payload, warnings_)) { fail("format", now_ms); return; }
      warnings_read_ = true;
      ++stats_.replies_ok;
      stats_.last_error.clear();
      finish(now_ms);
    }
  }

  // PI17: GS, then PS (the powers), MOD and WS. The reading is whole after WS.
  void on_pi17_data(const std::string& payload, std::uint64_t now_ms) {
    const auto next = [this, now_ms](Step step, const char* command) {
      ++stats_.replies_ok;
      step_ = step;
      send_query(command, now_ms);
    };
    if (step_ == Step::kGs) {
      status_ = voltronic::Status();
      if (!voltronic::pi17::parse_gs(payload, status_)) { fail("format", now_ms); return; }
      learn(Dialect::kPi17);
      next(Step::kPs, "PS");
    } else if (step_ == Step::kPs) {
      if (!voltronic::pi17::parse_ps(payload, status_)) { fail("format", now_ms); return; }
      next(Step::kMod, "MOD");
    } else if (step_ == Step::kMod) {
      if (!voltronic::pi17::parse_mod(payload, mode_)) { fail("format", now_ms); return; }
      next(Step::kFws, "WS");
    } else if (step_ == Step::kFws) {
      if (!voltronic::pi17::parse_ws(payload, warnings_)) { fail("format", now_ms); return; }
      warnings_read_ = true;
      ++stats_.replies_ok;
      stats_.last_error.clear();
      finish(now_ms);
    }
  }

  // ---- Pylontech console ----
  // The console ends every answer, a good one or a refusal ("Invalid command or fail to excute."), with a line of `$$` and its prompt (`pylon>` or `pylon_debug>`):
  // the answer is whole when the text ends in that prompt. Waiting for it also leaves nothing of this answer to be taken for the next one, and a command the battery does
  // not know is over at once instead of after the timeout.
  static bool console_done(const std::string& text) { return pylontech::console_done(text); }

  void ask_next_module(std::uint64_t now_ms) {
    // the next module that is there, in order, up to the limit
    while (module_at_ < modules_.size()) {
      const pylontech::Module& m = modules_[module_at_];
      if (m.present && m.number >= 1 && m.number <= max_modules_) break;
      ++module_at_;
    }
    if (module_at_ >= modules_.size()) { ask_system(now_ms); return; }
    const int number = modules_[module_at_].number;
    if (!(cells_wanted_ & (1u << number))) {
      // not this module's turn: it keeps the cells of its last one
      const CellsCache& cache = cells_cache_[static_cast<std::size_t>(number)];
      pylontech::attach_cells(modules_, number, cache.cells);
      modules_[module_at_].cell_flags = cache.flags;
      modules_[module_at_].cells_unread = cache.unread;
      if (cache.remaining_ah >= 0) modules_[module_at_].capacity_ah = cache.remaining_ah;
      modules_[module_at_].balancing = cache.balancing;
      after_bat(now_ms);
      return;
    }
    step_ = Step::kBat;
    buffer_.clear();
    send("bat " + std::to_string(number) + "\r", now_ms, kConsoleExtraTimeoutMs);
  }

  // `pwrsys`, last: the reading goes out with or without it. A battery that refuses it or does not answer twice in a row is left alone for half an hour.
  void ask_system(std::uint64_t now_ms) {
    if (!extended_wanted_ || (system_misses_ >= kExtendedTries && now_ms - system_tried_ms_ < kExtendedRarelyMs)) { finish(now_ms); return; }
    step_ = Step::kSystem;
    system_tried_ms_ = now_ms;
    buffer_.clear();
    send("pwrsys\r", now_ms, kConsoleExtraTimeoutMs);
  }
  void system_missed() { if (system_misses_ < kExtendedTries) ++system_misses_; }

  // Which modules' cells are asked in this cycle: all of them, or the next `cells_per_cycle_` present ones in rotation (and any that has no cells yet).
  void choose_cells_to_read() {
    std::vector<int> numbers;
    for (const pylontech::Module& m : modules_) if (m.present && m.number >= 1 && m.number <= max_modules_ && m.number <= 16) numbers.push_back(m.number);
    std::sort(numbers.begin(), numbers.end());
    cells_wanted_ = 0;
    if (numbers.empty()) return;
    if (cells_per_cycle_ <= 0 || static_cast<std::size_t>(cells_per_cycle_) >= numbers.size()) {
      for (int n : numbers) cells_wanted_ |= 1u << n;
      return;
    }
    std::size_t start = 0;
    while (start < numbers.size() && numbers[start] < bat_cursor_) ++start;
    if (start >= numbers.size()) start = 0;
    int last = numbers[start];
    for (int k = 0; k < cells_per_cycle_; ++k) {
      last = numbers[(start + static_cast<std::size_t>(k)) % numbers.size()];
      cells_wanted_ |= 1u << last;
    }
    bat_cursor_ = last + 1;
    for (int n : numbers) if (cells_cache_[static_cast<std::size_t>(n)].cells.empty()) cells_wanted_ |= 1u << n;
  }

  void rx_pylontech(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    if (step_ == Step::kIdle || !pending_.empty()) return;   // the next question has not gone out yet: what arrives is not its answer
    for (std::size_t i = 0; i < length; ++i) if (buffer_.size() < 8192) buffer_ += static_cast<char>(data[i]);
    if (!console_done(buffer_)) return;
    if (step_ == Step::kPwr) {
      wake_ = false;
      std::vector<pylontech::Module> found;
      pylontech::parse_pwr(buffer_, found);
      // The battery is not read as a stack of one: the first battery of its stack counts it already, and its figures would be added twice.
      slave_sheet_ = found.empty() ? pylontech::read_slave_sheet(buffer_) : pylontech::SlaveSheet{};
      slave_module_ = slave_sheet_.place;
      if (found.empty()) { fail("format", now_ms); return; }
      ++stats_.replies_ok;
      stats_.last_error.clear();
      modules_ = found;
      // A module that is not there any more takes what it told with it: the one that comes in its place is another battery, with its own serial number.
      for (const pylontech::Module& m : modules_) {
        if (m.present || m.number < 1 || m.number > 16) continue;
        extras_[static_cast<std::size_t>(m.number)] = Extras{};
        cells_cache_[static_cast<std::size_t>(m.number)] = CellsCache{};   // nor are the cells of the one before shown as those of the one that comes back
      }
      module_at_ = 0;
      sheet_asked_ = false;
      choose_cells_to_read();
      ask_next_module(now_ms);
    } else if (step_ == Step::kBat) {
      std::vector<double> cells;
      double remaining_ah = -1;
      int balancing = 0;
      const bool read = pylontech::parse_bat(buffer_, cells, &remaining_ah, &balancing) > 0;
      std::vector<pylontech::CellFlag> flags;
      pylontech::parse_cell_flags(buffer_, flags);
      // A cell that reads 0 V for the first time may be bytes lost on the line: that table waits. Read as 0 V again, it is the battery that does not read the cell, and it is told.
      const std::uint32_t zeros = read ? pylontech::unread_mask(cells) : 0;
      CellsCache& seen = cells_cache_[static_cast<std::size_t>(modules_[module_at_].number)];
      const bool unconfirmed = (zeros & ~seen.zeros) != 0;
      if (read) seen.zeros = zeros;
      if (read && (unconfirmed || !pylontech::cells_match_row(modules_[module_at_], cells))) {
        // a table that does not agree with the module's own row is not published: the module keeps the cells of its last good turn
        stats_.last_error = unconfirmed ? "bat table with a cell at 0 V, to be read again" : "bat table does not match pwr";
        const CellsCache& kept = cells_cache_[static_cast<std::size_t>(modules_[module_at_].number)];
        pylontech::attach_cells(modules_, modules_[module_at_].number, kept.cells);
        modules_[module_at_].cell_flags = kept.flags;
        modules_[module_at_].cells_unread = kept.unread;
        if (kept.remaining_ah >= 0) modules_[module_at_].capacity_ah = kept.remaining_ah;
        modules_[module_at_].balancing = kept.balancing;
      } else if (read) {
        pylontech::attach_cells(modules_, modules_[module_at_].number, cells);
        if (remaining_ah >= 0) modules_[module_at_].capacity_ah = remaining_ah;
        modules_[module_at_].balancing = balancing;
        CellsCache& cache = cells_cache_[static_cast<std::size_t>(modules_[module_at_].number)];
        std::vector<int> unread;
        for (int i = 0; i < 32; ++i) if (zeros & (1u << i)) unread.push_back(i);
        modules_[module_at_].cell_flags = cache.flags = flags;
        modules_[module_at_].cells_unread = cache.unread = unread;
        cache.cells = cells;
        cache.remaining_ah = remaining_ah;
        cache.balancing = balancing;
        ++stats_.replies_ok;
      }
      after_bat(now_ms);
    } else if (step_ == Step::kInfo) {
      if (pylontech::parse_info(buffer_, modules_[module_at_]) > 0) ++stats_.replies_ok;
      pylontech::read_plate(buffer_, modules_[module_at_].plate);
      ask_stat(now_ms);
    } else if (step_ == Step::kStat) {
      if (pylontech::parse_info(buffer_, modules_[module_at_]) > 0) ++stats_.replies_ok;
      ask_event_or_end(now_ms);
    } else if (step_ == Step::kEvent) {
      pylontech::Event event;
      if (pylontech::parse_event(buffer_, event)) { modules_[module_at_].last_event = event; ++stats_.replies_ok; }
      ask_soh(now_ms);
    } else if (step_ == Step::kSoh) {
      if (pylontech::parse_soh(buffer_, modules_[module_at_]) > 0) ++stats_.replies_ok;
      end_extras(now_ms);
    } else if (step_ == Step::kSystem) {
      if (pylontech::parse_pwrsys(buffer_, system_)) { ++stats_.replies_ok; system_misses_ = 0; }
      else system_missed();
      finish(now_ms);
    } else if (step_ == Step::kSheet) {
      modules_[module_at_].fault_code = pylontech::parse_system_fault(buffer_, modules_[module_at_].number);
      if (!modules_[module_at_].fault_code.empty()) ++stats_.replies_ok;
      after_bat(now_ms);
    }
  }

  // The cells are in. The identity (`info`) and the history (`stat`) of a module hardly change: they are asked when they are not known or half an hour old, and kept. A module
  // whose sheet is still missing (a question not answered, or refused) does not wait half an hour: one such module per cycle is asked again, a few times in a row and then every
  // five minutes, so a whole stack gets its sheets in a few minutes and no cycle grows by more than one module's questions.
  void after_bat(std::uint64_t now_ms) {
    pylontech::Module& m = modules_[module_at_];
    // A module whose electronics report a fault is asked for its own sheet every turn: the code of the fault is there, and only there.
    if (!sheet_asked_ && faulty(m)) {
      sheet_asked_ = true;
      step_ = Step::kSheet;
      buffer_.clear();
      send("pwr " + std::to_string(m.number) + "\r", now_ms, kConsoleExtraTimeoutMs);
      return;
    }
    sheet_asked_ = false;
    const Extras& e = extras_[static_cast<std::size_t>(m.number)];
    catching_up_ = false;
    if (e.valid && now_ms - e.at_ms < kExtrasRefreshMs) {
      fill_from_kept(m, e);
      if (caught_up_ || !incomplete(e) || (e.misses >= kExtrasQuickTries && now_ms - e.tried_ms < kExtrasRetryMs)) {
        ++module_at_;
        ask_next_module(now_ms);
        return;
      }
      caught_up_ = true;
      catching_up_ = true;
    }
    had_capacity_ = m.capacity_ah >= 0;
    // a module that has told nothing at all is given longer once: its answer may come through the first battery later than the usual time
    slow_sheet_ = catching_up_ && e.misses == 1 && e.model.empty() && e.full_capacity_ah < 0 && e.counters.empty();
    step_ = Step::kInfo;
    buffer_.clear();
    send("info " + std::to_string(m.number) + "\r", now_ms, slow_sheet_ ? kConsoleSlowTimeoutMs : kConsoleExtraTimeoutMs);
  }
  static bool faulty(const pylontech::Module& m) { return m.base_state == "SysError" || !m.cells_unread.empty() || pylontech::state_word_matters(m.system_alarm_state); }
  void ask_stat(std::uint64_t now_ms) {
    step_ = Step::kStat;
    buffer_.clear();
    send("stat " + std::to_string(modules_[module_at_].number) + "\r", now_ms, slow_sheet_ ? kConsoleSlowTimeoutMs : kConsoleExtraTimeoutMs);
  }
  // The newest entry of the battery's event log is asked when `stat` says there are entries and the one kept is not the newest (the log's last item is taken to be the count of
  // `Data Items`; a battery that answers something else just leaves the event out).
  void ask_event_or_end(std::uint64_t now_ms) {
    const pylontech::Module& m = modules_[module_at_];
    if (catching_up_) {   // only what is missing: the event log waits for the half-hourly round
      if (m.soh_counts.empty()) ask_soh(now_ms); else end_extras(now_ms);
      return;
    }
    long long items = 0;
    for (const auto& counter : m.counters) if (counter.first == "data_items") items = counter.second;
    const Extras& e = extras_[static_cast<std::size_t>(m.number)];
    if (items < 1 || items > 100000 || (e.last_event.valid && e.last_event.index == items)) { ask_soh(now_ms); return; }
    step_ = Step::kEvent;
    buffer_.clear();
    send("data event " + std::to_string(items) + "\r", now_ms, kConsoleExtraTimeoutMs);
  }
  // The health of each cell (`soh`), the last of the details asked once in a while.
  void ask_soh(std::uint64_t now_ms) {
    step_ = Step::kSoh;
    buffer_.clear();
    send("soh " + std::to_string(modules_[module_at_].number) + "\r", now_ms, kConsoleExtraTimeoutMs);
  }
  // The extras have been asked (answered or not): keep what came, unless the module also reports its remaining charge there (that one has to be fresh every time).
  void end_extras(std::uint64_t now_ms) {
    pylontech::Module& m = modules_[module_at_];
    Extras& e = extras_[static_cast<std::size_t>(m.number)];
    fill_from_kept(m, e);   // a question that got no answer this time does not take away what the module told before
    e.manufacturer = m.manufacturer; e.model = m.model; e.full_capacity_ah = m.full_capacity_ah; e.cycles = m.cycles; e.health_percent = m.health_percent; e.counters = m.counters; e.plate = m.plate; if (m.last_event.valid) e.last_event = m.last_event; e.cell_count = m.cell_count; e.board_version = m.board_version; e.main_soft_version = m.main_soft_version; e.soft_version = m.soft_version; e.max_charge_a = m.max_charge_a; e.max_discharge_a = m.max_discharge_a; e.console_baud = m.console_baud; e.soh_counts = m.soh_counts; e.soh_abnormal = m.soh_abnormal;
    if (!catching_up_) e.at_ms = now_ms;
    e.tried_ms = now_ms;
    e.misses = !incomplete(e) ? 0 : e.misses < kExtrasQuickTries ? e.misses + 1 : e.misses;
    e.valid = had_capacity_ || m.capacity_ah < 0;
    catching_up_ = false;
    ++module_at_;
    ask_next_module(now_ms);
  }

  // ---- ANT-BMS ----
  void rx_ant(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    for (std::size_t i = 0; i < length && step_ == Step::kAnt; ++i) {
      std::vector<std::uint8_t> frame;
      ant::Protocol protocol = ant::Protocol::kOld;
      if (!ant_framer_.feed(data[i], frame, protocol)) continue;
      ant::Reading reading;
      const bool ok = protocol == ant::Protocol::kOld ? ant::decode_old(frame.data(), frame.size(), reading) : ant::decode_new(frame.data(), frame.size(), reading);
      if (!ok) { ant_misses_hint(now_ms); fail("crc", now_ms); if (!ant_known_) next_cycle_ms_ = now_ms + 300; return; }
      ++stats_.replies_ok;
      stats_.last_error.clear();
      ant_protocol_ = protocol;
      ant_known_ = true;
      ant_misses_ = 0;
      ant_seen_ = true;
      ant_reading_ = reading;
      modules_.assign(1, reading.module);
      finish(now_ms);
    }
  }
  // A frame that is not whole or does not check, or silence: while the protocol is not known, the other one is tried next, soon.
  void ant_misses_hint(std::uint64_t) {
    if (!ant_known_) ant_protocol_ = ant_protocol_ == ant::Protocol::kOld ? ant::Protocol::kNew : ant::Protocol::kOld;
  }

  // A request nobody answered in time. The extras of a battery (`bat`, `info`) are not worth losing the reading for: it goes on without them. Anything
  // else gives the cycle up, and the next one starts at the next poll.
  void on_timeout(std::uint64_t now_ms) {
    ++stats_.timeouts;
    if (kind_ == config::Kind::kPylontech) { settling_ = true; settle_started_ms_ = now_ms; }
    if (step_ == Step::kBat) {
      // a module that did not answer for its cells keeps the ones of its last turn
      const int number = modules_[module_at_].number;
      const CellsCache& cache = cells_cache_[static_cast<std::size_t>(number)];
      if (!cache.cells.empty()) pylontech::attach_cells(modules_, number, cache.cells);
      modules_[module_at_].cell_flags = cache.flags;
      modules_[module_at_].cells_unread = cache.unread;
      if (cache.remaining_ah >= 0) modules_[module_at_].capacity_ah = cache.remaining_ah;
      modules_[module_at_].balancing = cache.balancing;
      after_bat(now_ms);
      return;
    }
    if (step_ == Step::kDriver) {
      std::vector<std::uint8_t> next;
      const drivers::Rx result = driver_->timeout(next);
      if (result == drivers::Rx::kNext) { pending_ = next; deadline_ms_ = now_ms + kDriverTimeoutMs; }
      else if (result == drivers::Rx::kDone) driver_done(now_ms);
      else { stats_.last_error = "timeout"; abandon(now_ms); }
      return;
    }
    // a QMOD or QPIWS that has never been answered is done without in the end (see mode_missed)
    if (step_ == Step::kQmod && !mode_ever_ && ++mode_misses_ >= kGiveUpTries) { stats_.last_error = "timeout"; after_mode(now_ms); return; }
    if (step_ == Step::kQpiws && !warnings_ever_ && ++warnings_misses_ >= kGiveUpTries) { stats_.last_error = "timeout"; warnings_.clear(); after_main_readings(now_ms); return; }
    if (step_ == Step::kQpigs2) { miss_pv2(now_ms); return; }
    if (step_ == Step::kQpgs) { ++unit_at_; ask_unit(now_ms); return; }
    if (is_identity_step()) { identity_missed(); if (step_ == Step::kQpiri && have_ratings_) ratings_at_ms_ = now_ms; if (step_ == Step::kQet && have_energy_) energy_at_ms_ = now_ms; next_identity(now_ms); return; }
    if (step_ == Step::kInfo) { ask_stat(now_ms); return; }
    if (step_ == Step::kStat) { ask_event_or_end(now_ms); return; }
    if (step_ == Step::kEvent) { ask_soh(now_ms); return; }
    if (step_ == Step::kSoh) { end_extras(now_ms); return; }
    if (step_ == Step::kSheet) { after_bat(now_ms); return; }   // no sheet: the reading goes without the code
    if (step_ == Step::kSystem) { system_missed(); finish(now_ms); return; }
    if (step_ == Step::kExtended) { extended_missed(now_ms); finish(now_ms); return; }
    stats_.last_error = "timeout";
    if (kind_ == config::Kind::kPylontech) { wake_ = true; slave_module_ = 0; slave_sheet_ = {}; }   // the console did not answer: the next try starts with a bare Enter
    if (kind_ == config::Kind::kVoltronic && auto_) {
      if (known_ && ++misses_ >= 3) { known_ = false; misses_ = 0; }
      if (!known_) dialect_ = next_dialect(dialect_);   // look for the next one at the next try
      abandon(now_ms);
      if (!known_) next_cycle_ms_ = now_ms + 300;
      return;
    }
    if (kind_ == config::Kind::kAnt) {
      if (ant_known_ && ++ant_misses_ >= 3) { ant_known_ = false; ant_misses_ = 0; }
      ant_misses_hint(now_ms);
      abandon(now_ms);
      if (!ant_known_) next_cycle_ms_ = now_ms + 300;   // look for the protocol without waiting a whole poll
      return;
    }
    abandon(now_ms);
  }

  config::Kind kind_;
  std::string node_id_, device_;
  std::uint64_t poll_ms_;
  int max_modules_;
  int highest_seen_ = 0;   // the highest place a battery has been in since the port started: one that goes away is still told as not there
  int cells_per_cycle_ = 0;
  int bat_cursor_ = 1;                // the module whose cells come next in the rotation
  std::uint32_t cells_wanted_ = 0;    // bit n: this cycle asks the cells of module n
  // zeros: the cells that read 0 V in the last table of the module, published or not (see rx_pylontech)
  bool sheet_asked_ = false;   // the sheet of the module in turn has been asked (or given up on)
  struct CellsCache { std::vector<double> cells; double remaining_ah = -1; int balancing = -1; std::vector<pylontech::CellFlag> flags; std::vector<int> unread; std::uint32_t zeros = 0; };
  std::array<CellsCache, 17> cells_cache_;   // by module number: the cells and remaining charge of the module's last turn
  Step step_ = Step::kIdle;
  std::uint64_t deadline_ms_ = 0, next_cycle_ms_ = 0, cycle_started_ms_ = 0, last_rx_ms_ = 0, last_reading_ms_ = 0;
  std::vector<std::uint8_t> pending_;
  std::string buffer_;
  voltronic::ReplyFramer framer_;
  voltronic::pi18::ReplyFramer framer18_;
  Dialect dialect_ = Dialect::kPi30;
  bool auto_ = true, known_ = false;   // "auto": the dialect is looked for; known: it answered (or it was chosen)
  bool tx_plain_ = false;              // the last question went without CRC
  bool crc_reply_seen_ = false, pi00_crc_replies_ = false, pi00_layout_ = false, pi16_ = false;   // pi00: a CRC reply was seen; its replies carry one; its QPIGS is the PI00 arrangement
  int misses_ = 0;
  voltronic::Status status_;
  bool pv2_wanted_ = false, pv2_absent_ = false, has_pv2_ = false;
  int pv2_misses_ = 0;
  voltronic::Pv2 pv2_;
  int parallel_ = 0, unit_at_ = 0;
  std::vector<voltronic::ParallelUnit> units_;
  std::string serial_, firmware_;                       // QID and QVFW
  voltronic::Ratings ratings_;                          // QPIRI
  bool have_ratings_ = false, have_energy_ = false;
  double energy_wh_ = 0;                                // QET
  std::uint64_t ratings_at_ms_ = 0, energy_at_ms_ = 0;
  bool identity_wanted_ = false;
  struct Asked { std::uint64_t at_ms = 0; int misses = 0; };
  bool extended_wanted_ = false;
  voltronic::Extended more_;
  std::array<Asked, static_cast<std::size_t>(More::kCount)> asked_{};
  std::size_t more_at_ = 0;
  pylontech::SystemSheet system_;                       // `pwrsys` of this cycle (valid = false: not read)
  int system_misses_ = 0;
  std::uint64_t system_tried_ms_ = 0;
  int qid_tries_ = 0, qvfw_tries_ = 0, qpiri_tries_ = 0, qet_tries_ = 0;
  char mode_ = 0;
  std::vector<std::string> warnings_;
  bool warnings_read_ = false;                          // the warnings of this cycle are the inverter's own answer
  bool mode_ever_ = false, warnings_ever_ = false;
  int mode_misses_ = 0, warnings_misses_ = 0;
  std::unique_ptr<drivers::Driver> driver_;             // only for the protocols that have one
  std::vector<std::uint8_t> last_tx_;
  std::vector<pylontech::Module> modules_;
  std::array<Extras, 17> extras_;   // by module number (1 to 16): what `info` and `stat` said, kept between cycles
  bool had_capacity_ = false;
  bool caught_up_ = false, catching_up_ = false;   // this cycle has asked a missing sheet again; the exchange under way is that one
  int slave_module_ = 0;                           // the last `pwr` was the sheet of one battery in this place of its stack
  pylontech::SlaveSheet slave_sheet_;              // ... and what it said
  bool slow_sheet_ = false;                        // `info` and `stat` of this module are given the longer time
  std::uint64_t console_timeout_ms_ = kConsoleExtraTimeoutMs;   // how long the console question under way is given
  bool settling_ = false;                          // a question timed out: the next one waits for silence
  std::uint64_t settle_started_ms_ = 0;
  std::size_t module_at_ = 0;
  ant::Framer ant_framer_;
  ant::Protocol ant_protocol_ = ant::Protocol::kOld;
  ant::Reading ant_reading_;
  bool ant_known_ = false, ant_seen_ = false;
  int ant_misses_ = 0;
  bool ready_ = false;
  bool wake_ = true;                  // the next `pwr` is preceded by a bare Enter (at the start and after a silence)
  PortStats stats_;
  RawLog raw_;
  CaptureSink capture_;
};

}  // namespace armor::solar
