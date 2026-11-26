// ARMOR-NODE-SOLAR - Victron VE.Direct, text mode: the equipment talks on its own once a second, the node only listens.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// 19200 baud 8N1. A block is lines of "\r\n<label>\t<value>" and ends with the line "Checksum\t<one byte>": the sum of every byte of the block, that one included, is zero modulo
// 256. A frame of the hexadecimal mode (':' to the end of the line) may arrive in between and is skipped. From the public "VE.Direct protocol" document; none has been connected.
//
// Three uses of the text mode: "monitor" (a BMV or SmartShunt: the battery's voltage, current, state of charge and temperature), "mppt" (a SmartSolar / BlueSolar charger: the
// panel and the battery side) and "inverter" (a Phoenix inverter: the AC output; the document gives its apparent power only, which is told as the watts too). "monitor-hex" is a
// second driver for a monitor, in the hexadecimal mode: the node asks for each value (it needs the port's transmit line wired too). The VE.Direct port is a 5 V (or 3.3 V) TTL level on a JST-PH connector, not RS232: it needs its own wiring (see docs/NODE_HARDWARE.md).
#pragma once
#include <array>
#include <cstring>

#include "equipment_driver.hpp"

namespace armor::solar::drivers {

enum class VedirectUse : std::uint8_t { kMonitor, kCharger, kInverter };

class VedirectDriver final : public Driver {
 public:
  explicit VedirectDriver(VedirectUse use) : use_(use) {}
  explicit VedirectDriver(bool charger) : use_(charger ? VedirectUse::kCharger : VedirectUse::kMonitor) {}
  bool inverter() const override { return use_ != VedirectUse::kMonitor; }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    command_ = "listening";
    reset();
    return {};
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>&) override {
    if (hex_) { if (byte == '\n') hex_ = false; return Rx::kMore; }
    if (state_ == State::kChecksum) {
      sum_ = static_cast<std::uint8_t>(sum_ + byte);
      const bool good = sum_ == 0;
      const Rx result = good ? finish() : Rx::kMore;   // a block that does not add up (the first one heard from its middle) is waited out
      reset();
      return result;
    }
    if (byte == ':' && state_ == State::kLabel && label_length_ == 0) { hex_ = true; return Rx::kMore; }
    sum_ = static_cast<std::uint8_t>(sum_ + byte);
    if (byte == '\r') { end_line(); return Rx::kMore; }
    if (byte == '\n') { end_line(); return Rx::kMore; }
    if (state_ == State::kLabel) {
      if (byte == '\t') {
        label_[label_length_] = '\0';
        state_ = std::strcmp(label_.data(), "Checksum") == 0 ? State::kChecksum : State::kValue;
      } else if (label_length_ + 1 < label_.size()) {
        label_[label_length_++] = static_cast<char>(byte);
      }
    } else if (value_length_ + 1 < value_.size()) {
      value_[value_length_++] = static_cast<char>(byte);
    }
    return Rx::kMore;
  }

  Rx timeout(std::vector<std::uint8_t>&) override { reset(); return bad("timeout"); }

 private:
  enum class State { kLabel, kValue, kChecksum };
  struct Fields {
    bool v = false, i = false, vpv = false, ppv = false, soc = false, t = false, cs = false, err = false, alarm = false, ce = false;
    long v_mv = 0, i_ma = 0, vpv_mv = 0, ppv_w = 0, soc_permille = 0, t_c = 0, cs_value = 0, err_value = 0;
    bool alarm_on = false;
    bool ac_v = false, ac_i = false, ac_s = false, mode = false;
    long ac_v_cv = 0, ac_i_da = 0, ac_s_va = 0, mode_value = 0, alarm_bits = 0, warn_bits = 0;
  };

  static bool number(const char* text, long& out) {
    if (*text == '\0') return false;
    const bool negative = *text == '-';
    if (negative) ++text;
    long value = 0;
    for (; *text != '\0'; ++text) {
      if (*text < '0' || *text > '9' || value > 100000000L) return false;
      value = value * 10 + (*text - '0');
    }
    out = negative ? -value : value;
    return true;
  }

