// ARMOR-NODE-SOLAR - reading a Modbus unit from a table of its registers: which register holds what, its type and its scale. It plans the reads and turns the answers into named values.
// Read only: the only request it can build is function 0x03 (driver_modbus.hpp). No driver of a port uses it yet; a new make is a table, checked on a computer, before it is wired in.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "driver_modbus.hpp"

namespace armor::solar::drivers {

enum class RegisterType : std::uint8_t { kU16, kS16, kU32, kS32 };   // the 32-bit ones take two registers, the high word first

struct TableField {
  std::uint16_t reg;
  RegisterType type;
  double scale;
  const char* name;
};

struct TableBlock {
  std::uint16_t first = 0, count = 0;
};

struct TableValue {
  const char* name = "";
  double value = 0;
};

inline std::uint16_t width_of(RegisterType type) { return type == RegisterType::kU32 || type == RegisterType::kS32 ? 2 : 1; }

// The reads that cover every field of a table, in the order of the registers. A read never asks for more than `most` registers, and never for registers no field names beyond `gap`
// of them in a row: a unit may refuse a read that touches a register it does not have. A `most` too small for a field leaves that field in a read of its own.
inline std::vector<TableBlock> plan_reads(const TableField* fields, std::size_t count, std::uint16_t most = 32, std::uint16_t gap = 0) {
  std::vector<TableField> sorted(fields, fields + count);
  std::sort(sorted.begin(), sorted.end(), [](const TableField& a, const TableField& b) { return a.reg < b.reg; });
  std::vector<TableBlock> blocks;
  for (const TableField& field : sorted) {
    const std::uint32_t end = static_cast<std::uint32_t>(field.reg) + width_of(field.type);   // the first register after the field
    if (!blocks.empty()) {
      TableBlock& last = blocks.back();
      const std::uint32_t covered = static_cast<std::uint32_t>(last.first) + last.count;
      if (field.reg <= covered + gap && end - last.first <= most) {
        if (end > covered) last.count = static_cast<std::uint16_t>(end - last.first);
        continue;
      }
    }
    blocks.push_back({field.reg, width_of(field.type)});
  }
  return blocks;
}

inline std::vector<std::uint8_t> block_request(std::uint8_t address, const TableBlock& block) { return modbus_read(address, block.first, block.count); }

// The values of the fields that lie wholly inside a block, from the data bytes of its answer (two per register, the high byte first). False, and nothing added, when the number of
// bytes is not that of the block.
inline bool decode_block(const TableBlock& block, const std::uint8_t* data, std::size_t length, const TableField* fields, std::size_t count, std::vector<TableValue>& out) {
  if (length != 2u * block.count) return false;
  for (std::size_t i = 0; i < count; ++i) {
    const TableField& field = fields[i];
    if (field.reg < block.first || static_cast<std::uint32_t>(field.reg) + width_of(field.type) > static_cast<std::uint32_t>(block.first) + block.count) continue;
    const std::uint8_t* at = data + 2u * static_cast<std::size_t>(field.reg - block.first);
    double raw = 0;
    if (width_of(field.type) == 1) {
      const std::uint16_t word = static_cast<std::uint16_t>((at[0] << 8) | at[1]);
      raw = field.type == RegisterType::kS16 ? static_cast<double>(static_cast<std::int16_t>(word)) : static_cast<double>(word);
    } else {
      const std::uint32_t word = (static_cast<std::uint32_t>(at[0]) << 24) | (static_cast<std::uint32_t>(at[1]) << 16) | (static_cast<std::uint32_t>(at[2]) << 8) | at[3];
      raw = field.type == RegisterType::kS32 ? static_cast<double>(static_cast<std::int32_t>(word)) : static_cast<double>(word);
    }
    out.push_back({field.name, raw * field.scale});
  }
  return true;
}

// The first block of an SRNE charge controller written as a table: the same registers and scales as ModbusDriver's kSrneMppt, which a test holds it to.
inline constexpr TableField kSrneMpptTable[] = {
    {0x0100, RegisterType::kU16, 1, "battery_percent"}, {0x0101, RegisterType::kU16, 0.1, "battery_v"}, {0x0102, RegisterType::kU16, 0.1, "battery_charge_a"},
    {0x0107, RegisterType::kU16, 0.1, "pv_v"},          {0x0108, RegisterType::kU16, 0.1, "pv_a"},      {0x0109, RegisterType::kU16, 1, "pv_w"},
};

}  // namespace armor::solar::drivers
