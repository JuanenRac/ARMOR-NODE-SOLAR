// ARMOR-NODE-SOLAR - the probe of a serial port, a diagnosis for equipment that says nothing: every speed, format and polarity the port can do, with every READING question the node
// knows, and a record of everything that was sent and everything that came back.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// It can only read, by construction: the questions are a fixed table in this file, built by the same builders the readings use (which only know reading commands); nothing from outside
// becomes bytes on the wire (the panel gives a port number, start and stop, nothing else); and `read_only()` checks every frame again, independently, right before it is sent.
// There is no setting command, no Modbus function but 03 and 04, no control frame of any battery board and no VE.Direct "set".
//
// No hardware here: the firmware gives it a line (configure, discard, read, write, framing_errors) and a clock, one step at a time, so that it can be stopped between two steps and so
// that the ports sharing a multiplexed UART can take their turns in between. Nothing here has run against real equipment.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ant_bms.hpp"
#include "driver_bms.hpp"
#include "driver_modbus.hpp"
#include "driver_vedirect.hpp"
#include "voltronic.hpp"
#include "voltronic_pi18.hpp"

namespace armor::solar::probe {

using Bytes = std::vector<std::uint8_t>;

enum class Format : std::uint8_t { k8N1, k8E1, k8N2 };
inline const char* format_name(Format f) { return f == Format::k8E1 ? "8E1" : f == Format::k8N2 ? "8N2" : "8N1"; }

// The speeds tried, the likeliest first (the port's own goes before all of them).
constexpr std::array<int, 8> kBauds{2400, 9600, 19200, 115200, 4800, 1200, 38400, 57600};
constexpr int kInverterMaxBaud = 19200;   // inverters talk slowly: faster speeds on their port only when asked for

// ---- the questions ------------------------------------------------------------------------------------------------------------------------------------------

enum class Family : std::uint8_t { kVoltronicCrc, kVoltronicPlain, kPi18, kPi17, kModbusHolding, kModbusInput, kDaly, kJbd, kJk, kAntOld, kAntNew, kVedirectHex, kPylontech };

struct Query {
  const char* name;
  Family family;
  const char* text;       // the command, for the text protocols
  std::uint8_t slave;     // Modbus
  std::uint16_t number;   // Modbus: the first register; DALY and JBD: the data id; VE.Direct: the register
  bool core;              // also asked in the formats other than 8N1
};

constexpr std::array<Query, 37> kQueries{{
    {"Voltronic QPI with CRC (PI30)", Family::kVoltronicCrc, "QPI", 0, 0, true},
    {"Voltronic QPIGS with CRC (PI30)", Family::kVoltronicCrc, "QPIGS", 0, 0, false},
    {"Voltronic QPI without CRC (PI00/PI16)", Family::kVoltronicPlain, "QPI", 0, 0, true},
    {"Voltronic QPIGS without CRC (PI00/PI16)", Family::kVoltronicPlain, "QPIGS", 0, 0, false},
    {"Voltronic ^P005PI (PI18)", Family::kPi18, "PI", 0, 0, false},
    {"Voltronic ^P005GS (PI18)", Family::kPi18, "GS", 0, 0, false},
    {"Voltronic ^P003PI (PI17)", Family::kPi17, "PI", 0, 0, false},
    {"Voltronic ^P003GS (PI17)", Family::kPi17, "GS", 0, 0, false},
    {"Modbus 03 slave 1 register 0x0000", Family::kModbusHolding, "", 1, 0x0000, true},
    {"Modbus 03 slave 2 register 0x0000", Family::kModbusHolding, "", 2, 0x0000, true},
    {"Modbus 03 slave 3 register 0x0000", Family::kModbusHolding, "", 3, 0x0000, true},
    {"Modbus 03 slave 4 register 0x0000", Family::kModbusHolding, "", 4, 0x0000, true},
    {"Modbus 03 slave 5 register 0x0000", Family::kModbusHolding, "", 5, 0x0000, true},
    {"Modbus 03 slave 6 register 0x0000", Family::kModbusHolding, "", 6, 0x0000, true},
    {"Modbus 03 slave 7 register 0x0000", Family::kModbusHolding, "", 7, 0x0000, true},
    {"Modbus 03 slave 8 register 0x0000", Family::kModbusHolding, "", 8, 0x0000, true},
    {"Modbus 03 slave 9 register 0x0000", Family::kModbusHolding, "", 9, 0x0000, true},
    {"Modbus 03 slave 10 register 0x0000", Family::kModbusHolding, "", 10, 0x0000, true},
    {"Modbus 04 slave 1 register 0x0000", Family::kModbusInput, "", 1, 0x0000, false},
    {"Modbus 04 slave 2 register 0x0000", Family::kModbusInput, "", 2, 0x0000, false},
    {"Modbus 04 slave 3 register 0x0000", Family::kModbusInput, "", 3, 0x0000, false},
    {"Modbus 04 slave 4 register 0x0000", Family::kModbusInput, "", 4, 0x0000, false},
    {"Modbus 04 slave 5 register 0x0000", Family::kModbusInput, "", 5, 0x0000, false},
    {"Modbus 04 slave 6 register 0x0000", Family::kModbusInput, "", 6, 0x0000, false},
    {"Modbus 04 slave 7 register 0x0000", Family::kModbusInput, "", 7, 0x0000, false},
    {"Modbus 04 slave 8 register 0x0000", Family::kModbusInput, "", 8, 0x0000, false},
    {"Modbus 04 slave 9 register 0x0000", Family::kModbusInput, "", 9, 0x0000, false},
    {"Modbus 04 slave 10 register 0x0000", Family::kModbusInput, "", 10, 0x0000, false},
    {"Modbus 03 slave 1 register 201 (SMG II)", Family::kModbusHolding, "", 1, 201, false},
    {"Modbus 03 slave 1 register 0x0100 (SRNE)", Family::kModbusHolding, "", 1, 0x0100, false},
    {"DALY 0x90 voltage and current", Family::kDaly, "", 0, 0x90, false},
    {"JBD 0x03 basic information", Family::kJbd, "", 0, 0x03, false},
    {"JK read all data", Family::kJk, "", 0, 0, false},
    {"ANT-BMS status (old protocol)", Family::kAntOld, "", 0, 0, false},
    {"ANT-BMS status (new protocol)", Family::kAntNew, "", 0, 0, false},
    {"VE.Direct HEX get 0xED8D", Family::kVedirectHex, "", 0, 0xED8D, false},
    {"Pylontech console pwr", Family::kPylontech, "pwr", 0, 0, false},
}};

// The bytes of a question. Only the table above goes in.
inline Bytes build(const Query& q) {
  Bytes out;
  switch (q.family) {
    case Family::kVoltronicCrc: voltronic::build_command(q.text, out); break;
    case Family::kVoltronicPlain: voltronic::build_plain_command(q.text, out); break;
    case Family::kPi18: voltronic::pi18::build_command(q.text, out); break;
    case Family::kPi17: voltronic::pi17::build_command(q.text, out); break;
    case Family::kModbusHolding: out = drivers::modbus_read(q.slave, q.number, 1); break;
    case Family::kModbusInput: {
      out = {q.slave, 0x04, static_cast<std::uint8_t>(q.number >> 8), static_cast<std::uint8_t>(q.number), 0x00, 0x01};
      const std::uint16_t crc = drivers::crc16_modbus(out.data(), out.size());
      out.push_back(static_cast<std::uint8_t>(crc));
      out.push_back(static_cast<std::uint8_t>(crc >> 8));
      break;
    }
    case Family::kDaly: out = drivers::DalyDriver::request(static_cast<std::uint8_t>(q.number)); break;
    case Family::kJbd: out = drivers::JbdDriver::request(static_cast<std::uint8_t>(q.number)); break;
    case Family::kJk: out = drivers::JkDriver::request(); break;
    case Family::kAntOld: out = ant::request(ant::Protocol::kOld); break;
    case Family::kAntNew: out = ant::request(ant::Protocol::kNew); break;
    case Family::kVedirectHex: out = drivers::VedirectHexDriver::request(q.number); break;
    case Family::kPylontech: out = {'p', 'w', 'r', '\r'}; break;
  }
  return out;
}

// The second lock, which knows nothing of the table: true only for a frame that is one of the reading requests described here. Anything else is never written to the line.
inline bool read_only(const Bytes& f) {
  const std::size_t n = f.size();
  if (n < 4) return false;
  const auto is = [&f, n](std::string_view text) { return n == text.size() && std::equal(f.begin(), f.end(), text.begin(), [](std::uint8_t a, char b) { return a == static_cast<std::uint8_t>(b); }); };
  if (is("pwr\r")) return true;                 // the Pylontech console's table of the modules
  if (is(":78DED0034\n")) return true;          // VE.Direct HEX "get" (7) of register 0xED8D; "set" is 8
  // Voltronic: a question (Q...), with CRC or without
  for (const char* name : {"QPI", "QPIGS"}) {
    const std::string_view text(name);
    if (n < text.size() + 1 || f[n - 1] != '\r' || !std::equal(text.begin(), text.end(), f.begin(), [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; })) continue;
    if (n == text.size() + 1) return true;
    if (n == text.size() + 3) {
      const auto crc = voltronic::wire_crc(f.data(), text.size());
      if (f[text.size()] == crc[0] && f[text.size() + 1] == crc[1]) return true;
    }
  }
  // PI18 (^P...CRC CR) and PI17 (^P...CR): the reading words PI and GS only; a setting begins with ^S
  if (f[0] == '^') {
    for (const char* frame : {"^P005PI", "^P005GS"}) {
      const std::string_view text(frame);
      if (n != text.size() + 3 || f[n - 1] != '\r' || !std::equal(text.begin(), text.end(), f.begin(), [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; })) continue;
      const auto crc = voltronic::wire_crc(f.data(), text.size());
      return f[text.size()] == crc[0] && f[text.size() + 1] == crc[1];
    }
    return is("^P003PI\r") || is("^P003GS\r");
  }
  // Modbus RTU: functions 03 and 04 (read registers), slaves 1 to 10, one register
  if (n == 8 && f[0] >= 1 && f[0] <= 10 && (f[1] == 0x03 || f[1] == 0x04)) {
    const std::uint16_t crc = drivers::crc16_modbus(f.data(), 6);
    return f[4] == 0x00 && f[5] == 0x01 && f[6] == static_cast<std::uint8_t>(crc) && f[7] == static_cast<std::uint8_t>(crc >> 8);
  }
  // DALY: A5 40 <data id 0x90..0x98> 08, eight zeros
  if (n == 13 && f[0] == 0xA5 && f[1] == 0x40 && f[2] >= 0x90 && f[2] <= 0x98 && f[3] == 0x08) return std::all_of(f.begin() + 4, f.begin() + 12, [](std::uint8_t b) { return b == 0; });
  // JBD: DD A5 (read; 5A writes) <03, 04 or 05> 00
  if (n == 7 && f[0] == 0xDD && f[1] == 0xA5 && f[2] >= 0x03 && f[2] <= 0x05 && f[3] == 0x00 && f[6] == 0x77) return true;
  // JK: command 06, read all data
  if (n == 21 && f[0] == 0x4E && f[1] == 0x57 && f[8] == 0x06 && f[9] == 0x03 && f[11] == 0x00) return f == drivers::JkDriver::request();
  // ANT-BMS: the two requests for the readings
  if (f[0] == 0x5A) return f == Bytes{0x5A, 0x5A, 0x00, 0x00, 0x01, 0x01};
  if (f[0] == 0x7E) return n == 10 && f[1] == 0xA1 && f[2] == 0x01 && f == ant::request(ant::Protocol::kNew);
  return false;
}

// ---- what came back -----------------------------------------------------------------------------------------------------------------------------------------

// The protocol whose reply is inside `data`, or "" when none of them is (at a wrong speed the bytes are noise).
inline const char* recognise(const std::uint8_t* d, std::size_t n) {
  const auto contains = [d, n](std::string_view text) {
    return std::search(d, d + n, text.begin(), text.end(), [](std::uint8_t a, char b) { return a == static_cast<std::uint8_t>(b); }) != d + n;
  };
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint8_t* p = d + i;
    const std::size_t left = n - i;
    if (p[0] == '(' || p[0] == '^') {
      for (std::size_t end = 3; end < left; ++end) {
        if (p[end] != '\r') continue;
        std::string text;
        voltronic::pi18::Kind kind;
        if (p[0] == '(' && (voltronic::parse_reply(p, end + 1, text) || voltronic::parse_reply_plain(p, end + 1, text) || voltronic::parse_reply_revo(p, end + 1, text))) return "Voltronic";
        if (p[0] == '^' && voltronic::pi18::parse_reply(p, end + 1, kind, text)) return "Voltronic PI17/PI18";
      }
    }
    if (left >= 5 && p[0] >= 1 && p[0] <= 247) {
      std::size_t whole = 0;
      if (p[1] == 0x83 || p[1] == 0x84) whole = 5;
      else if ((p[1] == 0x03 || p[1] == 0x04) && p[2] >= 2 && (p[2] & 1) == 0) whole = 5u + p[2];
      if (whole != 0 && whole <= left) {
        const std::uint16_t crc = drivers::crc16_modbus(p, whole - 2);
        if (p[whole - 2] == static_cast<std::uint8_t>(crc) && p[whole - 1] == static_cast<std::uint8_t>(crc >> 8)) return "Modbus";
      }
    }
    if (left >= 13 && p[0] == 0xA5 && p[1] == 0x01 && p[3] == 0x08) {
      unsigned sum = 0;
      for (std::size_t k = 0; k < 12; ++k) sum += p[k];
      if (static_cast<std::uint8_t>(sum) == p[12]) return "DALY";
    }
    if (left >= 7 && p[0] == 0xDD && p[1] >= 0x03 && p[1] <= 0x05 && left >= 7u + p[3] && p[6u + p[3]] == 0x77) return "JBD";
    if (left >= 11 && p[0] == 0x4E && p[1] == 0x57 && p[10] != 0x00) return "JK";
    if (left >= 20 && p[0] == 0xAA && p[1] == 0x55 && p[2] == 0xAA && p[3] == 0xFF) return "ANT-BMS";
    if (left >= 10 && p[0] == 0x7E && p[1] == 0xA1 && p[2] == 0x11) return "ANT-BMS";
    if (left >= 8 && p[0] == ':' && p[1] == '7') {
      std::size_t k = 2;
      while (k < left && ((p[k] >= '0' && p[k] <= '9') || (p[k] >= 'A' && p[k] <= 'F'))) ++k;
      if (k >= 10 && k < left && p[k] == '\n') return "VE.Direct HEX";
    }
  }
  if (contains("\r\nPID\t") || contains("\r\nV\t") || contains("\r\nChecksum\t")) return "VE.Direct text";
  if (contains("pylon>") || contains("pylon_debug>") || contains("Command completed")) return "Pylontech console";
  return "";
}

