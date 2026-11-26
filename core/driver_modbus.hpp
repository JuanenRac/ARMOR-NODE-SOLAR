// ARMOR-NODE-SOLAR - inverters and solar chargers that speak Modbus RTU: Easun / ISolar SMG II and the SRNE family. Read only (function 0x03).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// 9600 baud 8N1, slave address 1. A request is [address, 03, register (2), count (2), CRC (2, low byte first)]; a reply is [address, 03, bytes, registers, CRC]; a refusal is
// [address, 83, code, CRC]. The CRC is CRC-16/MODBUS. The register numbers are the ones of the makers' public documents (the "SMG RS232 communication protocol" V1.0.1, and
// SRNE's Modbus documents for inverters, V1.96, and for charge controllers, V1.3). No unit of any of them has been connected: the SRNE maps in particular are read from a table
// whose layout is hard to follow and need a capture to be confirmed.
#pragma once
#include <array>

#include "equipment_driver.hpp"

namespace armor::solar::drivers {

inline std::uint16_t crc16_modbus(const std::uint8_t* data, std::size_t length) {
  std::uint16_t crc = 0xFFFF;
  for (std::size_t i = 0; i < length; ++i) {
    crc = static_cast<std::uint16_t>(crc ^ data[i]);
    for (int bit = 0; bit < 8; ++bit) crc = static_cast<std::uint16_t>((crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1));
  }
  return crc;
}

inline std::vector<std::uint8_t> modbus_read(std::uint8_t address, std::uint16_t first, std::uint16_t count) {
  std::vector<std::uint8_t> out{address, 0x03, static_cast<std::uint8_t>(first >> 8), static_cast<std::uint8_t>(first), static_cast<std::uint8_t>(count >> 8), static_cast<std::uint8_t>(count)};
  const std::uint16_t crc = crc16_modbus(out.data(), out.size());
  out.push_back(static_cast<std::uint8_t>(crc));
  out.push_back(static_cast<std::uint8_t>(crc >> 8));
  return out;
}

enum class ModbusMap : std::uint8_t { kSmg2, kSrneHybrid, kSrneMppt };

class ModbusDriver final : public Driver {
 public:
  explicit ModbusDriver(ModbusMap map, std::uint8_t address = 1) : map_(map), address_(address) {}
  bool inverter() const override { return true; }

  std::vector<std::uint8_t> begin() override {
    out_ = Out();
    error_ = "";
    at_ = 0;
    got_main_ = false;
    return request();
  }

  Rx feed(std::uint8_t byte, std::vector<std::uint8_t>& next) override {
    // the first byte of a reply is the slave's address: anything before it is noise
    if (length_ == 0 && byte != address_) return Rx::kMore;
    if (length_ >= frame_.size()) { length_ = 0; return Rx::kMore; }
    frame_[length_++] = byte;
    if (length_ < 3) return Rx::kMore;
    const bool refusal = frame_[1] == 0x83;
    if (!refusal && (frame_[1] != 0x03 || frame_[2] != block().count * 2)) { length_ = 0; return Rx::kMore; }
    const std::size_t whole = refusal ? 5 : 5u + frame_[2];
    if (length_ < whole) return Rx::kMore;
    length_ = 0;
    const std::uint16_t crc = crc16_modbus(frame_.data(), whole - 2);
    if (frame_[whole - 2] != static_cast<std::uint8_t>(crc) || frame_[whole - 1] != static_cast<std::uint8_t>(crc >> 8)) return bad("crc");
    if (refusal) return skip(next, "nak");
    decode(frame_.data() + 3);
    return advance(next);
  }

  Rx timeout(std::vector<std::uint8_t>& next) override { length_ = 0; return skip(next, "timeout"); }

 private:
  struct Block { std::uint16_t first, count; bool essential; const char* name; };
  static constexpr std::size_t kMaxRegisters = 40;

  std::size_t blocks() const { return map_ == ModbusMap::kSmg2 ? 3 : 2; }
  Block block() const {
    static constexpr Block kSmg2[3] = {{201, 34, true, "registers 201..234"}, {100, 2, false, "faults 100..101"}, {108, 2, false, "warnings 108..109"}};
    static constexpr Block kSrneHybrid[2] = {{0x0100, 15, true, "registers 0x0100..0x010E"}, {0x0210, 18, true, "registers 0x0210..0x0221"}};
    static constexpr Block kSrneMppt[2] = {{0x0100, 10, true, "registers 0x0100..0x0109"}, {0x010B, 3, false, "state and faults 0x010B..0x010D"}};
    return map_ == ModbusMap::kSmg2 ? kSmg2[at_] : map_ == ModbusMap::kSrneHybrid ? kSrneHybrid[at_] : kSrneMppt[at_];
  }
  std::vector<std::uint8_t> request() {
    length_ = 0;
    command_ = block().name;
    return modbus_read(address_, block().first, block().count);
  }
  Rx advance(std::vector<std::uint8_t>& next) {
    if (block().essential) got_main_ = true;
    if (++at_ >= blocks()) return got_main_ ? Rx::kDone : bad("format");
    next = request();
    return Rx::kNext;
  }
  // a block that is refused or not answered: an essential one loses the cycle, an optional one is left out
  Rx skip(std::vector<std::uint8_t>& next, const char* why) {
    if (block().essential) return bad(why);
    return advance(next);
  }

  void warn(const char* name) {
    for (const std::string& known : out_.warnings) if (known == name) return;
    if (out_.warnings.size() < 32) out_.warnings.emplace_back(name);
  }

