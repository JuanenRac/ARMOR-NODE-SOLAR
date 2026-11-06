// ARMOR-NODE-SOLAR - tests/sim_bench.cpp: the driver of the simulation bench. The text of the console of a battery stack goes through the real parsers.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// It is built from the core files of the firmware, on a computer, and fed by ARMOR-SIMULATOR (python -m armor_simulator.node_bench) with the buses of the equipment, spoiled
// on purpose. Nothing here has run on a board.
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "../core/pylontech.hpp"

// The line protocol shared by the drivers of every node (see ARMOR-SIMULATOR docs/NODE_BENCH.md): one case per line on the standard input,
//   bus <TAB> id <TAB> arg <TAB> payload        (payload: hex of bytes or text; "-" when there is none)
// and one answer per case on the standard output: id <TAB> answer.
namespace simio {
inline std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::string item;
  for (char c : text) { if (c == sep) { out.push_back(item); item.clear(); } else item += c; }
  out.push_back(item);
  return out;
}
inline std::vector<std::uint8_t> unhex(const std::string& hex) {
  std::vector<std::uint8_t> out;
  if (hex == "-") return out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
  return out;
}
inline std::string unhex_text(const std::string& hex) { const auto bytes = unhex(hex); return std::string(bytes.begin(), bytes.end()); }
inline std::string fixed(double value, int digits) { char buffer[64]; std::snprintf(buffer, sizeof buffer, "%.*f", digits, value); return buffer; }
struct Case { std::string bus, id, arg, payload; };
inline bool next_case(Case& out) {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto f = split(line, '\t');
    if (f.size() != 4) continue;
    out = Case{f[0], f[1], f[2], f[3]};
    return true;
  }
  return false;
}
}  // namespace simio

static const char* const BUS = "pylontech";

// The payload is two texts in hex, separated by a comma: what `pwr` printed and what `bat 1` printed.
static std::string answer(const simio::Case& c) {
  namespace pl = armor::solar::pylontech;
  const auto parts = simio::split(c.payload, ',');
  if (parts.size() != 2) return "bad case";
  std::vector<pl::Module> modules;
  pl::parse_pwr(simio::unhex_text(parts[0]), modules);
  std::string text = "pwr=" + std::to_string(modules.size());
  for (const pl::Module& m : modules) {
    if (!m.present) { text += " " + std::to_string(m.number) + ":absent"; continue; }
    text += " " + std::to_string(m.number) + ":" + simio::fixed(m.voltage_v, 3) + ":" + simio::fixed(m.current_a, 3) + ":" + simio::fixed(m.temperature_c, 3) + ":" + std::to_string(m.soc_percent) + ":" + m.base_state;
  }
  std::vector<double> cells;
  pl::parse_bat(simio::unhex_text(parts[1]), cells);
  text += " | cells=" + std::to_string(cells.size());
  if (!cells.empty()) {
    text += " v=";
    for (std::size_t i = 0; i < cells.size(); ++i) text += std::string(i ? "," : "") + simio::fixed(cells[i], 3);
  }
  return text;
}

int main() {
  simio::Case c;
  while (simio::next_case(c)) std::cout << c.id << '\t' << (c.bus == BUS ? answer(c) : std::string("unsupported")) << '\n';
  return 0;
}
