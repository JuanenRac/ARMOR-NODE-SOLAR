// ARMOR-NODE-SOLAR - Pylontech batteries (US2000, US3000, US5000 and the C versions): the text of the console, and the frames of the RS485 protocol.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Sources: what the batteries print on their console port and the public description of their RS485 frame; NOTHING here has been connected to a battery.
//
// CONSOLE (RJ45 "console", RS232 levels, 115200 baud 8N1): typing `pwr` and Enter prints a table with one row per module of the stack, the master
// answering for all of them, then "Command completed successfully" and a prompt. Columns, in order:
//     Power  Volt(mV)  Curr(mA)  Tempr(m°C)  Tlow  Thigh  Vlow(mV)  Vhigh(mV)  Base.St  Volt.St  Curr.St  Temp.St  Coulomb(%)  Time(date time)  B.V.St  B.T.St
// A module that is not there has "Absent" in its row. The columns are read by the NAMES in the header line (`Power Volt Curr ...`), because the firmwares
// differ: the older ones stop at B.T.St, others add MosTempr and M.T.St, and the newer ones (US5000 V2.3) add an Id column after Tlow, Thigh, Vlow and Vhigh and a
// SysAlarm.St at the end. Without a header line, the columns listed above are assumed. The Time column is two words (a date and a time).
//
// `bat <n>` prints a row per cell; the newer firmwares add the charge in mAH and whether the cell is being balanced (Y/N). `info <n>` prints the identity of the module
// (Device name, Specification such as "48V/74AH", Cell Number, versions) and `stat <n>` its history (CYCLE Times).
//
// RS485 (the other RJ45, 9600 or 115200 baud): frames of ASCII hexadecimal  '~' VER ADR CID1 CID2 LENGTH INFO CHKSUM CR,
//     LENGTH = the length of INFO in characters (12 bits) with a 4-bit check in front: the check is the sum of the three 4-bit groups of the length,
//              inverted, plus one, in 4 bits; CHKSUM = the sum of the ASCII codes from VER to the end of INFO, inverted, plus one, in 16 bits.
// Only the framing is here; the meaning of INFO differs between versions of the protocol and is not decoded until a capture from a real battery exists.
#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace armor::solar::pylontech {

// ---- the console ---------------------------------------------------------------------------------------------------------------------

// The counters of a battery's life that the console's `stat <module>` prints as "Name : value". The names are the console's own, and what a counter means is not documented by the maker
// in the sources we have: the groups and the "severe" mark are an interpretation (not official), kept short on purpose. A severe counter that goes up is worth telling about.
enum class StatGroup { kUse, kHealth, kVoltage, kCurrent, kTemperature, kOther };
struct StatCounterInfo {
  const char* console;   // the name as the console prints it, lowercase
  const char* key;       // the name in the message (lowercase letters, digits and underscores)
  StatGroup group;
  bool severe;           // one of the counters of a protection or an error: an increase is told
};
inline const std::vector<StatCounterInfo>& stat_catalog() {
  static const std::vector<StatCounterInfo> table = {
      {"data items", "data_items", StatGroup::kUse, false}, {"hisdata items", "hisdata_items", StatGroup::kUse, false}, {"charge times", "charge_times", StatGroup::kUse, false},
      {"shut times", "shut_times", StatGroup::kUse, false}, {"reset times", "reset_times", StatGroup::kUse, false}, {"idle times", "idle_times", StatGroup::kUse, false},
      {"soh times", "soh_times", StatGroup::kHealth, false}, {"lifewarn times", "lifewarn_times", StatGroup::kHealth, false}, {"lifealarm times", "lifealarm_times", StatGroup::kHealth, true},
      {"dsg cap", "dsg_cap", StatGroup::kHealth, false}, {"pwr percent", "pwr_percent", StatGroup::kHealth, false},
      {"bat ov times", "bat_ov_times", StatGroup::kVoltage, true}, {"bat hv times", "bat_hv_times", StatGroup::kVoltage, true}, {"bat lv times", "bat_lv_times", StatGroup::kVoltage, false},
      {"bat uv times", "bat_uv_times", StatGroup::kVoltage, true}, {"input ov times", "input_ov_times", StatGroup::kVoltage, true}, {"pwr ov times", "pwr_ov_times", StatGroup::kVoltage, true},
      {"pwr hv times", "pwr_hv_times", StatGroup::kVoltage, true}, {"pwr lv times", "pwr_lv_times", StatGroup::kVoltage, false}, {"pwr uv times", "pwr_uv_times", StatGroup::kVoltage, true},
      {"bat slp times", "bat_slp_times", StatGroup::kVoltage, false},
      {"coc times", "coc_times", StatGroup::kCurrent, true}, {"coc2 times", "coc2_times", StatGroup::kCurrent, true}, {"doc times", "doc_times", StatGroup::kCurrent, true},
      {"doc2 times", "doc2_times", StatGroup::kCurrent, true}, {"sc times", "sc_times", StatGroup::kCurrent, true}, {"coca times", "coca_times", StatGroup::kCurrent, false},
      {"doca times", "doca_times", StatGroup::kCurrent, false},
      {"cot times", "cot_times", StatGroup::kTemperature, true}, {"cut times", "cut_times", StatGroup::kTemperature, false}, {"dot times", "dot_times", StatGroup::kTemperature, true},
      {"dut times", "dut_times", StatGroup::kTemperature, false}, {"cht times", "cht_times", StatGroup::kTemperature, false}, {"clt times", "clt_times", StatGroup::kTemperature, false},
      {"dht times", "dht_times", StatGroup::kTemperature, false}, {"dlt times", "dlt_times", StatGroup::kTemperature, false}, {"ht cnt", "ht_cnt", StatGroup::kTemperature, false},
      {"lt cnt", "lt_cnt", StatGroup::kTemperature, false}, {"ht@0.5c cnt", "ht_half_c_cnt", StatGroup::kTemperature, false},
      {"bmicerr times", "bmicerr_times", StatGroup::kOther, true}, {"rv times", "rv_times", StatGroup::kOther, false}, {"system error times", "system_error_times", StatGroup::kOther, true},
      {"charge cnt.", "charge_cnt", StatGroup::kUse, false}, {"discharge cnt.", "discharge_cnt", StatGroup::kUse, false}, {"status cnt.", "status_cnt", StatGroup::kUse, false},
      {"pwr slp times", "pwr_slp_times", StatGroup::kVoltage, false}, {"lv cnt", "lv_cnt", StatGroup::kVoltage, false}, {"lt@0.5c cnt", "lt_half_c_cnt", StatGroup::kTemperature, false},
  };
  return table;
}

