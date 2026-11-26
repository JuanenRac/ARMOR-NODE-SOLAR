// ARMOR-NODE-SOLAR - three battery management systems with a binary serial protocol: DALY (UART/RS485), JBD / Jiabaida and JK (RS485/UART). Read only.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Written from the makers' public protocol documents; none of these boards has been connected.
//   DALY, 9600 8N1: thirteen-byte frames A5, address (0x40 from the host, 0x01 from the board), data id, length 8, eight bytes, sum of the twelve before. Asked: 0x90 (voltage,
//     current, state of charge), 0x93 (state and remaining capacity), 0x94 (cell and sensor counts), 0x95 (cell voltages, three per frame), 0x96 (temperatures, seven per frame)
//     and 0x98 (failures). Only 0x90 is essential.
//   JBD, 9600 8N1: DD A5 <command> 00 <checksum> 77 -> DD <command> <status> <length> <data> <checksum> 77; the checksum is 0x10000 less the sum of status, length and data.
//     Asked: 0x03 (basic information) and 0x04 (cells).
//   JK, 115200 8N1: 4E 57, length, terminal, command, source, type, identified data, record number, 68, four bytes of checksum (the last two: the sum of all before 68 included).
//     Asked: the "read all data" request (command 0x06).
#pragma once
#include <array>

#include "equipment_driver.hpp"

namespace armor::solar::drivers {

// ---- DALY --------------------------------------------------------------------------------------------------------------------------

class DalyDriver final : public Driver {
 public:
  bool inverter() const override { return false; }

  static std::vector<std::uint8_t> request(std::uint8_t id) {
    std::vector<std::uint8_t> out(13, 0);
    out[0] = 0xA5; out[1] = 0x40; out[2] = id; out[3] = 0x08;
    unsigned sum = 0;
    for (std::size_t i = 0; i < 12; ++i) sum += out[i];
    out[12] = static_cast<std::uint8_t>(sum);
    return out;
  }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    module_ = pylontech::Module();
    at_ = 0;
    cells_ = sensors_ = 0;
    return ask();
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>& next) override {
    if (length_ == 0 && byte != 0xA5) return Rx::kMore;
    frame_[length_++] = byte;
    if (length_ < frame_.size()) return Rx::kMore;
    length_ = 0;
    unsigned sum = 0;
    for (std::size_t i = 0; i < 12; ++i) sum += frame_[i];
    if (static_cast<std::uint8_t>(sum) != frame_[12]) return skip(next, "crc");
    if (frame_[1] == 0x40 || frame_[2] != kIds[at_] || frame_[3] != 8) return Rx::kMore;   // our own request heard back, or an answer to something else
    decode(frame_.data() + 4);
    if (++frames_ < frames_wanted_) return Rx::kMore;
    return advance(next);
  }

  Rx timeout(std::vector<std::uint8_t>& next) override { length_ = 0; return skip(next, "timeout"); }

 private:
  static constexpr std::uint8_t kIds[6] = {0x90, 0x93, 0x94, 0x95, 0x96, 0x98};