enum class Result : std::uint8_t { kNothing, kEcho, kUnknown, kAnswer };

struct Verdict {
  Result result = Result::kNothing;
  bool echo = false;             // our own bytes came back (a bridge between TX and RX, or a line that echoes)
  const char* protocol = "";     // for kAnswer
};

// Tells our own bytes coming back (a bridge, an echo) from an answer.
inline Verdict judge(const Bytes& sent, const Bytes& got) {
  Verdict v;
  if (got.empty()) return v;
  std::size_t skip = 0;
  if (!sent.empty()) {
    while (got.size() - skip >= sent.size() && std::equal(sent.begin(), sent.end(), got.begin() + static_cast<std::ptrdiff_t>(skip))) { skip += sent.size(); v.echo = true; }
    // part of what was sent and nothing else
    if (!v.echo && got.size() >= 3 && got.size() < sent.size() && std::equal(got.begin(), got.end(), sent.begin())) { v.echo = true; skip = got.size(); }
  }
  if (skip >= got.size()) { v.result = Result::kEcho; return v; }
  v.protocol = recognise(got.data() + skip, got.size() - skip);
  v.result = v.protocol[0] != '\0' ? Result::kAnswer : Result::kUnknown;
  return v;
}

// Sixteen bytes a line, in hexadecimal with the printable characters beside them.
inline void hex_lines(const std::uint8_t* d, std::size_t n, std::string& out) {
  static const char kDigits[] = "0123456789ABCDEF";
  for (std::size_t at = 0; at < n; at += 16) {
    out += "      ";
    for (std::size_t i = at; i < at + 16; ++i) {
      if (i < n) { out += kDigits[d[i] >> 4]; out += kDigits[d[i] & 15]; out += ' '; }
      else out += "   ";
    }
    out += " |";
    for (std::size_t i = at; i < at + 16 && i < n; ++i) out += (d[i] >= 0x20 && d[i] <= 0x7E) ? static_cast<char>(d[i]) : '.';
    out += "|\n";
  }
}