// One entry of the battery's event log, as `data event <item>` prints it. The protection and fault fields are kept as the console wrote them ("0x2 PHV"): the meaning of the
// numbers is not decoded here, the mnemonics beside them are what a person reads.
struct Event {
  int index = 0;
  std::string time;                              // "17-05-13 20:52:58" as printed (the battery's own clock)
  double voltage_v = 0, current_a = 0, temperature_c = 0;
  int percent = -1;
  std::string base_state, voltage_state, current_state, temperature_state, coulomb_state;   // Charge, High, Normal, Normal, Full ...
  std::string power_events, battery_events, system_fault;                                   // "0x2 PHV", "0x0", "0x0"
  bool valid = false;
};

struct Module {
  int number = 0;
  bool present = false;
  double voltage_v = 0, current_a = 0, temperature_c = 0, temperature_low_c = 0, temperature_high_c = 0;
  double cell_low_v = 0, cell_high_v = 0;
  int soc_percent = -1;               // the "Coulomb" column
  double capacity_ah = -1, full_capacity_ah = -1;   // remaining and total capacity in ampere-hours, from the console's `info` text; -1 when it was not read
  int cycles = -1;
  int health_percent = -1;            // the capacity the module has learned (`stat`'s Pwr Coulomb) against its rated one, in percent; -1 when unknown
  std::vector<std::pair<std::string, long long>> counters;   // the counters of `stat`'s text that are in stat_catalog(): the message key and the value, in the order the console printed them
  std::string model;                  // the device name from `info` (US3000C ...)
  int cell_count = -1;                // `info`: Cell Number
  std::string board_version, main_soft_version, soft_version;   // `info`: the board, the main software and the software of the module (V10R04, B68.2.0.0, V2.6)
  double max_charge_a = -1, max_discharge_a = -1;   // `info`: Max Charge Curr / Max Dischg Curr, the module's current limits in amperes (both positive); -1 when not told
  int console_baud = -1;              // `info`: Console Port rate (an old unit says 19200), -1 when not told
  std::vector<int> soh_counts;        // `soh`: the SOH count of each cell, in cell order (empty when it was not read)
  std::vector<int> soh_abnormal;      // the cells (0-based, like cells_v) whose SOH status is not Normal
  Event last_event;                   // the newest entry of the battery's event log (`data event`); valid = false when it was not read
  std::vector<double> cells_v;        // the voltage of each cell, in volts, from the console's `bat` table; empty when it was not read
  std::vector<double> temperatures_c;  // every temperature sensor of the module (a BMS that lists them); empty when the equipment does not
  std::string base_state, voltage_state, current_state, temperature_state;   // Idle, Charge, Dischg, Normal, Absent ...
  std::string cell_voltage_state, cell_temperature_state, mosfet_temperature_state, system_alarm_state;   // B.V.St, B.T.St, M.T.St, SysAlarm.St when the firmware prints them
  int balancing = -1;                 // how many cells are being balanced (the console's `bat` table, when its firmware tells); -1 when unknown
  bool has_power = false;             // a battery management system that measures its own power
  double power_w = 0;                 // that power: negative while discharging
  int charge_mos = -1, discharge_mos = -1;   // the status codes of the MOSFETs of a battery management system (ANT-BMS); -1 when the equipment does not say
  bool protecting = false;            // a protection has switched a MOSFET off
};

