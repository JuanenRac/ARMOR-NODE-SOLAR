// ARMOR-NODE-SOLAR - the PI18 serial protocol of Voltronic / MPP Solar hybrid inverters (InfiniSolar V, LV5048 Hybrid, SunGoldPower, Axioma and the many clones that answer ^P005GS).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Source: the protocols as implemented by the mpp-solar project (MIT) and the esphome-pipsolar project (Apache 2.0), with the sample replies they publish as the test vectors.
// NOTHING here has been connected to an inverter.
//
//   Serial 2400 baud 8N1 on the RS232 port, like the older protocol, but with other frames:
//   Request:  ^P<three digits: the command's length + 3><command> <CRC-16, two bytes> <CR>            (^P005GS, ^P006MOD, ^P005FWS ...)
//   Reply:    ^D<three digits: the payload's length + 3><payload: numbers separated by commas> <CRC-16> <CR>;  ^0 <CRC> <CR> refuses, ^1 <CRC> <CR> accepts
//   CRC-16:   the same as the older protocol (XMODEM, with the 0x28 / 0x0D / 0x0A rule), over every byte before it.
// Only reading commands are built (^P...): the setting frames (^S...) are not built anywhere in this project.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "voltronic.hpp"

namespace armor::solar::voltronic::pi18 {

// The commands that read, and nothing else: the general status, the working mode, the faults and warnings, the ratings, the serial number and the firmware versions.
inline bool allowed(std::string_view command) {
  for (const char* known : {"GS", "MOD", "FWS", "PIRI", "ID", "VFW", "PI", "FLAG", "T"}) if (command == known) return true;
  return false;
}

inline bool build_command(std::string_view command, std::vector<std::uint8_t>& out) {
  out.clear();
  if (!allowed(command)) return false;
  const std::size_t length = command.size() + 3;
  const std::string head = std::string("^P") + static_cast<char>('0' + (length / 100) % 10) + static_cast<char>('0' + (length / 10) % 10) + static_cast<char>('0' + length % 10);
  out.assign(head.begin(), head.end());
  out.insert(out.end(), command.begin(), command.end());
  const std::array<std::uint8_t, 2> crc = wire_crc(out.data(), out.size());
  out.push_back(crc[0]);
  out.push_back(crc[1]);
  out.push_back('\r');
  return true;
}

enum class Kind { kData, kAck, kNak };

// A reply checked and cut open: its kind and, for data, the payload text between the header and the CRC. False when it is not a whole reply, its CRC is wrong, or the length it
// declares is not the length it has.
inline bool parse_reply(const std::uint8_t* frame, std::size_t length, Kind& kind, std::string& payload) {
  payload.clear();
  if (length < 5 || frame[0] != '^' || frame[length - 1] != '\r') return false;
  const std::size_t body = length - 3;
  const std::array<std::uint8_t, 2> crc = wire_crc(frame, body);
  if (frame[body] != crc[0] || frame[body + 1] != crc[1]) return false;
  if (frame[1] == '0' && length == 5) { kind = Kind::kNak; return true; }
  if (frame[1] == '1' && length == 5) { kind = Kind::kAck; return true; }
  if (frame[1] != 'D' || length < 8) return false;
  for (std::size_t i = 2; i < 5; ++i) if (frame[i] < '0' || frame[i] > '9') return false;
  const std::size_t declared = static_cast<std::size_t>(frame[2] - '0') * 100 + static_cast<std::size_t>(frame[3] - '0') * 10 + static_cast<std::size_t>(frame[4] - '0');
  if (declared != body - 5 + 3) return false;
  kind = Kind::kData;
  payload.assign(reinterpret_cast<const char*>(frame) + 5, body - 5);
  return true;
}

// Finds replies in a byte stream: from '^' to CR. A '^' inside a frame starts over.
class ReplyFramer {
 public:
  bool feed(std::uint8_t byte, std::vector<std::uint8_t>& frame) {
    if (byte == '^') { buffer_.clear(); buffer_.push_back(byte); return false; }
    if (buffer_.empty()) return false;
    if (buffer_.size() >= kMaxReply) { buffer_.clear(); return false; }
    buffer_.push_back(byte);
    if (byte != '\r') return false;
    frame = buffer_;
    buffer_.clear();
    return true;
  }
  void clear() { buffer_.clear(); }
 private:
  static constexpr std::size_t kMaxReply = 320;
  std::vector<std::uint8_t> buffer_;
};

inline std::vector<std::string> split_commas(const std::string& text) {
  std::vector<std::string> fields;
  std::string current;
  for (char c : text) { if (c == ',') { fields.push_back(current); current.clear(); } else current += c; }
  fields.push_back(current);
  return fields;
}

// ---- GS: the general status --------------------------------------------------------------------------------------------------------

// The 28 fields (two of them optional on the older units): grid V x10 and Hz x10, output V x10 and Hz x10, output VA, W and load %, battery V x10, battery V from the charger
// x10 (two chargers), discharge and charge current, capacity %, the inverter's and the two chargers' temperatures, the two PV inputs' power and voltage x10, then seven small
// codes: configuration changed, charger 1 and 2 status (0 abnormal, 1 normal not charging, 2 charging), load connected, battery power direction (0 none, 1 charging, 2 discharging),
// DC/AC direction (0 none, 1 AC to DC, 2 DC to AC), line power direction (0 none, 1 input, 2 output), and the parallel id.
inline bool parse_gs(const std::string& payload, Status& out) {
  const std::vector<std::string> f = split_commas(payload);
  if (f.size() < 27) return false;
  double v[28] = {};
  // the three temperatures (fields 13 to 15: the inverter's and the two chargers') may be below zero in a shed in winter; every other field is a size, a power or a code
  for (std::size_t i = 0; i < f.size() && i < 28; ++i) if (!detail::number(f[i], v[i]) || (v[i] < 0 && !(i >= 13 && i <= 15))) return false;
  Status s;
  s.grid_v = v[0] / 10; s.grid_hz = v[1] / 10; s.out_v = v[2] / 10; s.out_hz = v[3] / 10;
  s.out_va = v[4]; s.out_w = v[5]; s.load_percent = v[6];
  s.battery_v = v[7] / 10; s.battery_v_scc = v[8] / 10;
  s.battery_discharge_a = v[10]; s.battery_charge_a = v[11]; s.battery_percent = v[12];
  s.heatsink_c = v[13];
  const double pv1_w = v[16], pv2_w = v[17], pv1_v = v[18] / 10, pv2_v = v[19] / 10;
  s.pv_w = pv1_w + pv2_w; s.has_pv_w = true;
  s.pv_v = pv1_w >= pv2_w ? pv1_v : pv2_v;                     // the input that works harder is the one shown
  s.pv_a = s.pv_v > 1.0 ? s.pv_w / s.pv_v : 0.0;              // the protocol gives no PV current: the power over the voltage
  s.load_on = v[23] == 1;
  s.scc_charging = v[21] == 2 || v[22] == 2;
  s.charging = v[24] == 1;
  s.ac_charging = v[25] == 1;
  s.config_changed = v[20] == 1;
  hold_limits(s);   // an absurd load or power is held at its limit, as for the other dialects, so the message still goes out
  out = s;
  return true;
}

// ---- MOD: the mode -----------------------------------------------------------------------------------------------------------------

// 00 power on, 01 standby, 02 bypass, 03 battery, 04 fault, 05 hybrid (line). Mapped to the letters of the older protocol; bypass is the line passing through, so it is 'L'.
inline bool parse_mod(const std::string& payload, char& mode) {
  if (payload.size() != 2 || payload[0] != '0' || payload[1] < '0' || payload[1] > '5') return false;
  static const char kLetters[6] = {'P', 'S', 'L', 'B', 'F', 'L'};
  mode = kLetters[payload[1] - '0'];
  return true;
}

// ---- FWS: the faults and warnings ----------------------------------------------------------------------------------------------------

// A fault code (0: none), then one flag per warning: line fail, output circuit short, inverter over temperature, fan lock, battery voltage high, battery low, battery under,
// overload, EEPROM fail, power limit, PV1 and PV2 voltage high, the two MPPT overload warnings, battery too low to charge for each charger. A fault code makes `inverter_fault`
// (and `fault_code_<n>`). Extra fields are ignored (some units send more).
inline bool parse_fws(const std::string& payload, std::vector<std::string>& active) {
  active.clear();
  const std::vector<std::string> f = split_commas(payload);
  if (f.size() < 17) return false;
  double v[24] = {};
  for (std::size_t i = 0; i < f.size() && i < 24; ++i) if (!detail::number(f[i], v[i]) || v[i] < 0) return false;
  if (v[0] > 0) { active.push_back("inverter_fault"); active.push_back("fault_code_" + std::to_string(static_cast<int>(v[0]))); }
  static const char* const kNames[16] = {"line_fail", "output_short", "over_temperature", "fan_locked", "battery_voltage_high", "battery_low", "battery_under_shutdown", "overload",
                                         "eeprom_fault", "power_limit", "pv_voltage_high", "pv2_voltage_high", "mppt_overload_warning", "mppt2_overload_warning",
                                         "battery_too_low_to_charge", "battery_too_low_to_charge_scc2"};
  for (std::size_t i = 0; i < 16; ++i) if (v[1 + i] >= 1) active.push_back(kNames[i]);
  return true;
}

}  // namespace armor::solar::voltronic::pi18