// ---- the record ---------------------------------------------------------------------------------------------------------------------------------------------

// The text of a probe, kept in pieces so that it never needs one huge block. Not locked: whoever shares it between tasks locks around it.
class Log {
 public:
  static constexpr std::size_t kPiece = 4096;
  explicit Log(std::size_t capacity = 384 * 1024) : capacity_(capacity) {}
  // Adds text; when the record is full only what is `kept` (the summary) still goes in, and `truncated` says that steps are missing.
  void append(std::string_view text, bool kept = false) {
    if (!kept && size_ + text.size() > capacity_) { truncated_ = true; return; }
    while (!text.empty()) {
      if (pieces_.empty() || pieces_.back()->size() >= kPiece) { pieces_.push_back(std::make_unique<std::string>()); pieces_.back()->reserve(kPiece); }
      const std::size_t n = std::min(text.size(), kPiece - pieces_.back()->size());
      pieces_.back()->append(text.data(), n);
      text.remove_prefix(n);
      size_ += n;
    }
  }
  std::size_t size() const { return size_; }
  bool truncated() const { return truncated_; }
  // Copies up to `capacity` bytes starting at `from`; returns how many (0 at the end).
  std::size_t read(std::size_t from, char* out, std::size_t capacity) const {
    if (from >= size_) return 0;
    const std::string& piece = *pieces_[from / kPiece];   // every piece but the last is full
    const std::size_t at = from % kPiece, n = std::min(capacity, piece.size() - at);
    std::copy(piece.begin() + static_cast<std::ptrdiff_t>(at), piece.begin() + static_cast<std::ptrdiff_t>(at + n), out);
    return n;
  }
  std::string text() const {
    std::string out;
    out.reserve(size_);
    for (const auto& piece : pieces_) out += *piece;
    return out;
  }