  void end_line() {
    if (state_ == State::kValue) {
      value_[value_length_] = '\0';
      const char* l = label_.data();
      const char* v = value_.data();
      if (std::strcmp(l, "V") == 0) fields_.v = number(v, fields_.v_mv);
      else if (std::strcmp(l, "I") == 0) fields_.i = number(v, fields_.i_ma);
      else if (std::strcmp(l, "VPV") == 0) fields_.vpv = number(v, fields_.vpv_mv);
      else if (std::strcmp(l, "PPV") == 0) fields_.ppv = number(v, fields_.ppv_w);
      else if (std::strcmp(l, "SOC") == 0) fields_.soc = number(v, fields_.soc_permille);
      else if (std::strcmp(l, "T") == 0) fields_.t = number(v, fields_.t_c);
      else if (std::strcmp(l, "CS") == 0) fields_.cs = number(v, fields_.cs_value);
      else if (std::strcmp(l, "ERR") == 0) fields_.err = number(v, fields_.err_value);
      else if (std::strcmp(l, "Alarm") == 0) { fields_.alarm = true; fields_.alarm_on = std::strcmp(v, "ON") == 0; }
      else if (std::strcmp(l, "AC_OUT_V") == 0) fields_.ac_v = number(v, fields_.ac_v_cv);
      else if (std::strcmp(l, "AC_OUT_I") == 0) fields_.ac_i = number(v, fields_.ac_i_da);
      else if (std::strcmp(l, "AC_OUT_S") == 0) fields_.ac_s = number(v, fields_.ac_s_va);
      else if (std::strcmp(l, "MODE") == 0) fields_.mode = number(v, fields_.mode_value);
      else if (std::strcmp(l, "AR") == 0) { if (!number(v, fields_.alarm_bits)) fields_.alarm_bits = 0; }
      else if (std::strcmp(l, "WARN") == 0) { if (!number(v, fields_.warn_bits)) fields_.warn_bits = 0; }
    }
    state_ = State::kLabel;
    label_length_ = value_length_ = 0;
  }

  void reset() {
    fields_ = Fields();
    sum_ = 0;
    state_ = State::kLabel;
    label_length_ = value_length_ = 0;
    hex_ = false;
  }

  // A Phoenix inverter: the AC output, the battery it takes from, its mode and the alarm and warning reasons (one bit each, the same table).
  Rx finish_inverter() {
    if (!fields_.ac_v) return Rx::kMore;
    voltronic::Status& s = out_.status;
    s = voltronic::Status();
    s.battery_v = fields_.v_mv * 0.001;
    s.out_v = fields_.ac_v_cv * 0.01;
    s.out_va = fields_.ac_s ? fields_.ac_s_va : (fields_.ac_i ? s.out_v * fields_.ac_i_da * 0.1 : 0);
    s.out_w = s.out_va;
    const long mode = fields_.mode ? fields_.mode_value : -1;
    out_.mode = mode == 2 ? 'B' : mode == 4 ? 'D' : mode == 5 || mode == 253 ? 'H' : 0;
    s.load_on = mode == 2 || mode == 5;
    static const char* const kReasons[14] = {"battery_low", "battery_voltage_high", "battery_low_soc", "starter_voltage_low", "starter_voltage_high", "low_temperature", "over_temperature",
                                             "mid_voltage", "overload", "dc_ripple", "output_voltage_low", "output_voltage_high", "output_short", "bms_lockout"};
    out_.warnings.clear();
    const long bits = fields_.alarm_bits | fields_.warn_bits;
    for (int bit = 0; bit < 14; ++bit) if (bits & (1L << bit)) out_.warnings.emplace_back(kReasons[bit]);
    out_.warnings_known = true;
    voltronic::hold_limits(s);
    return Rx::kDone;
  }