  std::vector<std::uint8_t> ask() {
    length_ = 0;
    frames_ = 0;
    const std::uint8_t id = kIds[at_];
    frames_wanted_ = id == 0x95 ? (cells_ + 2) / 3 : id == 0x96 ? (sensors_ + 6) / 7 : 1;
    static const char* const kNames[6] = {"0x90 voltage and current", "0x93 state", "0x94 counts", "0x95 cells", "0x96 temperatures", "0x98 failures"};
    command_ = kNames[at_];
    return request(id);
  }
  Rx advance(std::vector<std::uint8_t>& next) {
    for (;;) {
      if (++at_ >= 6) return finish();
      // cells and temperatures are only asked for when the board said how many it has
      if ((kIds[at_] == 0x95 && cells_ == 0) || (kIds[at_] == 0x96 && sensors_ == 0)) continue;
      next = ask();
      return Rx::kNext;
    }
  }
  Rx skip(std::vector<std::uint8_t>& next, const char* why) {
    if (at_ == 0) return bad(why);
    return advance(next);
  }
  Rx finish() {
    detail::finish_module(module_);
    out_.modules.assign(1, module_);
    return Rx::kDone;
  }
  void decode(const std::uint8_t* d) {
    switch (kIds[at_]) {
      case 0x90:
        module_.voltage_v = detail::be16(d) * 0.1;
        module_.current_a = detail::bounded((static_cast<int>(detail::be16(d + 4)) - 30000) * 0.1, -1000, 1000);
        module_.soc_percent = static_cast<int>(detail::bounded(detail::be16(d + 6) * 0.1 + 0.5, 0, 100));
        module_.has_temperature = false;
        break;
      case 0x93:
        module_.base_state = d[0] == 1 ? "Charge" : d[0] == 2 ? "Dischg" : "Idle";
        module_.capacity_ah = detail::bounded(detail::be32(d + 4) * 0.001, 0, 100000);
        break;
      case 0x94:
        cells_ = d[0] <= 32 ? d[0] : 32;
        sensors_ = d[1] <= 8 ? d[1] : 8;
        module_.cell_count = cells_ > 0 ? static_cast<int>(cells_) : -1;
        break;
      case 0x95: {
        const std::size_t first = static_cast<std::size_t>(d[0] > 0 ? d[0] - 1 : 0) * 3;   // frame numbers start at 1
        for (std::size_t i = 0; i < 3 && first + i < cells_; ++i) {
          if (module_.cells_v.size() <= first + i) module_.cells_v.resize(first + i + 1, 0.0);
          module_.cells_v[first + i] = detail::bounded(detail::be16(d + 1 + 2 * i) * 0.001, 0, 10);
        }
        break;
      }
      case 0x96:
        for (std::size_t i = 1; i < 8 && module_.temperatures_c.size() < sensors_; ++i) module_.temperatures_c.push_back(static_cast<double>(d[i]) - 40);
        module_.has_temperature = !module_.temperatures_c.empty();
        break;
      default: {
        bool any = false;
        for (std::size_t i = 0; i < 7; ++i) any = any || d[i] != 0;
        module_.system_alarm_state = any ? "Alarm" : "Normal";
        break;
      }
    }
  }

  std::size_t at_ = 0, length_ = 0, frames_ = 0, frames_wanted_ = 1, cells_ = 0, sensors_ = 0;
  std::array<std::uint8_t, 13> frame_{};
  pylontech::Module module_;
};

// ---- JBD / Jiabaida ----------------------------------------------------------------------------------------------------------------

class JbdDriver final : public Driver {
 public:
  bool inverter() const override { return false; }