 private:
  std::size_t capacity_, size_ = 0;
  bool truncated_ = false;
  std::vector<std::unique_ptr<std::string>> pieces_;
};

// ---- the probe ----------------------------------------------------------------------------------------------------------------------------------------------

struct Setup {
  int port = 1;                   // 1 to 10, for the record
  bool emulated = false;          // a software port: 8N1, normal polarity and the slow speeds only
  bool can_transmit = true;       // a port with no TX pin only listens
  int baud = 2400;                // the port's own settings, tried first
  bool invert = false;
  int max_baud = 115200;          // what the port can do at all: softuart::kMaxBaud for an emulated one
  bool inverter = false;          // an inverter port: kInverterMaxBaud at most unless `fast`
  bool fast = false;              // the person asked for the faster speeds too (only matters to an inverter port, and never past max_baud)
  std::string version;            // of the firmware, for the record
};

struct Clock {
  std::function<std::uint64_t()> uptime_ms;
  std::function<std::string()> stamp;   // the time of day (or the time since the start) for the record
};

struct Found {
  const char* query = "";
  const char* protocol = "";
  int baud = 0;
  Format format = Format::k8N1;
  bool inverted = false;
};

enum class State : std::uint8_t { kRunning, kDone, kStopped, kTimeLimit };
inline const char* state_name(State s) { return s == State::kRunning ? "running" : s == State::kDone ? "done" : s == State::kStopped ? "stopped" : "time_limit"; }