  Rx finish() {
    if (!fields_.v) return Rx::kMore;   // the history block of a monitor has no voltage: not the one we want
    if (use_ == VedirectUse::kInverter) return finish_inverter();
    if (use_ == VedirectUse::kCharger) {
      if (!fields_.vpv && !fields_.cs) return Rx::kMore;
      voltronic::Status& s = out_.status;
      s = voltronic::Status();
      s.battery_v = fields_.v_mv * 0.001;
      const double amps = fields_.i ? fields_.i_ma * 0.001 : 0;
      s.battery_charge_a = amps > 0 ? amps : 0;
      s.battery_discharge_a = amps < 0 ? -amps : 0;
      s.pv_v = fields_.vpv ? fields_.vpv_mv * 0.001 : 0;
      s.pv_w = fields_.ppv ? fields_.ppv_w : 0;
      s.has_pv_w = true;
      s.pv_a = s.pv_v > 1 ? s.pv_w / s.pv_v : 0;
      // charger state: 2 fault, 3 bulk, 4 absorption, 5 float, 7 equalise
      const long cs = fields_.cs ? fields_.cs_value : -1;
      s.scc_charging = cs == 3 || cs == 4 || cs == 5 || cs == 7;
      s.charging = s.scc_charging;
      out_.mode = cs == 2 ? 'F' : cs == 0 ? 'S' : 0;
      out_.warnings.clear();
      if (fields_.err && fields_.err_value != 0) out_.warnings.push_back("charger_error_" + std::to_string(fields_.err_value < 0 ? 0 : fields_.err_value));
      out_.warnings_known = fields_.err;
      voltronic::hold_limits(s);
    } else {
      pylontech::Module m;
      m.voltage_v = detail::bounded(fields_.v_mv * 0.001, 0, 1000);
      m.current_a = detail::bounded(fields_.i ? fields_.i_ma * 0.001 : 0, -1000, 1000);
      if (fields_.soc) m.soc_percent = static_cast<int>(detail::bounded(fields_.soc_permille * 0.1 + 0.5, 0, 100));
      m.has_temperature = fields_.t;
      if (fields_.t) m.temperature_c = detail::bounded(static_cast<double>(fields_.t_c), -50, 200);
      m.system_alarm_state = fields_.alarm && fields_.alarm_on ? "Alarm" : "Normal";
      detail::finish_module(m);
      out_.modules.assign(1, m);
    }
    return Rx::kDone;
  }

  VedirectUse use_;
  State state_ = State::kLabel;
  bool hex_ = false;
  std::uint8_t sum_ = 0;
  std::array<char, 12> label_{};
  std::array<char, 24> value_{};
  std::size_t label_length_ = 0, value_length_ = 0;
  Fields fields_;
};

// ---- the hexadecimal mode of a battery monitor (BMV, SmartShunt) -----------------------------------------------------------------------
// A frame is ':' <command, one hex digit> <bytes, two digits each> <check> '\n', numbers low byte first, and the command, the bytes and the check add up to 0x55. "Get" (7) asks a
// register: id (two bytes), flags 0. The answer is 7 id flags value. Text blocks keep arriving in between and are passed over. Only reading is built.
class VedirectHexDriver final : public Driver {
 public:
  bool inverter() const override { return false; }

