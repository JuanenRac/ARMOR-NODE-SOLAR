// ARMOR-NODE-SOLAR - tests/mqtt_shim.cpp: the real decoding and message code of the node as a program for a computer, for the MQTT shim of ARMOR-SIMULATOR.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
//   mqtt_shim <node-id>      standard input, one reading per line, fields separated by a TAB:
//                              timestamp_ms <TAB> inverter <TAB> device <TAB> QPIGS text <TAB> QMOD letter <TAB> QPIWS flags
//                              timestamp_ms <TAB> battery  <TAB> device <TAB> hex of the `pwr` table <TAB> hex of the `bat` table of module 1
//                            standard output, one message per line:  topic <TAB> payload
//
// It is built from the core files of the firmware: the Voltronic framing and decoders (the replies are framed here with the node's own CRC and read back through the node's own
// reader, as the node does with what the serial line gives it), the Pylontech console reader and the writer of the solar message. It does not talk to any broker: the simulator
// publishes what it prints, so the server receives exactly the bytes the node would have sent. A line that does not decode is left out, as the node leaves out a device with no
// good reading. Nothing here has run on a board; the radio, the TLS and the clock of the board are not in it.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "../core/mqtt_topic.hpp"
#include "../core/pylontech.hpp"
#include "../core/solar_json.hpp"
#include "../core/voltronic.hpp"

namespace {
std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::string item;
  for (char c : text) { if (c == sep) { out.push_back(item); item.clear(); } else item += c; }
  out.push_back(item);
  return out;
}
std::string unhex(const std::string& hex) {
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
  return out;
}
// The text of a reply as it comes through the serial line and the node's reader: framed with the node's CRC, then taken apart again.
bool through_the_wire(const std::string& text, std::string& back) {
  namespace v = armor::solar::voltronic;
  std::vector<std::uint8_t> frame{'('};
  frame.insert(frame.end(), text.begin(), text.end());
  const auto crc = v::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]); frame.push_back(crc[1]); frame.push_back('\r');
  return v::parse_reply(frame.data(), frame.size(), back);
}
}  // namespace

int main(int argc, char** argv) {
  namespace v = armor::solar::voltronic;
  namespace py = armor::solar::pylontech;
  const std::string node = argc > 1 ? argv[1] : "";
  const std::string health_topic = armor::mqtt::will_topic(node);
  if (!health_topic.empty()) std::cout << health_topic << '\t' << armor::mqtt::birth_payload(node) << '\n';
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::vector<std::string> f = split(line, '\t');
    if (f.size() < 5) continue;
    const std::uint64_t now_ms = std::stoull(f[0]);
    const std::string topic = armor::solar::topic(node, f[2]);
    if (topic.empty()) continue;
    if (f[1] == "inverter" && f.size() >= 6) {
      std::string qpigs, qmod, qpiws;
      v::Status status;
      char mode = 0;
      std::vector<std::string> warnings;
      if (!through_the_wire(f[3], qpigs) || !v::parse_qpigs(qpigs, status)) continue;
      if (through_the_wire(f[4], qmod)) v::parse_qmod(qmod, mode);
      if (through_the_wire(f[5], qpiws)) v::parse_qpiws(qpiws, warnings);
      std::cout << topic << '\t' << armor::solar::inverter_json(node, f[2], now_ms, mode, status, warnings) << '\n';
    } else if (f[1] == "battery") {
      std::vector<py::Module> modules;
      if (py::parse_pwr(unhex(f[3]), modules) == 0) continue;
      std::vector<double> cells;
      if (py::parse_bat(unhex(f[4]), cells) > 0) py::attach_cells(modules, 1, cells);
      std::cout << topic << '\t' << armor::solar::battery_json(node, f[2], now_ms, modules) << '\n';
    }
  }
  return 0;
}