struct Progress {
  State state = State::kRunning;
  std::size_t step = 0, total = 0;
  std::size_t combinations = 0;        // speed, format and polarity tried so far
  std::size_t sent = 0;                // questions sent
  std::uint32_t rx_bytes = 0, framing_errors = 0;
  std::size_t answers = 0, unknown = 0, echoes = 0;   // steps with each
  std::vector<Found> found;            // the answers, the first of each protocol and settings
  std::vector<Found> noise;            // where bytes came that no protocol explains
  int baud = 0;                        // being tried now
  Format format = Format::k8N1;
  bool inverted = false;
  const char* query = "";
  // "answered", "bytes" (something came that nothing explains), "echo" (only our own bytes) or "silent"
  const char* verdict() const { return answers > 0 ? "answered" : unknown > 0 ? "bytes" : echoes > 0 ? "echo" : "silent"; }
};

class Probe {
 public:
  static constexpr std::uint64_t kActiveLimitMs = 8 * 60'000;    // time spent on the line
  static constexpr std::uint64_t kWallLimitMs = 20 * 60'000;     // and from start to end (a multiplexed line is shared with the other ports)
  static constexpr std::uint64_t kListenMs = 1'500;
  static constexpr std::uint64_t kInverterWaitMs = 700;          // a Voltronic answer may take this long to begin
  static constexpr std::uint64_t kWaitMs = 300;
  static constexpr std::uint64_t kQuietMs = 200;                 // an answer is over after this much silence
  static constexpr std::uint64_t kStepLimitMs = 2'500;
  static constexpr std::size_t kMaxKept = 1024;                  // bytes of one step written in the record (all are counted)
  static constexpr std::size_t kMaxFound = 12;