namespace detail {
inline std::vector<std::string> tokens(const std::string& line) {
  std::vector<std::string> out;
  std::string current;
  for (char c : line) {
    if (c == ' ' || c == '\t' || c == '\r') { if (!current.empty()) { out.push_back(current); current.clear(); } }
    else current += c;
  }
  if (!current.empty()) out.push_back(current);
  return out;
}
inline bool integer(const std::string& text, long& out) {
  if (text.empty() || text.size() > 11) return false;
  std::size_t i = (text[0] == '-' || text[0] == '+') ? 1 : 0;
  if (i >= text.size()) return false;
  long value = 0;
  for (std::size_t k = i; k < text.size(); ++k) { if (text[k] < '0' || text[k] > '9') return false; value = value * 10 + (text[k] - '0'); }
  out = text[0] == '-' ? -value : value;
  return true;
}
}  // namespace detail

// The modules of the text a `pwr` command printed (any number of lines, with or without the header and the prompt). Returns how many rows were read.
inline std::size_t parse_pwr(const std::string& text, std::vector<Module>& modules) {
  modules.clear();
  std::vector<std::string> columns{"Power", "Volt", "Curr", "Tempr", "Tlow", "Thigh", "Vlow", "Vhigh", "Base.St", "Volt.St", "Curr.St", "Temp.St", "Coulomb", "Time", "B.V.St", "B.T.St"};
  std::string line;
  const auto handle = [&](const std::string& row) {
    const std::vector<std::string> t = detail::tokens(row);
    if (t.size() >= 8 && t[0] == "Power" && t[1] == "Volt") {   // the header of this firmware; a name of two words ("LC. St") is one column
      columns.clear();
      for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i] == "LC." && i + 1 < t.size() && t[i + 1] == "St") { columns.push_back("LC.St"); ++i; }
        else columns.push_back(t[i]);
      }
      return;
    }
    long number = 0;
    if (t.empty() || !detail::integer(t[0], number) || number < 1 || number > 16) return;
    Module m;
    m.number = static_cast<int>(number);
    bool absent = false;
    for (const std::string& token : t) if (token == "Absent") absent = true;
    if (absent) { m.present = false; m.base_state = "Absent"; modules.push_back(m); return; }
    std::map<std::string, std::string> cell;
    std::size_t at = 1;
    for (std::size_t c = 1; c < columns.size() && at < t.size(); ++c) {
      if (columns[c] == "Time" && at + 1 < t.size() && t[at].find('-') != std::string::npos && t[at + 1].find(':') != std::string::npos) { at += 2; continue; }
      cell[columns[c]] = t[at++];
    }
    long v[7] = {};
    static const char* const kNumbers[7] = {"Volt", "Curr", "Tempr", "Tlow", "Thigh", "Vlow", "Vhigh"};
    for (std::size_t i = 0; i < 7; ++i) { const auto found = cell.find(kNumbers[i]); if (found == cell.end() || !detail::integer(found->second, v[i])) return; }
    for (const char* name : {"Base.St", "Volt.St", "Curr.St", "Temp.St", "Coulomb"}) if (cell.find(name) == cell.end()) return;
    m.present = true;
    m.voltage_v = v[0] / 1000.0; m.current_a = v[1] / 1000.0; m.temperature_c = v[2] / 1000.0;
    m.temperature_low_c = v[3] / 1000.0; m.temperature_high_c = v[4] / 1000.0; m.cell_low_v = v[5] / 1000.0; m.cell_high_v = v[6] / 1000.0;
    m.base_state = cell["Base.St"]; m.voltage_state = cell["Volt.St"]; m.current_state = cell["Curr.St"]; m.temperature_state = cell["Temp.St"];
    const auto state = [&](const char* name, std::string& into) { const auto found = cell.find(name); if (found != cell.end()) into = found->second; };
    state("B.V.St", m.cell_voltage_state); state("B.T.St", m.cell_temperature_state); state("M.T.St", m.mosfet_temperature_state); state("SysAlarm.St", m.system_alarm_state);
    {
      const std::string& soc = cell["Coulomb"];
      long percent = 0;
      if (!soc.empty() && soc.back() == '%' && detail::integer(soc.substr(0, soc.size() - 1), percent) && percent >= 0 && percent <= 100) m.soc_percent = static_cast<int>(percent);
    }
    const auto mos = cell.find("MosTempr");
    long mos_mc = 0;
    if (mos != cell.end() && detail::integer(mos->second, mos_mc)) m.temperatures_c = {m.temperature_c, mos_mc / 1000.0};   // the cells' sensor and the MOSFETs'
    modules.push_back(m);
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  return modules.size();
}