  static std::vector<std::uint8_t> request(std::uint8_t command) {
    const std::uint16_t sum = static_cast<std::uint16_t>(0x10000u - command);
    return {0xDD, 0xA5, command, 0x00, static_cast<std::uint8_t>(sum >> 8), static_cast<std::uint8_t>(sum), 0x77};
  }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    module_ = pylontech::Module();
    length_ = 0;
    command_id_ = 0x03;
    command_ = "0x03 basic information";
    return request(0x03);
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>& next) override {
    if (length_ == 0 && byte != 0xDD) return Rx::kMore;
    if (length_ >= frame_.size()) { length_ = 0; return Rx::kMore; }
    frame_[length_++] = byte;
    if (length_ == 2 && frame_[1] != command_id_) { length_ = 0; return Rx::kMore; }
    if (length_ < 4 || length_ < 7u + frame_[3]) return Rx::kMore;
    const std::size_t n = frame_[3];
    length_ = 0;
    if (frame_[6 + n] != 0x77) return bad("format");
    unsigned sum = 0;
    for (std::size_t i = 2; i < 4 + n; ++i) sum += frame_[i];
    if (static_cast<std::uint16_t>(0x10000u - sum) != detail::be16(frame_.data() + 4 + n)) return bad("crc");
    if (frame_[2] != 0) return bad("nak");
    const std::uint8_t* d = frame_.data() + 4;
    if (command_id_ == 0x03) {
      if (!basic(d, n)) return bad("format");
      command_id_ = 0x04;
      command_ = "0x04 cell voltages";
      next = request(0x04);
      return Rx::kNext;
    }
    for (std::size_t i = 0; i + 1 < n && module_.cells_v.size() < 32; i += 2) module_.cells_v.push_back(detail::bounded(detail::be16(d + i) * 0.001, 0, 10));
    return finish();
  }

  // The cells are the extra: a board that answered the basic information is a reading without them.
  Rx timeout(std::vector<std::uint8_t>&) override {
    length_ = 0;
    return command_id_ == 0x04 ? finish() : bad("timeout");
  }

 private:
  bool basic(const std::uint8_t* d, std::size_t n) {
    if (n < 23) return false;
    module_.voltage_v = detail::be16(d) * 0.01;
    module_.current_a = detail::bounded(detail::be16s(d + 2) * 0.01, -1000, 1000);   // positive: charging
    module_.capacity_ah = detail::be16(d + 4) * 0.01;
    module_.full_capacity_ah = detail::be16(d + 6) * 0.01;
    module_.cycles = detail::be16(d + 8);
    int balancing = 0;
    for (std::uint32_t bits = (static_cast<std::uint32_t>(detail::be16(d + 14)) << 16) | detail::be16(d + 12); bits != 0; bits >>= 1) balancing += static_cast<int>(bits & 1);
    module_.balancing = balancing;
    const std::uint16_t protection = detail::be16(d + 16);
    module_.protecting = protection != 0;
    module_.system_alarm_state = protection != 0 ? "Alarm" : "Normal";
    module_.soc_percent = d[19] <= 100 ? d[19] : 100;
    module_.cell_count = d[21] >= 1 && d[21] <= 32 ? d[21] : -1;
    const std::size_t sensors = d[22];
    for (std::size_t i = 0; i < sensors && i < 8 && 23 + 2 * i + 1 < n; ++i)
      module_.temperatures_c.push_back(detail::bounded((static_cast<int>(detail::be16(d + 23 + 2 * i)) - 2731) * 0.1, -50, 200));
    module_.has_temperature = !module_.temperatures_c.empty();
    return true;
  }
  Rx finish() {
    detail::finish_module(module_);
    out_.modules.assign(1, module_);
    return Rx::kDone;
  }

  std::size_t length_ = 0;
  std::uint8_t command_id_ = 0x03;
  std::array<std::uint8_t, 80> frame_{};
  pylontech::Module module_;
};

// ---- JK ------------------------------------------------------------------------------------------------------------------------------

namespace jk {
// The size of the data of each identifier of the document, from 0x80 on; 0x79 carries its own length. An identifier that is not here ends the reading of the list.
inline std::size_t size_of(std::uint8_t id) {
  if (id >= 0x80 && id <= 0x84) return 2;
  if (id == 0x85 || id == 0x86) return 1;
  if (id == 0x87 || (id >= 0x8A && id <= 0x8C)) return 2;
  if (id == 0x89) return 4;
  if (id >= 0x8E && id <= 0x9C) return 2;
  if (id == 0x9D) return 1;
  if (id >= 0x9E && id <= 0xA8) return 2;
  if (id == 0xA9) return 1;
  if (id == 0xAA) return 4;
  if (id == 0xAB || id == 0xAC) return 1;
  if (id == 0xAD) return 2;
  if (id == 0xAE || id == 0xAF) return 1;
  if (id == 0xB0) return 2;
  if (id == 0xB1) return 1;
  if (id == 0xB2) return 10;
  if (id == 0xB3) return 1;
  if (id == 0xB4) return 8;
  if (id == 0xB5 || id == 0xB6) return 4;
  if (id == 0xB7) return 15;
  if (id == 0xB8) return 1;
  if (id == 0xB9) return 4;
  if (id == 0xBA) return 24;
  if (id >= 0xBB && id <= 0xBD) return 1;
  if (id == 0xBE || id == 0xBF) return 2;
  if (id == 0xC0) return 1;
  return 0;
}
// 0..100 is that temperature in degrees; above 100 it is below zero (101 is -1).
inline double temperature(std::uint16_t raw) { return raw <= 100 ? static_cast<double>(raw) : -static_cast<double>(raw - 100); }
}  // namespace jk

class JkDriver final : public Driver {
 public:
  bool inverter() const override { return false; }