  Probe(Setup setup, Clock clock) : setup_(std::move(setup)), clock_(std::move(clock)) {
    plan();
    progress_.total = steps_.size();
    started_ms_ = clock_.uptime_ms();
  }

  bool finished() const { return progress_.state != State::kRunning; }
  const Progress& progress() const { return progress_; }

  // The first lines of the record.
  std::string header() const {
    std::string out = "A.R.M.O.R. solar node - serial port probe\n";
    out += "port " + std::to_string(setup_.port) + (setup_.emulated ? " (emulated)" : " (UART)") + ", firmware " + setup_.version + ", started " + clock_.stamp() + "\n";
    out += "the port's own settings: " + std::to_string(setup_.baud) + " 8N1 " + (setup_.invert ? "inverted" : "normal") + "\n";
    out += "only reading questions are sent; " + std::to_string(steps_.size()) + " steps\n";
    if (setup_.emulated) out += "an emulated port: 8N1 and normal polarity only, up to " + std::to_string(setup_.max_baud) + " baud\n";
    if (!setup_.can_transmit) out += "this port has no TX pin: it only listens\n";
    std::string tried, beyond, unasked;
    std::array<int, kBauds.size()> all = kBauds;
    std::sort(all.begin(), all.end());
    for (const int baud : all) {
      std::string& into = baud > setup_.max_baud ? beyond : (baud > top_baud() && baud != setup_.baud) ? unasked : tried;
      into += (into.empty() ? "" : ", ") + std::to_string(baud);
    }
    out += "speeds tried: " + tried + "\n";
    if (!beyond.empty()) out += "speeds skipped: " + beyond + " (this port reaches " + std::to_string(setup_.max_baud) + " baud at most)\n";
    if (!unasked.empty()) out += "speeds skipped: " + unasked + " (an inverter port, and the faster speeds were not asked for)\n";
    out += "\n";
    return out;
  }

  // Stops between two steps: nothing else is sent.
  void stop() { if (!finished()) progress_.state = State::kStopped; }

