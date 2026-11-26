// ARMOR-NODE-SOLAR - Victron VE.Direct, text mode: the equipment talks on its own once a second, the node only listens.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// 19200 baud 8N1. A block is lines of "\r\n<label>\t<value>" and ends with the line "Checksum\t<one byte>": the sum of every byte of the block, that one included, is zero modulo
// 256. A frame of the hexadecimal mode (':' to the end of the line) may arrive in between and is skipped. From the public "VE.Direct protocol" document; none has been connected.
//
// Two uses: "monitor" (a BMV or SmartShunt: the battery's voltage, current, state of charge and temperature) and "mppt" (a SmartSolar / BlueSolar charger: the panel and the
// battery side). The VE.Direct port is a 5 V (or 3.3 V) TTL level on a JST-PH connector, not RS232: it needs its own wiring (see docs/NODE_HARDWARE.md).
#pragma once
#include <array>
#include <cstring>

#include "equipment_driver.hpp"

namespace armor::solar::drivers {

class VedirectDriver final : public Driver {
 public:
  explicit VedirectDriver(bool charger) : charger_(charger) {}
  bool inverter() const override { return charger_; }

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

  Rx finish() {
    if (!fields_.v) return Rx::kMore;   // the history block of a monitor has no voltage: not the one we want
    if (charger_) {
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

  bool charger_;
  State state_ = State::kLabel;
  bool hex_ = false;
  std::uint8_t sum_ = 0;
  std::array<char, 12> label_{};
  std::array<char, 24> value_{};
  std::size_t label_length_ = 0, value_length_ = 0;
  Fields fields_;
};

}  // namespace armor::solar::drivers