  static std::vector<std::uint8_t> request() {
    std::vector<std::uint8_t> out{0x4E, 0x57, 0x00, 0x13, 0, 0, 0, 0, 0x06, 0x03, 0x00, 0x00, 0, 0, 0, 0, 0x68};
    unsigned sum = 0;
    for (const std::uint8_t b : out) sum += b;
    out.insert(out.end(), {0x00, 0x00, static_cast<std::uint8_t>(sum >> 8), static_cast<std::uint8_t>(sum)});
    return out;
  }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    frame_.clear();
    command_ = "read all data";
    return request();
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>&) override {
    if (frame_.empty() && byte != 0x4E) return Rx::kMore;
    if (frame_.size() == 1 && byte != 0x57) { frame_.clear(); return Rx::kMore; }
    frame_.push_back(byte);
    if (frame_.size() < 4) return Rx::kMore;
    const std::size_t whole = static_cast<std::size_t>(detail::be16(frame_.data() + 2)) + 2;
    if (whole < 20 || whole > kMaxFrame) { frame_.clear(); return Rx::kMore; }
    if (frame_.size() < whole) return Rx::kMore;
    const Rx result = decode();
    std::vector<std::uint8_t>().swap(frame_);   // give the memory back between cycles
    return result;
  }

  Rx timeout(std::vector<std::uint8_t>&) override {
    std::vector<std::uint8_t>().swap(frame_);
    return bad("timeout");
  }

 private:
  static constexpr std::size_t kMaxFrame = 400;

  Rx decode() {
    const std::size_t whole = frame_.size();
    if (frame_[whole - 5] != 0x68) return bad("format");
    unsigned sum = 0;
    for (std::size_t i = 0; i + 4 < whole; ++i) sum += frame_[i];
    if (static_cast<std::uint16_t>(sum) != detail::be16(frame_.data() + whole - 2)) return bad("crc");
    if (frame_[10] == 0x00) return Rx::kMore;   // a request (our own, heard back), not an answer
    pylontech::Module m;
    m.has_temperature = false;
    bool have_voltage = false;
    std::uint16_t raw_current = 0;
    bool have_current = false;
    int version = 0;
    const std::size_t end = whole - 9;   // the record number, 0x68 and the checksum follow the data
    std::size_t at = 11;
    while (at < end) {
      const std::uint8_t id = frame_[at++];
      if (id == 0x79) {
        if (at >= end) break;
        const std::size_t length = frame_[at++];
        if (at + length > end) return bad("format");
        for (std::size_t i = 0; i + 2 < length && m.cells_v.size() < 32; i += 3) m.cells_v.push_back(detail::bounded(detail::be16(frame_.data() + at + i + 1) * 0.001, 0, 10));
        at += length;
        continue;
      }
      const std::size_t size = jk::size_of(id);
      if (size == 0 || at + size > end) break;
      const std::uint8_t* d = frame_.data() + at;
      if (id >= 0x80 && id <= 0x82) m.temperatures_c.push_back(detail::bounded(jk::temperature(detail::be16(d)), -50, 200));
      else if (id == 0x83) { m.voltage_v = detail::be16(d) * 0.01; have_voltage = true; }
      else if (id == 0x84) { raw_current = detail::be16(d); have_current = true; }
      else if (id == 0x85) m.soc_percent = d[0] <= 100 ? d[0] : 100;
      else if (id == 0x87) m.cycles = detail::be16(d);
      else if (id == 0x8A) m.cell_count = detail::be16(d) >= 1 && detail::be16(d) <= 32 ? detail::be16(d) : -1;
      else if (id == 0x8B) { m.protecting = detail::be16(d) != 0; m.system_alarm_state = m.protecting ? "Alarm" : "Normal"; }
      else if (id == 0xAA) m.full_capacity_ah = detail::bounded(detail::be32(d), 0, 100000);
      else if (id == 0xC0) version = d[0];
      at += size;
    }
    if (!have_voltage) return bad("format");
    // The current: in the first version of the protocol 10000 less the value, in 10 mA (positive: charging); in version 1 the top bit says charging and the rest is the magnitude.
    if (have_current) {
      const double amps = version == 1 ? ((raw_current & 0x8000) ? 1.0 : -1.0) * (raw_current & 0x7FFF) * 0.01 : (10000 - static_cast<int>(raw_current)) * 0.01;
      m.current_a = detail::bounded(amps, -1000, 1000);
    }
    m.has_temperature = !m.temperatures_c.empty();
    detail::finish_module(m);
    out_.modules.assign(1, m);
    return Rx::kDone;
  }

  std::vector<std::uint8_t> frame_;
};

}  // namespace armor::solar::drivers