  // One step on the line: sets it, sends one question (or none), waits for what comes. Returns the lines for the record. `cancel` cuts the wait short.
  template <class Line>
  std::string step(Line& line, const std::atomic<bool>* cancel = nullptr) {
    std::string out;
    if (finished()) return out;
    const std::uint64_t begun = clock_.uptime_ms();
    if (cancel != nullptr && cancel->load()) { stop(); return out; }
    if (active_ms_ >= kActiveLimitMs || begun - started_ms_ >= kWallLimitMs) { progress_.state = State::kTimeLimit; return out; }
    if (at_ >= steps_.size()) { progress_.state = State::kDone; return out; }
    const Step s = steps_[at_++];
    progress_.step = at_;
    progress_.baud = s.baud; progress_.format = s.format; progress_.inverted = s.inverted;
    const Query* query = s.query < 0 ? nullptr : &kQueries[static_cast<std::size_t>(s.query)];
    progress_.query = query == nullptr ? "listen only" : query->name;
    const std::string where = std::to_string(s.baud) + " " + format_name(s.format) + (s.inverted ? " inverted" : " normal");
    out += "#" + std::to_string(at_) + " " + clock_.stamp() + "  " + where + "  " + progress_.query + "\n";
    if (!line.configure(s.baud, s.inverted, s.format)) {
      out += "    the port cannot be set like this: skipped\n";
    } else {
      note_combination(s);
      line.discard();
      const std::uint32_t errors_before = line.framing_errors();
      const Bytes frame = query == nullptr ? Bytes() : build(*query);
      Bytes got;
      std::size_t count = 0;
      bool sent = true;
      if (query == nullptr) {
        out += "    sent: nothing\n";
      } else if (!read_only(frame)) {       // cannot happen with the table above; if it ever did, nothing goes out
        out += "    NOT sent: not a reading question\n";
        sent = false;
      } else {
        out += "    sent " + std::to_string(frame.size()) + " bytes:\n";
        hex_lines(frame.data(), frame.size(), out);
        sent = line.write(frame.data(), frame.size());
        if (sent) ++progress_.sent; else out += "    the port could not send it\n";
      }
      if (sent) receive(line, query, got, count, cancel);
      const std::uint32_t errors = line.framing_errors() - errors_before;
      progress_.framing_errors += errors;
      progress_.rx_bytes += static_cast<std::uint32_t>(count);
      const Verdict v = judge(frame, got);
      if (count == 0) out += "    received: nothing\n";
      else {
        out += "    received " + std::to_string(count) + " bytes";
        if (v.result == Result::kEcho) out += " - ECHO of what was sent (a bridge between TX and RX, or a line that echoes)";
        else if (v.result == Result::kAnswer) out += std::string(v.echo ? " - after an echo," : " -") + " ANSWER (" + v.protocol + ")";
        else out += std::string(v.echo ? " - an echo and then bytes" : " - bytes") + " that no protocol explains";
        out += ":\n";
        hex_lines(got.data(), got.size(), out);
        if (count > got.size()) out += "      ... and " + std::to_string(count - got.size()) + " more bytes\n";
      }
      if (errors > 0) out += "    framing or parity errors: " + std::to_string(errors) + "\n";
      tally(s, query, v);
    }
    active_ms_ += clock_.uptime_ms() - begun;
    if (cancel != nullptr && cancel->load()) stop();
    else if (at_ >= steps_.size()) progress_.state = State::kDone;
    return out;
  }

  // The end of the record: what was found.
  std::string summary() const {
    const Progress& p = progress_;
    std::string out = "\nSUMMARY (port " + std::to_string(setup_.port) + ", " + clock_.stamp() + ")\n";
    out += std::string("  ") + (p.state == State::kStopped ? "stopped by hand" : p.state == State::kTimeLimit ? "stopped at the time limit" : p.state == State::kDone ? "every step done" : "still running") + ": "
           + std::to_string(p.step) + " of " + std::to_string(p.total) + " steps, " + std::to_string(p.combinations) + " combinations of speed, format and polarity, " + std::to_string(p.sent) + " questions sent\n";
    out += "  bytes received: " + std::to_string(p.rx_bytes) + "; framing or parity errors: " + std::to_string(p.framing_errors) + "\n";
    out += "  steps with an answer: " + std::to_string(p.answers) + "; with unexplained bytes: " + std::to_string(p.unknown) + "; with only an echo: " + std::to_string(p.echoes) + "\n";
    for (const Found& f : p.found) out += std::string("  ANSWERED ") + f.query + " at " + std::to_string(f.baud) + " " + format_name(f.format) + (f.inverted ? " inverted" : " normal") + " (" + f.protocol + ")\n";
    for (const Found& f : p.noise) out += std::string("  bytes at ") + std::to_string(f.baud) + " " + format_name(f.format) + (f.inverted ? " inverted" : " normal") + " after " + f.query + "\n";
    if (p.answers == 0 && p.unknown == 0 && p.echoes == 0) out += "  NOT ONE BYTE in " + std::to_string(p.combinations) + " combinations\n";
    else if (p.answers == 0 && p.unknown == 0) out += "  only an ECHO: the node hears itself (a bridge between TX and RX) and nothing else\n";
    return out;
  }

 private:
  struct Step {
    int baud;
    Format format;
    bool inverted;
    int query;   // index in kQueries, -1: only listen
  };

  // the fastest speed this probe goes to
  int top_baud() const { return setup_.inverter && !setup_.fast ? std::min(setup_.max_baud, kInverterMaxBaud) : setup_.max_baud; }

