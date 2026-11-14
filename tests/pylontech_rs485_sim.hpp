// ARMOR-NODE-SOLAR - a simulated RS-485 line with a Pylontech battery stack at the end of it, for the tests of the summary of the system.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The master (the Poller) writes its commands and reads what the stack answers, a few bytes at a time, as a half-duplex line gives them. The console of the stack echoes what it is
// typed and answers `pwr` (the table of the modules) and `bat N` (the cells of one module) in the layout of the console documentation; the identity and history commands answer
// "Unknown command" (the Poller goes on without them). The faults that a real line has can be switched on one by one: a stack that never answers, noise, a reply that arrives in
// small pieces, a line cut in the middle of a reply, a module that disappears. NOTHING here has run on a real battery or a real line.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sim {

struct Module {
  bool present = true;
  int voltage_mv = 49872, current_ma = -1280, temperature_mc = 22000, low_mc = 20000, high_mc = 25000, cell_low_mv = 3330, cell_high_mv = 3348, soc = 88;
  std::string state = "Dischg";
  int cells = 15;
  int cell_mv = 3325;       // every cell reads this, except the first, which reads cell_high_mv
};

class PylontechRs485 {
 public:
  std::vector<Module> modules;
  // faults
  bool silent = false;              // never answers
  bool noise = false;               // answers bytes that are no console text
  std::size_t chunk = 0;            // 0: the reply comes whole; n: n bytes per read
  std::size_t cut_after = 0;        // 0: whole; n: the line is cut after n bytes of each reply (the rest never comes)
  int commands_seen = 0;

  // The master's bytes.
  void feed(const std::uint8_t* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
      const char c = static_cast<char>(data[i]);
      if (c == '\n') continue;
      if (c == '\r') { const std::string typed = line_; line_.clear(); if (!typed.empty()) handle(typed); }
      else line_ += c;
    }
  }
  void feed(const std::vector<std::uint8_t>& bytes) { feed(bytes.data(), bytes.size()); }

  // What the line carries towards the master now (one read).
  std::vector<std::uint8_t> read() {
    const std::size_t n = chunk == 0 ? out_.size() : std::min(chunk, out_.size());
    std::vector<std::uint8_t> bytes(out_.begin(), out_.begin() + static_cast<std::ptrdiff_t>(n));
    out_.erase(out_.begin(), out_.begin() + static_cast<std::ptrdiff_t>(n));
    return bytes;
  }

 private:
  static std::string crlf(const std::string& s) { std::string o; for (char c : s) { if (c == '\n') o += "\r\n"; else o += c; } return o; }
  void say(const std::string& text) {
    if (silent) return;
    std::string t = noise ? std::string("\x01\x02garbage\xff\xfe\r\n") : crlf(text);
    if (cut_after != 0 && t.size() > cut_after) t.resize(cut_after);
    out_.insert(out_.end(), t.begin(), t.end());
  }
  static std::string num(int v) { return std::to_string(v); }

  std::string pwr_text() const {
    std::string t = "@\n";
    t += "Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time                 B.V.St   B.T.St\n";
    for (std::size_t i = 0; i < modules.size(); ++i) {
      const Module& m = modules[i];
      if (!m.present) { t += num(static_cast<int>(i + 1)) + "     -      -      -      -      -      -      -      Absent   -        -        -        -        -                    -        -\n"; continue; }
      t += num(static_cast<int>(i + 1)) + " " + num(m.voltage_mv) + " " + num(m.current_ma) + " " + num(m.temperature_mc) + " " + num(m.low_mc) + " " + num(m.high_mc) + " " + num(m.cell_low_mv) + " " +
           num(m.cell_high_mv) + " " + m.state + " Normal Normal Normal " + num(m.soc) + "% 2018-11-14 15:12:04 Normal Normal\n";
    }
    return t + "Command completed successfully\n$$\npylon>";
  }
  std::string bat_text(int number) const {
    if (number < 1 || static_cast<std::size_t>(number) > modules.size() || !modules[static_cast<std::size_t>(number - 1)].present) return "@\nInvalid command or fail to excute.\n$$\npylon>";
    const Module& m = modules[static_cast<std::size_t>(number - 1)];
    std::string t = "@\nBattery Volt Curr Tempr Base State Volt. State Curr. State Temp. State Coulomb\n";
    for (int i = 0; i < m.cells; ++i) t += num(i) + " " + num(i == 0 ? m.cell_high_mv : m.cell_mv) + " " + num(m.current_ma) + " " + num(m.temperature_mc) + " " + m.state + " Normal Normal Normal " + num(m.soc) + "%\n";
    return t + "Command completed successfully\n$$\npylon>";
  }

  void handle(const std::string& line) {
    ++commands_seen;
    const std::string echo = line + "\n";
    if (line == "pwr") { say(echo + pwr_text()); return; }
    if (line.rfind("bat ", 0) == 0) { say(echo + bat_text(std::atoi(line.c_str() + 4))); return; }
    say(echo + "@\nUnknown command '" + line + "' - try 'help'\n$$\npylon>");
  }

  std::string line_;
  std::vector<std::uint8_t> out_;
};

}  // namespace sim