  static std::vector<std::uint8_t> request(std::uint16_t id) {
    const std::uint8_t bytes[3] = {static_cast<std::uint8_t>(id), static_cast<std::uint8_t>(id >> 8), 0};
    const std::uint8_t check = static_cast<std::uint8_t>(0x55 - 7 - bytes[0] - bytes[1] - bytes[2]);
    std::vector<std::uint8_t> out{':', '7'};
    static const char kDigits[] = "0123456789ABCDEF";
    for (const std::uint8_t b : {bytes[0], bytes[1], bytes[2], check}) {
      out.push_back(static_cast<std::uint8_t>(kDigits[b >> 4]));
      out.push_back(static_cast<std::uint8_t>(kDigits[b & 15]));
    }
    out.push_back('\n');
    return out;
  }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    module_ = pylontech::Module();
    module_.has_temperature = false;
    at_ = 0;
    return ask();
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>& next) override {
    if (byte == ':') { length_ = 0; inside_ = true; return Rx::kMore; }
    if (!inside_) return Rx::kMore;
    if (byte != '\n') {
      if (byte == '\r') return Rx::kMore;
      if (length_ >= line_.size()) { inside_ = false; return Rx::kMore; }
      line_[length_++] = static_cast<char>(byte);
      return Rx::kMore;
    }
    inside_ = false;
    // the command digit, then pairs of digits
    if (length_ < 3 || (length_ & 1) == 0 || nibble(line_[0]) != 7) return Rx::kMore;
    std::array<std::uint8_t, 20> data{};
    const std::size_t count = (length_ - 1) / 2;
    unsigned sum = 7;
    for (std::size_t i = 0; i < count; ++i) {
      const int high = nibble(line_[1 + 2 * i]), low = nibble(line_[2 + 2 * i]);
      if (high < 0 || low < 0) return Rx::kMore;
      data[i] = static_cast<std::uint8_t>(high * 16 + low);
      sum += data[i];
    }
    if ((sum & 0xFF) != 0x55) return at_ == 0 ? bad("crc") : advance(next);
    if (count < 5 || static_cast<std::uint16_t>(data[0] | (data[1] << 8)) != kRegisters[at_]) return Rx::kMore;   // another register's answer, or our question heard back
    if (data[2] != 0) return at_ == 0 ? bad("nak") : advance(next);
    const std::size_t size = count - 4;   // id, flags and the check are not the value
    std::uint32_t raw = 0;
    for (std::size_t i = 0; i < size && i < 4; ++i) raw |= static_cast<std::uint32_t>(data[3 + i]) << (8 * i);
    decode(raw, size);
    return advance(next);
  }

  // The voltage is essential; a register the monitor does not have is left out.
  Rx timeout(std::vector<std::uint8_t>& next) override {
    inside_ = false;
    return at_ == 0 ? bad("timeout") : advance(next);
  }

 private:
  static constexpr std::uint16_t kRegisters[6] = {0xED8D, 0xED8F, 0x0FFF, 0xEDEC, 0x1000, 0x0303};   // voltage, current, state of charge, temperature, capacity, cycles

  static int nibble(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

  std::vector<std::uint8_t> ask() {
    static const char* const kNames[6] = {"0xED8D voltage", "0xED8F current", "0x0FFF state of charge", "0xEDEC temperature", "0x1000 capacity", "0x0303 cycles"};
    command_ = kNames[at_];
    inside_ = false;
    return request(kRegisters[at_]);
  }
  Rx advance(std::vector<std::uint8_t>& next) {
    if (++at_ >= 6) {
      at_ = 5;
      detail::finish_module(module_);
      out_.modules.assign(1, module_);
      return Rx::kDone;
    }
    next = ask();
    return Rx::kNext;
  }
  void decode(std::uint32_t raw, std::size_t size) {
    const long value = size == 2 ? static_cast<long>(static_cast<std::int16_t>(raw)) : static_cast<long>(static_cast<std::int32_t>(raw));
    const bool missing = size == 2 && (raw == 0xFFFF || raw == 0x7FFF);   // "not known"
    switch (kRegisters[at_]) {
      case 0xED8D: module_.voltage_v = detail::bounded(value * 0.01, 0, 1000); break;
      case 0xED8F: if (raw != 0x7FFF) module_.current_a = detail::bounded(value * 0.1, -1000, 1000); break;
      case 0x0FFF: if (!missing) module_.soc_percent = static_cast<int>(detail::bounded(raw * 0.01 + 0.5, 0, 100)); break;
      case 0xEDEC:
        if (!missing) { module_.temperature_c = detail::bounded(raw * 0.01 - 273.15, -50, 200); module_.has_temperature = true; }
        break;
      case 0x1000: if (!missing) module_.full_capacity_ah = detail::bounded(raw, 0, 100000); break;
      default: if (raw <= 1000000) module_.cycles = static_cast<int>(raw); break;
    }
  }

  std::size_t at_ = 0, length_ = 0;
  bool inside_ = false;
  std::array<char, 24> line_{};
  pylontech::Module module_;
};

}  // namespace armor::solar::drivers