// ---- PI17: Infini-Solar 5 kW, LV 6 kW, WP and TWIN ----------------------------------------------------------------------------------------
//   Request:  ^P<three digits: the command's length + 1><command><CR>, with NO CRC (^P003GS, ^P003PS, ^P004MOD, ^P003WS)
//   Reply:    the same frames as PI18 (pi18::parse_reply and pi18::ReplyFramer)
// Written from the makers' two documents (the 5 kW one and the LV 6K / WP / TWIN one). Their templates and their field lists do not agree everywhere: the field lists are followed.
// NOTHING here has been connected to an inverter. Only reading commands are built.
namespace armor::solar::voltronic::pi17 {

inline bool build_command(std::string_view command, std::vector<std::uint8_t>& out) {
  out.clear();
  bool known = false;
  for (const char* name : {"GS", "PS", "MOD", "WS", "PI", "ID", "VFW"}) known = known || command == name;
  if (!known) return false;
  const std::size_t length = command.size() + 1;
  const std::string head = std::string("^P00") + static_cast<char>('0' + length);
  out.assign(head.begin(), head.end());
  out.insert(out.end(), command.begin(), command.end());
  out.push_back('\r');
  return true;
}

// The comma separated numbers of a payload; an empty field (reserved) is 0. False when one is not a number.
inline bool numbers(const std::string& payload, std::vector<double>& out) {
  out.clear();
  const std::vector<std::string> f = pi18::split_commas(payload);
  if (f.size() > 40) return false;
  for (const std::string& field : f) {
    double value = 0;
    if (!field.empty() && !detail::number(field, value)) return false;
    out.push_back(value);
  }
  return true;
}

// GS: PV voltages 1 and 2 (x0.1 V), PV currents, battery V x10, capacity %, battery current x10 (+ charging), grid V R S T x10, grid Hz x100, grid currents, output V R S T x10,
// output Hz x100, output currents, inner temperature, highest component temperature, ...
inline bool parse_gs(const std::string& payload, Status& out) {
  std::vector<double> v;
  if (!numbers(payload, v) || v.size() < 22) return false;
  const std::vector<std::string> f = pi18::split_commas(payload);
  if (f[4].empty() || f[5].empty()) return false;
  Status s = out;
  s.battery_v = v[4] / 10;
  s.battery_percent = v[5];
  s.battery_charge_a = v[6] > 0 ? v[6] / 10 : 0;
  s.battery_discharge_a = v[6] < 0 ? -v[6] / 10 : 0;
  s.grid_v = v[7] / 10; s.grid_hz = v[10] / 100;
  s.out_v = v[14] / 10; s.out_hz = v[17] / 100;
  s.heatsink_c = v.size() > 22 ? v[22] : v[21];
  s.pv_v = (v[1] > v[0] ? v[1] : v[0]) / 10;
  hold_limits(s);
  out = s;
  return true;
}

// PS: PV powers 1 and 2 (W), the battery power or nothing, the grid's active power R S T and total, the output's active power R S T and total, apparent power R S T and total,
// load %, output connected, PV 1 and 2 working, battery direction (1 charging, 2 discharging), DC/AC direction (1 AC to DC), line direction. It completes what GS filled.
inline bool parse_ps(const std::string& payload, Status& out) {
  std::vector<double> v;
  if (!numbers(payload, v) || v.size() < 22) return false;
  const std::vector<std::string> f = pi18::split_commas(payload);
  Status s = out;
  s.pv_w = v[0] + v[1]; s.has_pv_w = true;
  s.pv_a = s.pv_v > 1.0 ? s.pv_w / s.pv_v : 0.0;
  s.out_w = f[10].empty() ? v[7] : v[10];
  s.out_va = f[14].empty() ? v[11] : v[14];
  s.load_percent = v[15];
  s.load_on = v[16] == 1;
  s.charging = v[19] == 1;
  s.ac_charging = s.charging && v[20] == 1;
  s.scc_charging = s.charging && s.pv_w > 0;
  hold_limits(s);
  out = s;
  return true;
}

// MOD: 00 power on, 01 standby, 02 bypass, 03 battery, 04 fault, 05 hybrid (line, grid), 06 charge.
inline bool parse_mod(const std::string& payload, char& mode) {
  if (payload.size() != 2 || payload[0] != '0' || payload[1] < '0' || payload[1] > '6') return false;
  static const char kLetters[7] = {'P', 'S', 'L', 'B', 'F', 'L', 'L'};
  mode = kLetters[payload[1] - '0'];
  return true;
}

// WS: one 0/1 flag per warning, A to V.
inline bool parse_ws(const std::string& payload, std::vector<std::string>& active) {
  static const char* const kNames[22] = {"pv1_loss", "pv2_loss", "pv_voltage_high", "pv2_voltage_high", "battery_under", "battery_low", "battery_open", "battery_voltage_high",
                                         "battery_low_hybrid", "grid_voltage_high", "grid_voltage_low", "grid_frequency_high", "grid_frequency_low", "grid_average_voltage_high",
                                         "grid_voltage_loss", "grid_frequency_loss", "islanding", "grid_phase_error", "over_temperature", "overload", "emergency_power_off",
                                         "grid_wave_loss"};
  active.clear();
  std::vector<double> v;
  if (!numbers(payload, v) || v.size() < 20) return false;
  for (std::size_t i = 0; i < 22 && i < v.size(); ++i) if (v[i] >= 1) active.push_back(kNames[i]);
  return true;
}

}  // namespace armor::solar::voltronic::pi17