  void plan() {
    std::vector<int> bauds;
    const auto add_baud = [this, &bauds](int baud) {
      // the port's own speed is the person's choice: tried whenever the port can do it
      if (baud >= 1200 && (baud <= top_baud() || (baud == setup_.baud && baud <= setup_.max_baud)) && std::find(bauds.begin(), bauds.end(), baud) == bauds.end()) bauds.push_back(baud);
    };
    add_baud(setup_.baud);
    for (const int baud : kBauds) add_baud(baud);
    std::vector<bool> polarities{!setup_.emulated && setup_.invert};
    if (!setup_.emulated) polarities.push_back(!polarities.front());
    std::vector<Format> others;
    if (!setup_.emulated) others = {Format::k8E1, Format::k8N2};
    // first only listening: whoever talks by itself is heard before anything is sent
    for (const int baud : bauds) for (const bool inverted : polarities) steps_.push_back({baud, Format::k8N1, inverted, -1});
    if (!setup_.can_transmit) {
      for (const Format format : others) for (const int baud : bauds) for (const bool inverted : polarities) steps_.push_back({baud, format, inverted, -1});
      return;
    }
    for (const int baud : bauds)
      for (const bool inverted : polarities)
        for (std::size_t q = 0; q < kQueries.size(); ++q) steps_.push_back({baud, Format::k8N1, inverted, static_cast<int>(q)});
    for (const Format format : others)
      for (const int baud : bauds)
        for (std::size_t q = 0; q < kQueries.size(); ++q)
          if (kQueries[q].core) steps_.push_back({baud, format, polarities.front(), static_cast<int>(q)});
  }

  template <class Line>
  void receive(Line& line, const Query* query, Bytes& got, std::size_t& count, const std::atomic<bool>* cancel) {
    const bool inverter = query != nullptr && (query->family == Family::kVoltronicCrc || query->family == Family::kVoltronicPlain || query->family == Family::kPi18 || query->family == Family::kPi17);
    const std::uint64_t begun = clock_.uptime_ms();
    std::uint64_t deadline = begun + (query == nullptr ? kListenMs : inverter ? kInverterWaitMs : kWaitMs);
    std::uint8_t buffer[64];
    for (std::uint64_t now = begun; now < deadline; now = clock_.uptime_ms()) {
      if (cancel != nullptr && cancel->load()) return;
      const std::size_t n = line.read(buffer, sizeof buffer, 20);
      if (n == 0) continue;
      count += n;
      for (std::size_t i = 0; i < n && got.size() < kMaxKept; ++i) got.push_back(buffer[i]);
      if (query != nullptr) deadline = std::min(clock_.uptime_ms() + kQuietMs, begun + kStepLimitMs);
    }
  }

  void note_combination(const Step& s) {
    const std::uint32_t key = static_cast<std::uint32_t>(s.baud) * 8u + static_cast<std::uint32_t>(s.format) * 2u + (s.inverted ? 1u : 0u);
    if (std::find(combinations_.begin(), combinations_.end(), key) == combinations_.end()) combinations_.push_back(key);
    progress_.combinations = combinations_.size();
  }

  void tally(const Step& s, const Query* query, const Verdict& v) {
    Found f{query == nullptr ? "listen only" : query->name, v.protocol, s.baud, s.format, s.inverted};
    const auto same = [&f](const Found& g) { return g.baud == f.baud && g.format == f.format && g.inverted == f.inverted && std::string_view(g.protocol) == f.protocol; };
    if (v.echo) ++progress_.echoes;
    if (v.result == Result::kAnswer) {
      ++progress_.answers;
      if (progress_.found.size() < kMaxFound && std::none_of(progress_.found.begin(), progress_.found.end(), same)) progress_.found.push_back(f);
    } else if (v.result == Result::kUnknown) {
      ++progress_.unknown;
      if (progress_.noise.size() < kMaxFound && std::none_of(progress_.noise.begin(), progress_.noise.end(), same)) progress_.noise.push_back(f);
    }
  }

  Setup setup_;
  Clock clock_;
  std::vector<Step> steps_;
  std::vector<std::uint32_t> combinations_;
  Progress progress_;
  std::size_t at_ = 0;
  std::uint64_t started_ms_ = 0, active_ms_ = 0;
};

}  // namespace armor::solar::probe