  void decode(const std::uint8_t* data) {
    const Block b = block();
    const auto u = [&](std::uint16_t reg) { return static_cast<double>(detail::be16(data + 2 * (reg - b.first))); };
    const auto s = [&](std::uint16_t reg) { return static_cast<double>(detail::be16s(data + 2 * (reg - b.first))); };
    voltronic::Status& o = out_.status;
    if (map_ == ModbusMap::kSmg2) {
      if (b.first == 201) {
        // 201: 0 power on, 1 standby, 2 mains, 3 off grid, 4 bypass, 5 charging, 6 fault
        static constexpr char kModes[7] = {'P', 'S', 'L', 'B', 'L', 'L', 'F'};
        const double mode = u(201);
        out_.mode = mode < 7 ? kModes[static_cast<int>(mode)] : 0;
        o.grid_v = s(202) * 0.1; o.grid_hz = s(203) * 0.01;
        o.out_v = s(210) * 0.1; o.out_hz = s(212) * 0.01; o.out_w = s(213); o.out_va = s(214);
        o.battery_v = s(215) * 0.1;
        const double battery_a = s(232) * 0.1;   // positive: charging
        o.battery_charge_a = battery_a > 0 ? battery_a : 0;
        o.battery_discharge_a = battery_a < 0 ? -battery_a : 0;
        o.pv_v = s(219) * 0.1; o.pv_a = s(220) * 0.1; o.pv_w = s(223); o.has_pv_w = true;
        o.load_percent = s(225);
        o.heatsink_c = s(226) > s(227) ? s(226) : s(227);
        o.battery_percent = u(229);
        o.scc_charging = s(224) > 0;
        o.ac_charging = s(233) > 0;
        o.charging = battery_a > 0;
        o.load_on = o.out_v > 50;
      } else {
        const std::uint32_t bits = detail::be32(data);
        if (b.first == 100) {
          for (int bit = 0; bit < 32; ++bit) if (bits & (1u << bit)) warn(("fault_bit_" + std::to_string(bit)).c_str());
        } else {
          static const char* const kNames[19] = {"", "mains_waveform", "", "mains_low_voltage", "mains_over_frequency", "mains_low_frequency", "pv_low_voltage", "over_temperature",
                                                 "battery_low", "battery_open", "overload", "battery_equalisation", "battery_not_recovered", "power_limit", "fan_locked", "pv_energy_low",
                                                 "parallel_communication", "parallel_output_mode", "parallel_battery_difference"};
          for (int bit = 0; bit < 19; ++bit) if ((bits & (1u << bit)) && kNames[bit][0] != '\0') warn(kNames[bit]);
          out_.warnings_known = true;
        }
      }
    } else if (map_ == ModbusMap::kSrneHybrid) {
      if (b.first == 0x0100) {
        o.battery_percent = u(0x100);
        o.battery_v = u(0x101) * 0.1;
        const double current = s(0x102) * 0.1;   // positive: discharging
        o.battery_discharge_a = current > 0 ? current : 0;
        o.battery_charge_a = current < 0 ? -current : 0;
        o.pv_v = u(0x107) * 0.1; o.pv_a = u(0x108) * 0.1;
        o.pv_w = u(0x109); o.has_pv_w = true;
        o.charging = current < 0;
        o.scc_charging = o.pv_w > 0 && current < 0;
      } else {
        // 0 power on delay, 1 standby, 2 initialising, 3 soft start, 4 on the mains, 5 inverting, 6/7 transfers, 8 battery activation, 9 manual off, 10 fault
        static constexpr char kModes[11] = {'P', 'S', 'P', 'P', 'L', 'B', 'L', 'B', 'P', 'D', 'F'};
        const double mode = u(0x210);
        out_.mode = mode < 11 ? kModes[static_cast<int>(mode)] : 0;
        o.bus_v = u(0x212) * 0.1;
        o.grid_v = u(0x213) * 0.1; o.grid_hz = u(0x215) * 0.01;
        o.out_v = u(0x216) * 0.1; o.out_hz = u(0x218) * 0.01;
        o.out_w = u(0x21B); o.out_va = u(0x21C);
        o.ac_charging = s(0x21E) > 0;
        o.load_percent = u(0x21F);
        o.heatsink_c = (s(0x220) > s(0x221) ? s(0x220) : s(0x221)) * 0.1;
        o.load_on = o.out_v > 50;
      }
    } else {   // SRNE charge controller: it has no AC side
      if (b.first == 0x0100) {
        o.battery_percent = u(0x100);
        o.battery_v = u(0x101) * 0.1;
        o.battery_charge_a = u(0x102) * 0.1;
        const std::uint8_t raw = data[2 * 3];   // high byte of 0x0103: the controller's temperature, bit 7 is the sign
        o.heatsink_c = (raw & 0x80) ? -static_cast<double>(raw & 0x7F) : static_cast<double>(raw);
        o.pv_v = u(0x107) * 0.1; o.pv_a = u(0x108) * 0.1; o.pv_w = u(0x109); o.has_pv_w = true;
        o.charging = o.battery_charge_a > 0;
        o.scc_charging = o.charging;
      } else {
        const std::uint32_t faults = detail::be32(data + 2);
        for (int bit = 0; bit < 31; ++bit) if (faults & (1u << bit)) warn(("fault_bit_" + std::to_string(bit)).c_str());
        out_.warnings_known = true;
      }
    }
    voltronic::hold_limits(o);
  }

  ModbusMap map_;
  std::uint8_t address_;
  std::size_t at_ = 0, length_ = 0;
  bool got_main_ = false;
  std::array<std::uint8_t, 5 + 2 * kMaxRegisters> frame_{};
};

}  // namespace armor::solar::drivers
