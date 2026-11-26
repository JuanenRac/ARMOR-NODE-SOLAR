// ARMOR-NODE-SOLAR - the catalogue of the equipment protocols the node speaks: the id every part of A.R.M.O.R. uses, the port setting that selects it and its serial line.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The same table, with its words for people, is ARMOR-COMMON's src/armor_common/solar_protocols.json (tests/check_protocols.py compares them). A table in flash: no RAM.
#pragma once
#include <array>
#include <memory>
#include <string_view>

#include "driver_bms.hpp"
#include "driver_modbus.hpp"
#include "driver_vedirect.hpp"
#include "solar_config.hpp"

namespace armor::solar::protocols {

struct Protocol {
  const char* id;        // the catalogue id
  const char* port;      // the port's "kind" setting
  const char* dialect;   // the port's "dialect" setting ("" when the kind has a single protocol)
  int baud;              // 8N1 in all of them
  const char* message;   // which message its readings become: "inverter" or "battery"
};

constexpr std::array<Protocol, 15> kProtocols{{
    {"pi30", "voltronic", "pi30", 2400, "inverter"},
    {"pi30-revo", "voltronic", "revo", 2400, "inverter"},
    {"pi18", "voltronic", "pi18", 2400, "inverter"},
    {"pi41", "voltronic", "pi41", 2400, "inverter"},
    {"pi34", "voltronic", "pi34", 2400, "inverter"},
    {"modbus-smg2", "modbus", "smg2", 9600, "inverter"},
    {"modbus-srne-hybrid", "modbus", "srne-hybrid", 9600, "inverter"},
    {"modbus-srne-mppt", "modbus", "srne-mppt", 9600, "inverter"},
    {"vedirect-mppt", "vedirect", "mppt", 19200, "inverter"},
    {"vedirect-monitor", "vedirect", "monitor", 19200, "battery"},
    {"pylontech-console", "pylontech", "", 115200, "battery"},
    {"ant-bms", "ant", "", 19200, "battery"},
    {"daly-uart", "daly", "", 9600, "battery"},
    {"jbd", "jbd", "", 9600, "battery"},
    {"jk-rs485", "jk", "", 115200, "battery"},
}};

// The driver of a binary or stream protocol, or none (the Voltronic, Pylontech and ANT-BMS ports are handled by the poller itself).
inline std::unique_ptr<drivers::Driver> make_driver(config::Kind kind, std::string_view dialect) {
  if (kind == config::Kind::kModbus)
    return std::make_unique<drivers::ModbusDriver>(dialect == "srne-hybrid" ? drivers::ModbusMap::kSrneHybrid : dialect == "srne-mppt" ? drivers::ModbusMap::kSrneMppt : drivers::ModbusMap::kSmg2);
  if (kind == config::Kind::kDaly) return std::make_unique<drivers::DalyDriver>();
  if (kind == config::Kind::kJbd) return std::make_unique<drivers::JbdDriver>();
  if (kind == config::Kind::kJk) return std::make_unique<drivers::JkDriver>();
  if (kind == config::Kind::kVedirect) return std::make_unique<drivers::VedirectDriver>(dialect == "mppt");
  return nullptr;
}

}  // namespace armor::solar::protocols