// The cells of a module from the text a `bat <module>` command printed: one row per cell with the index, the voltage (mV), the current (mA), the temperature
// (m°C), the four states and the charge (a percentage; the newer firmwares then give the charge in mAH and a Y or N for the cell being balanced). Returns how many cells were
// read; a row that is not one is skipped, and a cell voltage out of 0 to 10 V refuses the whole table. `remaining_ah` gets the charge in ampere-hours when a row has it (-1
// otherwise) and `balancing` how many cells are being balanced.
inline std::size_t parse_bat(const std::string& text, std::vector<double>& cells, double* remaining_ah = nullptr, int* balancing = nullptr) {
  cells.clear();
  if (remaining_ah != nullptr) *remaining_ah = -1;
  if (balancing != nullptr) *balancing = 0;
  std::string line;
  bool bad = false;
  const auto handle = [&](const std::string& row) {
    std::vector<std::string> t = detail::tokens(row);
    t.erase(std::remove(t.begin(), t.end(), std::string("-")), t.end());   // a table printed with " - " between its columns reads the same
    long index = 0, millivolts = 0;
    if (t.size() < 3 || !detail::integer(t[0], index) || index < 0 || index > 31 || !detail::integer(t[1], millivolts)) return;   // some firmwares print fewer columns than others
    if (millivolts < 0 || millivolts > 10000) { bad = true; return; }
    if (static_cast<std::size_t>(index) != cells.size()) { bad = true; return; }   // the cells come in order, from 0
    cells.push_back(millivolts / 1000.0);
    long milliamp_hours = 0;
    if (remaining_ah != nullptr && *remaining_ah < 0 && t.size() >= 11 && detail::integer(t[9], milliamp_hours) && milliamp_hours >= 0 && t[10] == "mAH") *remaining_ah = milliamp_hours / 1000.0;
    if (balancing != nullptr && t.size() >= 12 && t[11] == "Y") ++*balancing;
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  if (bad) cells.clear();
  return cells.size();
}

// What a module says about itself in the text of the console's `info <module>` command: lines of "Name : value". Read: the device name, the remaining and the total
// capacity (in mAH, or Ah when the unit says so) and the cycle count. Names are matched without regard to case and spaces; a line that is not one is skipped. Returns
// how many of the four were found. The exact names differ between firmware versions, so a missing one is left unset rather than guessed.
inline int parse_info(const std::string& text, Module& module) {
  int found = 0;
  std::string line;
  const auto lower = [](std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; };
  const auto trim = [](std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
  };
  const auto amp_hours = [&](const std::string& value, double& out) {
    const std::vector<std::string> t = detail::tokens(value);
    long n = 0;
    if (t.empty() || !detail::integer(t[0], n) || n < 0) return false;
    const std::string unit = t.size() > 1 ? lower(t[1]) : "mah";
    out = unit == "ah" ? static_cast<double>(n) : n / 1000.0;
    return out <= 100000.0;
  };
  long long coulomb_mas = -1;   // `stat`'s "Pwr Coulomb": the capacity the battery has learned, in milliampere-seconds (a new 50 Ah module says 180000000)
  double specified_ah = -1;   // the "48V/74AH" of the Specification line: the rated capacity, used when there is no Total Capacity line
  const auto handle = [&](const std::string& row) {
    const std::size_t colon = row.find(':');
    if (colon == std::string::npos) return;
    const std::string name = lower(trim(row.substr(0, colon))), value = trim(row.substr(colon + 1));
    long n = 0;
    for (const StatCounterInfo& counter : stat_catalog()) {
      if (name != counter.console) continue;
      const std::vector<std::string> words = detail::tokens(value);
      if (words.empty() || words[0].empty() || words[0].size() > 10) return;
      long long parsed = 0;
      for (char c : words[0]) { if (c < '0' || c > '9') return; parsed = parsed * 10 + (c - '0'); }
      bool known = false;
      for (auto& kept : module.counters) if (kept.first == counter.key) { kept.second = parsed; known = true; }
      if (!known && module.counters.size() < 48) module.counters.emplace_back(counter.key, parsed);
      ++found;
      return;
    }
    {
      std::string squeezed;   // the name with one space between the words ("Soft  version" is "soft version")
      for (const std::string& w : detail::tokens(name)) squeezed += (squeezed.empty() ? "" : " ") + w;
      std::string* into = squeezed == "board version" ? &module.board_version : squeezed == "main soft version" ? &module.main_soft_version : squeezed == "soft version" ? &module.soft_version : nullptr;
      if (into != nullptr) {
        const std::vector<std::string> w = detail::tokens(value);
        if (!w.empty() && w[0].size() <= 24) *into = w[0];
        return;
      }
    }
    if (name == "cell number") { const std::vector<std::string> w = detail::tokens(value); if (!w.empty() && detail::integer(w[0], n) && n >= 1 && n <= 32) module.cell_count = static_cast<int>(n); return; }
    if (name == "max charge curr" || name == "max dischg curr") {   // "53000mA" or "-55000mA": the figure and its unit, with no space between them
      std::string digits = value;
      if (digits.size() > 2 && lower(digits.substr(digits.size() - 2)) == "ma") digits.erase(digits.size() - 2);
      if (detail::integer(trim(digits), n) && n != 0 && n > -1000000 && n < 1000000) (name == "max charge curr" ? module.max_charge_a : module.max_discharge_a) = (n < 0 ? -n : n) / 1000.0;
      return;
    }
    if (name == "console port rate") { const std::vector<std::string> w = detail::tokens(value); if (!w.empty() && detail::integer(w[0], n) && n >= 300 && n <= 1000000) module.console_baud = static_cast<int>(n); return; }
    if (name == "pwr coulomb") {
      const std::vector<std::string> words = detail::tokens(value);
      if (words.empty() || words[0].empty() || words[0].size() > 12) return;
      long long parsed = 0;
      for (char c : words[0]) { if (c < '0' || c > '9') return; parsed = parsed * 10 + (c - '0'); }
      coulomb_mas = parsed;
      return;
    }
    if (name == "specification") {
      const std::size_t slash = value.find('/');
      if (slash != std::string::npos && value.size() > slash + 3 && detail::integer(value.substr(slash + 1, value.size() - slash - 3), n) && lower(value.substr(value.size() - 2)) == "ah" && n > 0 && n <= 100000) specified_ah = static_cast<double>(n);
      return;
    }
    if (name == "device name" || name == "devicename") { if (!value.empty() && value.size() <= 24) { module.model = value; ++found; } }
    else if (name == "remain capacity" || name == "remaining capacity") { if (amp_hours(value, module.capacity_ah)) ++found; }
    else if (name == "total capacity" || name == "full capacity" || name == "capacity") { if (amp_hours(value, module.full_capacity_ah)) ++found; }
    else if (name == "cycle times" || name == "cycle" || name == "cycles") { const std::vector<std::string> t = detail::tokens(value); if (!t.empty() && detail::integer(t[0], n) && n >= 0 && n <= 1000000) { module.cycles = static_cast<int>(n); ++found; } }
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  if (specified_ah > 0 && module.full_capacity_ah < 0) { module.full_capacity_ah = specified_ah; ++found; }
  if (coulomb_mas > 0 && module.full_capacity_ah > 0) {   // health: what the battery holds now against what it was made to hold
    const double percent = static_cast<double>(coulomb_mas) / (module.full_capacity_ah * 3600000.0) * 100.0;
    module.health_percent = percent >= 100.0 ? 100 : static_cast<int>(percent + 0.5);
    ++found;
  }
  return found;
}

// The event of the text a `data event <item>` printed (the header part before the per-cell table); false when it has no item index or no time.
inline bool parse_event(const std::string& text, Event& out) {
  Event e;
  std::string line;
  bool in_item = false, done = false;
  const auto trim = [](std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
  };
  const auto number = [&](const std::string& value, double scale, double& into) {
    const std::vector<std::string> words = detail::tokens(value);
    long n = 0;
    if (words.empty() || !detail::integer(words[0], n)) return false;
    into = static_cast<double>(n) * scale;
    return true;
  };
  const auto handle = [&](const std::string& row) {
    if (done) return;
    if (row.find("-------") != std::string::npos) { if (in_item) done = true; return; }
    const std::size_t colon = row.find(':');
    if (colon == std::string::npos) return;
    const std::string name = trim(row.substr(0, colon));
    const std::string value = trim(row.substr(colon + 1));
    if (name == "Item Index") { long n = 0; const std::vector<std::string> w = detail::tokens(value); if (!w.empty() && detail::integer(w[0], n) && n >= 0) { e.index = static_cast<int>(n); in_item = true; } return; }
    if (!in_item) return;
    double v = 0;
    const auto word = [&](std::string& into) {   // as printed, with one space between the words ("0x4        BVNOR" is "0x4 BVNOR")
      const std::vector<std::string> words = detail::tokens(value);
      std::string joined;
      for (const std::string& w : words) joined += (joined.empty() ? "" : " ") + w;
      if (!joined.empty() && joined.size() <= 40) into = joined;
    };
    if (name == "Time") word(e.time);
    else if (name == "Voltage") { if (number(value, 0.001, v)) e.voltage_v = v; }
    else if (name == "Current") { if (number(value, 0.001, v)) e.current_a = v; }
    else if (name == "Temperature") { if (number(value, 0.001, v)) e.temperature_c = v; }
    else if (name == "Percent") { if (number(value, 1, v) && v >= 0 && v <= 100) e.percent = static_cast<int>(v); }
    else if (name == "Base State") word(e.base_state);
    else if (name == "Volt. State") word(e.voltage_state);
    else if (name == "Curr. State") word(e.current_state);
    else if (name == "Tempr. State") word(e.temperature_state);
    else if (name == "Coul. Status") word(e.coulomb_state);
    else if (name == "Power Events") word(e.power_events);
    else if (name == "Bat Events") word(e.battery_events);
    else if (name == "System Fault") word(e.system_fault);
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  e.valid = in_item && !e.time.empty();
  if (e.valid) out = e;
  return e.valid;
}

// The health of each cell from the text a `soh <module>` command printed: rows of cell, voltage (mV), SOH count and status (Normal, Abnormal ...). Fills soh_counts in cell order and
// soh_abnormal with the cells whose status is not "Normal"; returns how many cells were read (0 when the text has no such table).
inline std::size_t parse_soh(const std::string& text, Module& module) {
  std::vector<int> counts, abnormal;
  std::string line;
  long first_index = 0;
  bool bad = false;
  const auto handle = [&](const std::string& row) {
    std::vector<std::string> t = detail::tokens(row);
    t.erase(std::remove(t.begin(), t.end(), std::string("-")), t.end());
    long index = 0, millivolts = 0, count = 0;
    if (t.size() < 4 || !detail::integer(t[0], index) || index < 0 || index > 31 || !detail::integer(t[1], millivolts) || !detail::integer(t[2], count)) return;
    if (counts.empty()) first_index = index;   // the table starts at cell 0 (or 1 on some firmwares) and goes on in order
    if (index != first_index + static_cast<long>(counts.size())) { bad = true; return; }
    if (millivolts < 0 || millivolts > 10000 || count < 0 || count > 1000000) { bad = true; return; }
    if (t[3] != "Normal") abnormal.push_back(static_cast<int>(counts.size()));
    counts.push_back(static_cast<int>(count));
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  if (bad) return 0;
  module.soh_counts = counts;
  module.soh_abnormal = abnormal;
  return counts.size();
}

// A whole snapshot of a stack in one answer: the text of `getpwr` ("value#" for each field). The first line is the pack's voltage (mV), current (mA), temperature (m°C), the smallest
// cell charge (mAH) and the four states; then one line per cell (voltage, temperature, two states); then "0#" and "1#". False when the first line is not that.
struct PowerSnapshot {
  double voltage_v = 0, current_a = 0, temperature_c = 0, lowest_cell_ah = 0;   // the fourth field is the smallest charge of a cell in mAH (it equals the weakest cell's charge in `bat`)
  std::string base_state, voltage_state, current_state, temperature_state;
  std::vector<double> cells_v;
  std::vector<double> cell_temperatures_c;
  std::vector<std::string> cell_states;     // the two states of each cell, joined with a space
};
inline bool parse_getpwr(const std::string& text, PowerSnapshot& out) {
  PowerSnapshot snapshot;
  bool first = true, bad = false;
  std::string line;
  const auto fields = [](const std::string& row) {
    std::vector<std::string> values;
    std::string current;
    for (char c : row) {
      if (c == '#') { values.push_back(current); current.clear(); }
      else if (c != ' ' && c != '\t' && c != '\r') current += c;
      else if (!current.empty() && c == ' ') { /* a space inside a field is not expected: dropped */ }
    }
    if (!current.empty()) values.push_back(current);
    return values;
  };
  const auto handle = [&](const std::string& row) {
    if (bad || row.find('#') == std::string::npos) return;
    const std::vector<std::string> v = fields(row);
    long a = 0, b = 0, c = 0, d = 0;
    if (first) {
      if (v.size() < 8 || !detail::integer(v[0], a) || !detail::integer(v[1], b) || !detail::integer(v[2], c) || !detail::integer(v[3], d)) { bad = true; return; }
      snapshot.voltage_v = a / 1000.0; snapshot.current_a = b / 1000.0; snapshot.temperature_c = c / 1000.0; snapshot.lowest_cell_ah = d / 1000.0;
      snapshot.base_state = v[4]; snapshot.voltage_state = v[5]; snapshot.current_state = v[6]; snapshot.temperature_state = v[7];
      first = false;
      return;
    }
    if (v.size() >= 4 && detail::integer(v[0], a) && detail::integer(v[1], b) && a > 100 && a <= 10000) {
      snapshot.cells_v.push_back(a / 1000.0);
      snapshot.cell_temperatures_c.push_back(b / 1000.0);
      snapshot.cell_states.push_back(v[2] + " " + v[3]);
    }
  };
  for (char ch : text) { if (ch == '\n') { handle(line); line.clear(); } else line += ch; }
  handle(line);
  if (bad || first || snapshot.cells_v.size() > 32) return false;
  out = snapshot;
  return true;
}

// One row of a `datalist` table (the BMS's own log in a table): an entry of the event log, or a point of the history (one every 1800 s).
struct DataRow {
  int item = 0;
  std::string date, time;                    // "25-10-28" and "16:27:27" as the battery prints them (its own clock)
  double voltage_v = 0, current_a = 0, temperature_c = 0, temperature_low_c = 0, temperature_high_c = 0, cell_low_v = 0, cell_high_v = 0;
  std::string base_state, voltage_state, current_state, temperature_state;
  int percent = -1;
  std::string events;                        // the mnemonics of an event row as printed (BHV, BOV, IDLE ...); empty when there are none
  std::string error_code, mosfet_state;      // a history row: ErrCode (0x0) and the MOSFETs' state
  bool history = false;
};
struct DataList {
  std::vector<DataRow> rows;
  int save_every_s = -1;                     // "Save data every 1800 S" of the history; -1 when the table is not a history
};

// The rows of the text a `datalist event|history|misc <item>` printed. The console cuts a long table with "Press [Enter] to be continued" and the next rows come after blanks: those
// lines are skipped like any other that is not a row. Event rows and history rows are told apart by the 14th column (a percentage, or ErrCode "0x..."). Returns how many rows were read
// (at most 512); a row with a number that is out of range is skipped.
inline std::size_t parse_datalist(const std::string& text, DataList& out) {
  DataList list;
  std::string line;
  const auto handle = [&](const std::string& row) {
    const std::vector<std::string> t = detail::tokens(row);
    if (t.size() >= 5 && t[0] == "Save" && t[1] == "data" && t[2] == "every") { long n = 0; if (detail::integer(t[3], n) && n > 0 && n <= 86400) list.save_every_s = static_cast<int>(n); return; }
    long item = 0;
    if (t.size() < 15 || list.rows.size() >= 512 || !detail::integer(t[0], item) || item < 0 || t[1].size() != 8 || t[1][2] != '-' || t[1][5] != '-' || t[2].size() != 8 || t[2][2] != ':') return;
    long n[7] = {};
    for (std::size_t i = 0; i < 7; ++i) if (!detail::integer(t[3 + i], n[i])) return;
    DataRow r;
    r.item = static_cast<int>(item);
    r.date = t[1]; r.time = t[2];
    r.voltage_v = n[0] / 1000.0; r.current_a = n[1] / 1000.0; r.temperature_c = n[2] / 1000.0; r.temperature_low_c = n[3] / 1000.0; r.temperature_high_c = n[4] / 1000.0;
    r.cell_low_v = n[5] / 1000.0; r.cell_high_v = n[6] / 1000.0;
    if (r.cell_low_v < 0 || r.cell_low_v > 10 || r.cell_high_v < 0 || r.cell_high_v > 10) return;
    r.base_state = t[10]; r.voltage_state = t[11]; r.current_state = t[12]; r.temperature_state = t[13];
    const auto percent_of = [](const std::string& s, int& into) { long p = 0; if (s.size() > 1 && s.back() == '%' && detail::integer(s.substr(0, s.size() - 1), p) && p >= 0 && p <= 100) into = static_cast<int>(p); };
    if (t[14].compare(0, 2, "0x") == 0) {   // a history row: ErrCode, the MOSFETs' state, the charge, then the columns of each module
      r.history = true;
      r.error_code = t[14];
      if (t.size() > 15) r.mosfet_state = t[15];
      if (t.size() > 16) percent_of(t[16], r.percent);
    } else {
      percent_of(t[14], r.percent);
      for (std::size_t i = 15; i < t.size(); ++i) r.events += (r.events.empty() ? "" : " ") + t[i];
      if (r.events.size() > 40) r.events.resize(40);
    }
    list.rows.push_back(r);
  };
  for (char c : text) { if (c == '\n') { handle(line); line.clear(); } else line += c; }
  handle(line);
  out = list;
  return list.rows.size();
}

// Puts the cells of `bat <number>` on the module of that number (in the list `parse_pwr` made); false when the module is not in the list or not present.
inline bool attach_cells(std::vector<Module>& modules, int number, const std::vector<double>& cells) {
  for (Module& m : modules) if (m.number == number && m.present) { m.cells_v = cells; return true; }
  return false;
}

// The console ends every answer, a good one or a refusal ("Invalid command or fail to excute."), with a line of `$$` and its prompt (`pylon>` or `pylon_debug>`): the answer is
// whole when the text ends in that prompt. Waiting for it also leaves nothing of this answer to be taken for the next one, and a command the battery does not know is over at once
// instead of after the timeout.
inline bool console_done(const std::string& text) {
  const std::size_t end = text.find_last_not_of(" \r\n\t");
  if (end == std::string::npos || text[end] != '>') return false;
  const std::size_t marker = text.rfind("$$");
  return marker != std::string::npos && marker < end;
}

// The whole stack in a few numbers: the modules that are present, their mean voltage, the total current, the temperature range and the mean state of charge.
struct Stack {
  int modules = 0;
  double voltage_v = 0, current_a = 0, temperature_min_c = 0, temperature_max_c = 0;
  int soc_percent = -1;
  double cell_low_v = 0, cell_high_v = 0;
  double capacity_ah = -1, full_capacity_ah = -1, energy_kwh = -1;   // the modules' add up (they are in parallel); -1 when no module said
  int cycles = -1;                    // of the most used module
  int health_percent = -1;            // the mean of the modules that know theirs
  std::string model;                  // the first module's
  std::string state;                  // "charging", "discharging", "idle"
  bool alarm = false;                 // any state column that is not Normal, Idle, Charge or Dischg
  bool has_power = false; double power_w = 0;   // the sum of the power the modules measured themselves
  int balancing = -1;                 // cells being balanced in all (-1 when no module says)
  bool protecting = false;            // any module is protecting
  int charge_mos = -1, discharge_mos = -1;   // the first module's that says
};

inline Stack summarise(const std::vector<Module>& modules) {
  Stack s;
  double voltage = 0, soc = 0, current = 0;
  int with_soc = 0, with_health = 0;
  double health = 0;
  bool first = true;
  for (const Module& m : modules) {
    if (!m.present) continue;
    ++s.modules;
    voltage += m.voltage_v;
    current += m.current_a;
    if (m.soc_percent >= 0) { soc += m.soc_percent; ++with_soc; }
    double cell_low = m.cell_low_v, cell_high = m.cell_high_v;
    if (!m.cells_v.empty()) {   // the cells' own extremes are truer than the module's Vlow and Vhigh columns
      cell_low = m.cells_v[0]; cell_high = m.cells_v[0];
      for (double v : m.cells_v) { if (v < cell_low) cell_low = v; if (v > cell_high) cell_high = v; }
    }
    if (first) { s.temperature_min_c = m.temperature_low_c; s.temperature_max_c = m.temperature_high_c; s.cell_low_v = cell_low; s.cell_high_v = cell_high; first = false; }
    else {
      if (m.temperature_low_c < s.temperature_min_c) s.temperature_min_c = m.temperature_low_c;
      if (m.temperature_high_c > s.temperature_max_c) s.temperature_max_c = m.temperature_high_c;
      if (cell_low < s.cell_low_v) s.cell_low_v = cell_low;
      if (cell_high > s.cell_high_v) s.cell_high_v = cell_high;
    }
    if (m.capacity_ah >= 0) s.capacity_ah = (s.capacity_ah < 0 ? 0 : s.capacity_ah) + m.capacity_ah;
    if (m.full_capacity_ah >= 0) s.full_capacity_ah = (s.full_capacity_ah < 0 ? 0 : s.full_capacity_ah) + m.full_capacity_ah;
    if (m.cycles > s.cycles) s.cycles = m.cycles;
    if (m.health_percent >= 0) { health += m.health_percent; ++with_health; }
    if (s.model.empty()) s.model = m.model;
    if (m.has_power) { s.has_power = true; s.power_w += m.power_w; }
    if (m.balancing >= 0) s.balancing = (s.balancing < 0 ? 0 : s.balancing) + m.balancing;
    if (m.protecting) s.protecting = true;
    if (s.charge_mos < 0) s.charge_mos = m.charge_mos;
    if (s.discharge_mos < 0) s.discharge_mos = m.discharge_mos;
    for (const std::string* state : {&m.voltage_state, &m.current_state, &m.temperature_state, &m.cell_voltage_state, &m.cell_temperature_state, &m.mosfet_temperature_state, &m.system_alarm_state})
      if (*state != "Normal" && !state->empty()) s.alarm = true;
  }
  if (s.modules > 0) {
    s.voltage_v = voltage / s.modules;   // the modules of a stack are in parallel: the same voltage, so the mean is what the bus has
    s.current_a = current;               // and their currents add
    if (s.capacity_ah >= 0) s.energy_kwh = s.capacity_ah * s.voltage_v / 1000.0;
    if (with_soc > 0) s.soc_percent = static_cast<int>(soc / with_soc + 0.5);
    if (with_health > 0) s.health_percent = static_cast<int>(health / with_health + 0.5);
    s.state = s.current_a > 0.5 ? "charging" : s.current_a < -0.5 ? "discharging" : "idle";
  }
  return s;
}

// ---- the RS485 frame -----------------------------------------------------------------------------------------------------------------

inline char hex_digit(unsigned value) { return "0123456789ABCDEF"[value & 0xF]; }
inline std::string hex(unsigned value, int digits) {
  std::string out;
  for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) out += hex_digit(value >> shift);
  return out;
}

// The four hexadecimal characters of LENGTH for an INFO of `info_length` characters: the check nibble and the 12-bit length.
inline std::string length_field(std::size_t info_length) {
  const unsigned len = static_cast<unsigned>(info_length) & 0xFFF;
  const unsigned sum = (len & 0xF) + ((len >> 4) & 0xF) + ((len >> 8) & 0xF);
  const unsigned check = ((~sum) + 1) & 0xF;
  return hex(check, 1) + hex(len, 3);
}

// The checksum characters for the text from VER to the end of INFO.
inline std::string frame_checksum(std::string_view body) {
  unsigned sum = 0;
  for (char c : body) sum += static_cast<unsigned char>(c);
  return hex(((~sum) + 1) & 0xFFFF, 4);
}

// A frame: '~' VER ADR CID1 CID2 LENGTH INFO CHKSUM CR. `info` is the hexadecimal text of INFO (may be empty).
inline std::string build_frame(unsigned version, unsigned address, unsigned cid1, unsigned cid2, std::string_view info) {
  std::string body = hex(version, 2) + hex(address, 2) + hex(cid1, 2) + hex(cid2, 2) + length_field(info.size()) + std::string(info);
  return "~" + body + frame_checksum(body) + "\r";
}

struct Frame {
  unsigned version = 0, address = 0, cid1 = 0, cid2 = 0;   // cid2 of a reply is its return code
  std::string info;
};

inline bool parse_frame(std::string_view text, Frame& out) {
  if (text.size() < 18 || text.front() != '~' || text.back() != '\r') return false;
  const std::string_view body = text.substr(1, text.size() - 6);   // between '~' and the four checksum characters and CR
  for (char c : text.substr(1, text.size() - 2)) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  if (frame_checksum(body) != text.substr(text.size() - 5, 4)) return false;
  const auto number = [&](std::size_t at, std::size_t digits) {
    unsigned value = 0;
    for (std::size_t i = 0; i < digits; ++i) { const char c = body[at + i]; value = value * 16 + (c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'A' + 10)); }
    return value;
  };
  const std::size_t info_length = number(9, 3);
  if (body.size() != 12 + info_length || length_field(info_length) != body.substr(8, 4)) return false;
  out.version = number(0, 2); out.address = number(2, 2); out.cid1 = number(4, 2); out.cid2 = number(6, 2);
  out.info = std::string(body.substr(12));
  return true;
}

}  // namespace armor::solar::pylontech
