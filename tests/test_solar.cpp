// ARMOR-NODE-SOLAR - host tests: the Voltronic / MPP Solar protocol, the Pylontech console and frames, and the messages.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The replies below are typical ones of the public documents of these devices, written by hand; no real device has been read.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "../core/solar_json.hpp"
#include "../core/solar_config.hpp"
#include "../core/modbus_table.hpp"
#include "../core/poller.hpp"
#include "pylontech_rs485_real.hpp"
#include "pylontech_real.hpp"
#include "pylontech_us2000_real.hpp"
#include "pylontech_logs_real.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace armor::solar;

static std::vector<std::uint8_t> reply(const std::string& text) {
  std::vector<std::uint8_t> frame{'('};
  frame.insert(frame.end(), text.begin(), text.end());
  const auto crc = voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]); frame.push_back(crc[1]); frame.push_back('\r');
  return frame;
}

static void test_voltronic_commands() {
  std::vector<std::uint8_t> out;
  // the CRCs of the reading commands, as the protocol document lists them on the wire
  const struct { const char* command; std::uint8_t hi, lo; } known[] = {{"QPIGS", 0xB7, 0xA9}, {"QPIRI", 0xF8, 0x54}, {"QMOD", 0x49, 0xC1}, {"QPIWS", 0xB4, 0xDA}};
  for (const auto& k : known) {
    CHECK(voltronic::build_command(k.command, out));
    CHECK(out.size() == std::string(k.command).size() + 3 && out[out.size() - 3] == k.hi && out[out.size() - 2] == k.lo && out.back() == '\r');
  }
  // the standard check value of CRC-16/XMODEM
  const std::string nine = "123456789";
  CHECK(voltronic::crc16_xmodem(reinterpret_cast<const std::uint8_t*>(nine.data()), nine.size()) == 0x31C3);
  // only commands that read; anything odd is refused
  CHECK(!voltronic::build_command("PCP00", out) && !voltronic::build_command("POP02", out) && !voltronic::build_command("Q", out) && !voltronic::build_command("qpigs", out));
  CHECK(!voltronic::build_command("QPIGS\r", out) && !voltronic::build_command("QPIGS (", out) && !voltronic::build_command("QAAAAAAAAAAAA", out));
  // a byte of the CRC that would be a delimiter is moved by one
  bool moved = false;
  for (int a = 'A'; a <= 'Z'; ++a) for (int b = 'A'; b <= 'Z'; ++b) for (int c = 'A'; c <= 'Z'; ++c) {
    const std::uint8_t data[4] = {'Q', static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(c)};
    const std::uint16_t raw = voltronic::crc16_xmodem(data, 4);
    const auto wire = voltronic::wire_crc(data, 4);
    for (int i = 0; i < 2; ++i) {
      const std::uint8_t r = i == 0 ? static_cast<std::uint8_t>(raw >> 8) : static_cast<std::uint8_t>(raw & 0xFF);
      if (r == 0x28 || r == 0x0D || r == 0x0A) { moved = true; if (wire[static_cast<std::size_t>(i)] != r + 1) CHECK(false); }
    }
    if (wire[0] == '(' || wire[0] == '\r' || wire[0] == '\n' || wire[1] == '(' || wire[1] == '\r' || wire[1] == '\n') CHECK(false);
  }
  CHECK(moved);
}

static void test_voltronic_replies() {
  const std::string qpigs = "000.0 00.0 230.0 50.0 0161 0119 003 460 57.50 012 100 0069 0014 103.8 57.45 00000 00110110 00 00 00856 010";
  std::string text;
  const std::vector<std::uint8_t> frame = reply(qpigs);
  CHECK(voltronic::parse_reply(frame.data(), frame.size(), text) && text == qpigs);
  voltronic::Status s;
  CHECK(voltronic::parse_qpigs(text, s));
  CHECK(s.grid_v == 0.0 && s.out_v == 230.0 && s.out_hz == 50.0 && s.out_va == 161 && s.out_w == 119 && s.load_percent == 3 && s.bus_v == 460);
  CHECK(s.battery_v == 57.5 && s.battery_charge_a == 12 && s.battery_percent == 100 && s.heatsink_c == 69 && s.pv_a == 14 && s.pv_v > 103.79 && s.pv_v < 103.81);
  CHECK(s.battery_discharge_a == 0 && s.battery_a() == 12.0);
  CHECK(s.pv_w == 856 && s.has_pv_w);
  // 00110110: b5 SCC firmware updated, b4 load on, b2 charging, b1 SCC charging
  CHECK(!s.sbu_priority && !s.config_changed && s.scc_updated && s.load_on && !s.battery_steady && s.charging && s.scc_charging && !s.ac_charging);
  // an older model with fewer fields: the PV power is the product
  voltronic::Status old;
  CHECK(voltronic::parse_qpigs("232.0 50.0 230.0 50.0 0000 0000 000 380 54.00 000 100 0035 00.0 000.0 00.00 00000 00010000", old));
  CHECK(!old.has_pv_w && old.pv_w == 0 && old.load_on && !old.charging);
  CHECK(!voltronic::parse_qpigs("", s) && !voltronic::parse_qpigs("NAK", s) && !voltronic::parse_qpigs("1 2 3", s));
  CHECK(!voltronic::parse_qpigs("000.0 00.0 230.0 50.0 0161 0119 003 460 5x.50 012 100 0069 0014 103.8 57.45 00000 00110110", s));
  CHECK(!voltronic::parse_qpigs("000.0 00.0 230.0 50.0 0161 0119 003 460 57.50 012 100 0069 0014 103.8 57.45 00000 0011011", s));   // 7 status bits
  // the mode and the warnings
  char mode = 0;
  CHECK(voltronic::parse_qmod("B", mode) && mode == 'B' && std::string(voltronic::mode_name(mode)) == "battery");
  CHECK(voltronic::parse_qmod("L", mode) && std::string(voltronic::mode_name(mode)) == "line");
  CHECK(!voltronic::parse_qmod("", mode) && !voltronic::parse_qmod("BB", mode) && !voltronic::parse_qmod("Z", mode));
  std::vector<std::string> warnings;
  CHECK(voltronic::parse_qpiws("00000000000000000000000000000000", warnings) && warnings.empty());
  CHECK(voltronic::parse_qpiws("00000100000010000000000000000000", warnings) && warnings.size() == 2 && warnings[0] == "line_fail" && warnings[1] == "battery_low");
  CHECK(voltronic::parse_qpiws("0000010", warnings) && warnings.size() == 1);   // a short reply: the rest is off
  CHECK(!voltronic::parse_qpiws("0000x", warnings) && !voltronic::parse_qpiws("", warnings));
  // the ratings
  voltronic::Ratings r;
  CHECK(voltronic::parse_qpiri("230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 1 1 01 01 1 0 54.0 0 1", r));
  CHECK(r.out_rating_w == 5000 && r.battery_rating_v == 48.0 && r.battery_bulk_v == 56.4 && r.battery_float_v == 54.0 && r.battery_type == 0);
  CHECK(r.max_ac_charge_a == 30 && r.max_charge_a == 60 && r.input_range == 0 && r.output_priority == 1 && r.charger_priority == 1);
  CHECK(!voltronic::parse_qpiri("230.0 21.7", r));
}

static void test_voltronic_frames() {
  const std::vector<std::uint8_t> good = reply("B");
  std::string text;
  CHECK(voltronic::parse_reply(good.data(), good.size(), text) && text == "B");
  std::vector<std::uint8_t> bad = good;
  bad[1] = 'L';
  CHECK(!voltronic::parse_reply(bad.data(), bad.size(), text));
  CHECK(!voltronic::parse_reply(good.data(), good.size() - 1, text));
  const std::uint8_t nak[] = {'(', 'N', 'A', 'K', 0x73, 0x73, '\r'};
  CHECK(voltronic::parse_reply(nak, sizeof nak, text) && text == "NAK");   // the refusal of a command has this well-known CRC
  const std::uint8_t wrong[] = {'(', 'N', 'A', 'K', 0x73, 0x74, '\r'};
  CHECK(!voltronic::parse_reply(wrong, sizeof wrong, text));               // and one with a wrong CRC is not accepted
  // the framer finds a reply in noise and restarts on a '('
  voltronic::ReplyFramer framer;
  std::vector<std::uint8_t> frame;
  int found = 0;
  const std::vector<std::uint8_t> noise{0x00, 0xFF, 'x', '\r'};
  for (std::uint8_t b : noise) CHECK(!framer.feed(b, frame));
  std::vector<std::uint8_t> stream{'(', 'B', 'B'};
  stream.insert(stream.end(), good.begin(), good.end());
  for (std::uint8_t b : stream) if (framer.feed(b, frame)) { ++found; CHECK(frame == good); }
  CHECK(found == 1);
  // a runaway frame is dropped
  for (int i = 0; i < 400; ++i) framer.feed('a', frame);
  CHECK(!framer.feed('\r', frame));
}

static const char* kPwr =
    "@\r\n"
    "Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time                 B.V.St   B.T.St\r\n"
    "1     49872  -1280  22000  20000  25000  3330   3348   Dischg   Normal   Normal   Normal   88%      2018-11-14 15:12:04  Normal   Normal\r\n"
    "2     49870  -1310  21500  19500  24500  3328   3349   Dischg   Normal   Normal   Normal   87%      2018-11-14 15:12:04  Normal   Normal\r\n"
    "3     -      -      -      -      -      -      -      Absent   -        -        -        -        -                    -        -\r\n"
    "Command completed successfully\r\n"
    "$$\r\npylon>";

static void test_pylontech_console() {
  std::vector<pylontech::Module> modules;
  CHECK(pylontech::parse_pwr(kPwr, modules) == 3);
  CHECK(modules[0].present && modules[0].number == 1 && modules[0].voltage_v == 49.872 && modules[0].current_a == -1.28 && modules[0].temperature_c == 22.0);
  CHECK(modules[0].temperature_low_c == 20.0 && modules[0].temperature_high_c == 25.0 && modules[0].cell_low_v == 3.33 && modules[0].cell_high_v == 3.348);
  CHECK(modules[0].soc_percent == 88 && modules[0].base_state == "Dischg" && modules[0].voltage_state == "Normal");
  CHECK(modules[1].soc_percent == 87 && modules[1].current_a == -1.31);
  CHECK(!modules[2].present && modules[2].number == 3 && modules[2].base_state == "Absent");
  const pylontech::Stack s = pylontech::summarise(modules);
  CHECK(s.modules == 2 && s.voltage_v > 49.870 && s.voltage_v < 49.872 && s.current_a < -2.58 && s.current_a > -2.60);
  CHECK(s.temperature_min_c == 19.5 && s.temperature_max_c == 25.0 && s.cell_low_v == 3.328 && s.cell_high_v == 3.349);
  CHECK(s.soc_percent == 88 && s.state == "discharging" && !s.alarm);   // the mean of 88 and 87 is 87.5, which rounds up
  // one module reporting a state that is not normal is an alarm
  std::string alarm = kPwr;
  alarm.replace(alarm.find("Normal   Normal   Normal   88%"), 8, "OverVol ");
  CHECK(pylontech::parse_pwr(alarm, modules) == 3 && pylontech::summarise(modules).alarm);
  // nothing useful: no modules, and no crash on odd text
  CHECK(pylontech::parse_pwr("", modules) == 0 && pylontech::parse_pwr("Command completed successfully\r\n$$\r\npylon>", modules) == 0);
  CHECK(pylontech::parse_pwr("1 abc def 3 4 5 6 7 Idle Normal Normal Normal 50%", modules) == 0);
  CHECK(pylontech::parse_pwr("99 1 2 3 4 5 6 7 Idle Normal Normal Normal 50%", modules) == 0);   // not a module number
  CHECK(pylontech::parse_pwr("1 50000 0 20000 20000 20000 3330 3330 Idle Normal Normal Normal 250%", modules) == 1 && modules[0].soc_percent == -1);
  CHECK(pylontech::summarise({}).modules == 0 && pylontech::summarise({}).soc_percent == -1);
}

static void test_pylontech_frames() {
  // the two checks of a frame, worked by hand: a length of 2 has a check of E, and the frame's checksum is the two's complement of the sum of its characters
  CHECK(pylontech::length_field(2) == "E002" && pylontech::length_field(0) == "0000" && pylontech::length_field(0x2A) == "402A");
  const std::string frame = pylontech::build_frame(0x20, 0x02, 0x46, 0x42, "02");
  CHECK(frame.size() == 1 + 14 + 4 + 1 && frame[0] == '~' && frame.back() == '\r' && frame.substr(1, 14) == "20024642E00202");
  unsigned sum = 0;
  for (char c : frame.substr(1, 14)) sum += static_cast<unsigned char>(c);
  CHECK(frame.substr(15, 4) == pylontech::hex((~sum + 1) & 0xFFFF, 4));
  pylontech::Frame parsed;
  CHECK(pylontech::parse_frame(frame, parsed) && parsed.version == 0x20 && parsed.address == 2 && parsed.cid1 == 0x46 && parsed.cid2 == 0x42 && parsed.info == "02");
  const std::string with_info = pylontech::build_frame(0x20, 0x01, 0x46, 0x00, "0102030405AB");
  CHECK(pylontech::parse_frame(with_info, parsed) && parsed.info == "0102030405AB");
  CHECK(pylontech::parse_frame(pylontech::build_frame(0x20, 0x01, 0x46, 0x00, ""), parsed) && parsed.info.empty());
  // damage is refused: a changed character, a changed length, a missing end, lowercase, a wrong start
  std::string damaged = frame;
  damaged[6] = '7';
  CHECK(!pylontech::parse_frame(damaged, parsed));
  std::string wrong_length = frame;
  wrong_length[9] = 'F';
  CHECK(!pylontech::parse_frame(wrong_length, parsed));
  CHECK(!pylontech::parse_frame(frame.substr(0, frame.size() - 1), parsed) && !pylontech::parse_frame("^" + frame.substr(1), parsed) && !pylontech::parse_frame("", parsed));
  std::string lower = with_info;
  lower[13] = 'a';
  CHECK(!pylontech::parse_frame(lower, parsed));
}

static const char* kBat =
    "@\r\n"
    "Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  Coulomb\r\n"
    "0        3324     -1281    22000    Dischg       Normal       Normal       Normal       88%\r\n"
    "1        3325     -1281    22000    Dischg       Normal       Normal       Normal       88%\r\n"
    "2        3323     -1281    22000    Dischg       Normal       Normal       Normal       88%\r\n"
    "3        3330     -1281    22000    Dischg       Normal       Normal       Normal       88%\r\n"
    "4        3329     -1281    22000    Dischg       Normal       Normal       Normal       88%\r\n"
    "5        3326     -1281    22500    Dischg       Normal       Normal       Normal       88%\r\n"
    "6        3327     -1281    22500    Dischg       Normal       Normal       Normal       88%\r\n"
    "7        3325     -1281    22500    Dischg       Normal       Normal       Normal       88%\r\n"
    "8        3324     -1281    22500    Dischg       Normal       Normal       Normal       88%\r\n"
    "9        3348     -1281    22500    Dischg       Normal       Normal       Normal       88%\r\n"
    "10       3326     -1281    23000    Dischg       Normal       Normal       Normal       88%\r\n"
    "11       3325     -1281    23000    Dischg       Normal       Normal       Normal       88%\r\n"
    "12       3324     -1281    23000    Dischg       Normal       Normal       Normal       88%\r\n"
    "13       3327     -1281    23000    Dischg       Normal       Normal       Normal       88%\r\n"
    "14       3326     -1281    23000    Dischg       Normal       Normal       Normal       88%\r\n"
    "Command completed successfully\r\n$$\r\npylon>";

static const char* kInfo =
    "@\r\n"
    "Device address      : 1\r\n"
    "Manufacturer        : Pylon\r\n"
    "Device name         : US3000C\r\n"
    "Board version       : PHILTEC_BOARD_V2\r\n"
    "Remain Capacity     : 65100 mAH\r\n"
    "Total Capacity      : 74000 mAH\r\n"
    "Cycle Times         : 312\r\n"
    "Command completed successfully\r\n$$\r\npylon>";

static void test_pylontech_cells_and_capacity() {
  std::vector<double> cells;
  CHECK(pylontech::parse_bat(kBat, cells) == 15);
  CHECK(cells[0] == 3.324 && cells[9] == 3.348 && cells[14] == 3.326);
  // a table with a gap, a cell out of range or nothing at all gives no cells
  CHECK(pylontech::parse_bat("0 3324 0 0 A B C D 1%\r\n2 3324 0 0 A B C D 1%\r\n", cells) == 0);
  CHECK(pylontech::parse_bat("0 99999 0 0 A B C D 1%\r\n", cells) == 0);
  CHECK(pylontech::parse_bat("", cells) == 0 && pylontech::parse_bat("Command completed successfully", cells) == 0);
  std::vector<pylontech::Module> modules;
  pylontech::parse_pwr(kPwr, modules);
  pylontech::parse_bat(kBat, cells);
  CHECK(pylontech::attach_cells(modules, 1, cells) && modules[0].cells_v.size() == 15);
  CHECK(!pylontech::attach_cells(modules, 3, cells) && !pylontech::attach_cells(modules, 9, cells));   // an absent module and one that is not there
  // the console's `info`: the model, the remaining and the total capacity, the cycles
  CHECK(pylontech::parse_info(kInfo, modules[0]) == 4);
  CHECK(modules[0].model == "US3000C" && modules[0].capacity_ah == 65.1 && modules[0].full_capacity_ah == 74.0 && modules[0].cycles == 312);
  pylontech::Module none;
  CHECK(pylontech::parse_info("", none) == 0 && pylontech::parse_info("Remain Capacity : abc mAH\r\nCycle Times : -4\r\n", none) == 0 && none.capacity_ah < 0 && none.cycles < 0);
  pylontech::Module ah;
  CHECK(pylontech::parse_info("Remain Capacity : 50 Ah\r\n", ah) == 1 && ah.capacity_ah == 50.0);
  // the second module has its own numbers; the stack adds the capacities (the modules are in parallel), takes the most used cycle count and the cells' extremes
  pylontech::parse_info("Device name : US3000C\r\nRemain Capacity : 60000 mAH\r\nTotal Capacity : 74000 mAH\r\nCycle Times : 340\r\n", modules[1]);
  const pylontech::Stack s = pylontech::summarise(modules);
  CHECK(s.capacity_ah > 125.09 && s.capacity_ah < 125.11 && s.full_capacity_ah == 148.0 && s.cycles == 340 && s.model == "US3000C");
  CHECK(s.energy_kwh > 6.2 && s.energy_kwh < 6.3);                     // 125.1 Ah at about 49.87 V
  CHECK(s.cell_low_v == 3.323 && s.cell_high_v == 3.349);              // the cells' own lowest of module 1 (3.323) and the second module's Vhigh column (3.349)
  const std::string json_text = battery_json("solar-1", "us3000-1", 5, modules);
  armor::json::Value doc;
  CHECK(armor::json::parse(json_text, doc) && doc.get("model")->text == "US3000C" && doc.get("cycles")->number == 340 && doc.get("full_capacity_ah")->number == 148.0);
  const armor::json::Value* stack = doc.get("stack");
  CHECK(stack != nullptr && stack->items.size() == 2 && stack->items[0].get("cells_v")->items.size() == 15 && stack->items[0].get("cycles")->number == 312);   // the empty place after the last battery is not sent
  CHECK(stack->items[1].get("cells_v") == nullptr);
}

static void test_pylontech_real_formats() {
  // the newer firmware (US5000 V2.3): Id columns after the extremes, MOSFET temperature and a system alarm state
  const std::string newer =
      "@\r\nPower Volt   Curr   Tempr  Tlow   Tlow.Id  Thigh  Thigh.Id  Vlow   Vlow.Id  Vhigh  Vhigh.Id  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time                 B.V.St   B.T.St   MosTempr  M.T.St  SysAlarm.St\r\n"
      "1     50200  -1000  21000  20000  4        22000  9         3340   3        3350   11        Charge   Normal   Normal   Normal   88%      2023-05-06 10:11:12  Normal   Normal   23000     Normal  Normal\r\n"
      "2     50190  1500   21500  20500  2        22500  8         3338   1        3352   5         Charge   Normal   Normal   Normal   87%      2023-05-06 10:11:12  Normal   Normal   23500     Normal  Normal\r\n"
      "3     -      -      -      -      -        -      -         -      -        -      -         Absent   -        -        -        -        -                    -        -        -         -       -\r\n";
  std::vector<pylontech::Module> modules;
  CHECK(pylontech::parse_pwr(newer, modules) == 3 && modules[0].present && !modules[2].present);
  CHECK(modules[0].voltage_v == 50.2 && modules[0].current_a == -1.0 && modules[0].temperature_low_c == 20.0 && modules[0].temperature_high_c == 22.0 && modules[0].cell_low_v == 3.34 && modules[0].cell_high_v == 3.35);
  CHECK(modules[0].soc_percent == 88 && modules[0].base_state == "Charge" && modules[0].system_alarm_state == "Normal" && modules[0].temperatures_c.size() == 2 && modules[0].temperatures_c[1] == 23.0);
  CHECK(modules[1].current_a == 1.5 && modules[1].cell_high_v == 3.352 && !pylontech::summarise(modules).alarm);
  std::string alarmed = newer;
  const std::string needle = "Normal  Normal\r\n2";
  alarmed.replace(alarmed.find(needle), needle.size(), "Normal  Alarm\r\n2");
  CHECK(pylontech::parse_pwr(alarmed, modules) == 3 && modules[0].system_alarm_state == "Alarm" && pylontech::summarise(modules).alarm);
  // the older firmware without B.V.St and B.T.St, and no header at all
  CHECK(pylontech::parse_pwr("1 49872 -1280 22000 20000 25000 3330 3348 Dischg Normal Normal Normal 88% 2018-11-14 15:12:04\r\n", modules) == 1 && modules[0].cell_voltage_state.empty() && modules[0].soc_percent == 88);
  CHECK(pylontech::parse_pwr("Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb Time\r\n1 49872 -1280 22000 20000 25000 3330 3348 Dischg Normal Normal Normal 88% 2018-11-14 15:12:04\r\n", modules) == 1 && modules[0].cell_high_v == 3.348);
  // a row cut short, or a header without the columns the reading needs
  CHECK(pylontech::parse_pwr("1 49872 -1280 22000\r\n", modules) == 0);
  CHECK(pylontech::parse_pwr("Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb\r\n1 49872 -1280 22000 20000 25000 3330 3348 Dischg Normal Normal Normal\r\n", modules) == 0);

  // `bat`: the newer firmware adds the charge in mAH and the balancing flag
  std::vector<double> cells;
  double remaining = 0;
  int balancing = -1;
  std::string bat = "@\r\nBattery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  SOC      Coulomb      BAL\r\n";
  for (int i = 0; i < 15; ++i) bat += std::to_string(i) + " " + std::to_string(3340 + i) + " -1000 22000 Dischg Normal Normal Normal 88% 66550 mAH " + (i == 4 || i == 9 ? "Y" : "N") + "\r\n";
  CHECK(pylontech::parse_bat(bat, cells, &remaining, &balancing) == 15 && remaining > 66.54 && remaining < 66.56 && balancing == 2);
  CHECK(pylontech::parse_bat(kBat, cells, &remaining, &balancing) == 15 && remaining < 0 && balancing == 0);   // the older rows have neither

  // `info` of a real module: the rated capacity comes from the Specification; `stat` gives the cycles
  pylontech::Module m;
  const std::string info = "@\r\nDevice address      : 1\r\nManufacturer        : Pylon\r\nDevice name         : US5000\r\nMain Soft version   : B66.6\r\nSpecification       : 48V/74AH\r\nCell Number         : 15\r\n";
  CHECK(pylontech::parse_info(info, m) == 2 && m.model == "US5000" && m.full_capacity_ah == 74.0 && m.capacity_ah < 0);
  CHECK(pylontech::parse_info("CHARGE Cnt.         : 400\r\nCYCLE Times         : 312\r\n", m) == 2 && m.cycles == 312);
  pylontech::Module both;
  CHECK(pylontech::parse_info("Total Capacity : 74000 mAH\r\nSpecification : 48V/50AH\r\n", both) == 1 && both.full_capacity_ah == 74.0);   // the measured one wins
  pylontech::Module odd;
  CHECK(pylontech::parse_info("Specification : /\r\nSpecification : 48V\r\nSpecification : 48V/AH\r\nSpecification : 48V/0AH\r\n", odd) == 0 && odd.full_capacity_ah < 0);
}

// What real batteries printed on their console (captured with the maker's own program on units of the same models: US2000C of four firmware versions, an
// US2KBPL, an US3000C and a 24 V unit). The console echoes every line followed by an empty one, and some units leave fields empty. Serial numbers are replaced.
static std::string echoed(const std::string& lines) {
  std::string out;
  for (char c : lines) { out += c; if (c == '\n') out += "\r\n"; }
  return out;
}

static void test_pylontech_real_captures() {
  const std::string us2000c =
      "@\r\nDevice address      : 1\r\nManufacturer        : Pylon\r\nDevice name         : US2000C\r\nBoard version       : V10R04\r\nBoard               : NF4.E2\r\n"
      "Main Soft version   : B69.12.0\r\nSoft  version       : V1.3\r\nBoot  version       : V1.0\r\nComm version        : V2.0\r\nRelease Date        : 21-12-27\r\n"
      "Barcode             : K000000C00000000\r\nSpecification       : 48V/50AH\r\nCell Number         : 15\r\nMax Dischg Curr     : -90000mA\r\nMax Charge Curr     : 90000mA\r\n"
      "EPONPort rate       : 1200\r\nConsole Port rate   : 115200\r\nCommand completed successfully\r\n$$\r\npylon_debug>\r\n";
  pylontech::Module m;
  CHECK(pylontech::parse_info(echoed(us2000c), m) == 2 && m.model == "US2000C" && m.full_capacity_ah == 50.0);
  // an US2KBPL (what a "US2000B" says of itself) and an US3000C with empty fields
  const std::string us2kbpl = "@\r\nDevice address      : 1\r\nManufacturer        : Pylon\r\nDevice name         : US2KBPL\r\nBoard version       : PHANTOMSAV10R03\r\nMain Soft version   : B65.12\r\n"
                              "Soft  version       : V2.3\r\nBarcode             : PPT000000000000\r\n\r\nSpecification       : 48V/50AH\r\nCell Number         : 15\r\nConsole Port rate   : 115200\r\nCommand completed successfully\r\n";
  pylontech::Module b;
  CHECK(pylontech::parse_info(echoed(us2kbpl), b) == 2 && b.model == "US2KBPL" && b.full_capacity_ah == 50.0);
  const std::string us3000c = "@\r\nDevice address      : 1\r\nManufacturer        : \r\nDevice name         : US3000C\r\nBoard version       : \r\nMain Soft version   : B68.8.0\r\n"
                              "Barcode             :                 \r\nSpecification       : 48V/74AH\r\nCell Number         : 15\r\nMax Dischg Curr     : -90000mA\r\nCommand completed successfully\r\n";
  pylontech::Module c;
  CHECK(pylontech::parse_info(echoed(us3000c), c) == 2 && c.model == "US3000C" && c.full_capacity_ah == 74.0);
  // `stat`: the counters of the battery's life; the cycles are "CYCLE Times". A newer firmware adds lines that end in "Hours" and a line without a colon opens the list.
  const std::string stat =
      "@\r\nDevice address           1\r\nData Items      :     1938\r\nCharge Times    :    49299\r\nBat OV Times    :      755\r\nSOH Times       :        0\r\nCYCLE Times     :     1344\r\n"
      "Pwr Percent     :       42\r\nPwr Coulomb     : 147752280\r\nDsg Cap         : 149287073\r\nHT Cnt          :        0 Hours\r\nCurrent Greater 70A Hours:        0 Hours\r\n"
      "System Error Times:        0\r\nCommand completed successfully\r\n$$\r\npylon_debug>\r\n";
  CHECK(pylontech::parse_info(echoed(stat), c) == 10 && c.cycles == 1344 && c.model == "US3000C" && c.health_percent == 55);   // 147752280 mAs of 74 Ah (266400000)
  pylontech::Module worn;
  worn.full_capacity_ah = 50.0;
  CHECK(pylontech::parse_info("Pwr Coulomb     : 100856900\r\nCYCLE Times     :      576\r\n", worn) == 2 && worn.health_percent == 56 && worn.cycles == 576);
  pylontech::Module newer;
  newer.full_capacity_ah = 50.0;
  CHECK(pylontech::parse_info("Pwr Coulomb     : 180000000\r\n", newer) == 1 && newer.health_percent == 100);
  pylontech::Module over;
  over.full_capacity_ah = 50.0;
  CHECK(pylontech::parse_info("Pwr Coulomb     : 200000000\r\n", over) == 1 && over.health_percent == 100);       // more than the rating is still 100
  pylontech::Module unrated, garbage;
  CHECK(pylontech::parse_info("Pwr Coulomb     : 180000000\r\n", unrated) == 0 && unrated.health_percent < 0);      // no rated capacity known: no health
  garbage.full_capacity_ah = 50.0;
  CHECK(pylontech::parse_info("Pwr Coulomb     : abc\r\nPwr Coulomb     : 0\r\nPwr Coulomb     : 1234567890123\r\n", garbage) == 0 && garbage.health_percent < 0);
  // the summary takes the mean of the modules that know theirs, and the message says it
  std::vector<pylontech::Module> pair(2);
  for (int i = 0; i < 2; ++i) { pair[i].number = i + 1; pair[i].present = true; pair[i].voltage_v = 50; pair[i].health_percent = i == 0 ? 100 : 56; }
  CHECK(pylontech::summarise(pair).health_percent == 78);
  pair[1].health_percent = -1;
  CHECK(pylontech::summarise(pair).health_percent == 100 && pylontech::summarise({}).health_percent < 0);
  pylontech::Module fresh;
  CHECK(pylontech::parse_info(echoed("@\r\nCYCLE Times     :        0\r\nPwr Coulomb     : 180000000\r\nCommand completed successfully\r\n"), fresh) == 1 && fresh.cycles == 0);
  // a bat table with fewer columns (index, voltage, temperature and two states) is read too
  std::vector<double> few;
  CHECK(pylontech::parse_bat("@\r\nBattery Volt Tempr Volt.St Temp.St\r\n0 3558 26700 Normal Normal\r\n1 3470 26700 Normal Normal\r\nCommand completed successfully\r\n", few) == 2 && few[1] == 3.47);
  // a console that does not know the command answers with a text and no completion line: nothing is read
  pylontech::Module none;
  CHECK(pylontech::parse_info("@\r\nInvalid command or fail to excute.\r\nUsage:\r\nShow recorded data - datalist [event/history]\r\n$$\r\npylon_debug>\r\n", none) == 0);
}

// The data plate of a module (serial number, Board, chip...), with INVENTED serial numbers: the chip follows the maker's rules and is "unknown" whenever they cannot decide.
static std::string plate_text(const std::string& model, const std::string& board, const std::string& serial) {
  std::string out = "@\r\nDevice address      : 1\r\nDevice name         : " + model + "\r\nBoard version       : V10R04\r\n";
  if (!board.empty()) out += "Board               : " + board + "\r\n";
  out += "Main Soft version   : B68.8.0.0\r\nSoft  version       : V2.4\r\nBoot  version       : V2.1\r\nComm version        : V2.0\r\nRelease Date        : 00-01-01\r\n";
  out += "Barcode             : " + serial + "\r\nSpecification       : 48V/74AH\r\nCell Number         : 15\r\nCommand completed successfully\r\n$$\r\npylon>";
  return out;
}
static std::string chip_told(const std::string& model, const std::string& board, const std::string& serial) {
  pylontech::DataPlate plate;
  return pylontech::read_plate(plate_text(model, board, serial), plate) ? std::string(plate.chip) : std::string("none");
}
static void test_pylontech_data_plate() {
  const std::string newer = "TEST00AE20000001", older = "TEST000XX0000001";
  pylontech::DataPlate plate;
  CHECK(pylontech::read_plate(plate_text("US3000C", "NF4.E2", newer), plate));
  CHECK(plate.serial == newer && plate.board == "NF4.E2" && std::string(plate.chip) == "newer" && plate.specification == "48V/74AH");
  CHECK(plate.boot_version == "V2.1" && plate.comm_version == "V2.0" && plate.release_date == "00-01-01");
  CHECK(chip_told("US3000C", "", older) == "older" && chip_told("US2000C", "ST1", older) == "older");
  // the serial number and the Board do not tell one story, or one of them is missing: it cannot be told
  CHECK(chip_told("US3000C", "NF4.E2", older) == "unknown" && chip_told("US3000C", "", newer) == "unknown" && chip_told("US3000C", "XYZ", older) == "unknown");
  CHECK(chip_told("US3000C", "NF4.E2", "") == "unknown" && chip_told("US3000C", "", "SHORT") == "unknown" && chip_told("US2000", "", older) == "unknown");
  pylontech::DataPlate blank;
  CHECK(pylontech::read_plate(plate_text("US3000C", "", "                "), blank) && blank.serial.empty() && blank.board.empty() && std::string(blank.chip) == "unknown");
  pylontech::DataPlate odd;
  CHECK(pylontech::read_plate(plate_text("US3000C", "NF4 E2", "TEST 0000 0001"), odd) && odd.serial.empty() && odd.board.empty() && std::string(odd.chip) == "unknown");
  pylontech::DataPlate untouched;
  CHECK(!pylontech::read_plate("@\r\nInvalid command or fail to excute.\r\n$$\r\npylon>", untouched) && untouched.chip == nullptr);
  // the message says the plate of the module, and nothing about one whose `info` was not read
  std::vector<pylontech::Module> modules(2);
  for (int i = 0; i < 2; ++i) { modules[i].number = i + 1; modules[i].present = true; modules[i].voltage_v = 50; }
  modules[0].plate = plate;
  const std::string text = battery_json("solar-1", "us3000-1", 1, modules);
  CHECK(text.find("\"serial\":\"TEST00AE20000001\"") != std::string::npos && text.find("\"chip\":\"newer\"") != std::string::npos && text.find("\"board\":\"NF4.E2\"") != std::string::npos);
  CHECK(text.find("\"serial\"") == text.rfind("\"serial\"") && text.find("\"chip\"") == text.rfind("\"chip\""));
}

// The charge left of a stack, with the two forms of the `bat` table: ampere-hours for each cell (a real US2000, whose cells differ) and percentages only.
static std::string us_console(const std::string& command, int modules, const std::string& bat, bool info_answers) {
  std::string out = "@\r\n";
  if (command == "pwr 1") {   // the sheet of one module (invented figures, the shape of a captured one)
    out += " Power  1\r\n Voltage         : 38286       mV\r\n Power Events    : 0x0        \r\n System Fault    : 0x40       VOLT err \r\n";
  } else if (command == "pwr") {
    out += "Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb Time B.V.St B.T.St\r\n";
    for (int i = 1; i <= modules; ++i) out += std::to_string(i) + " 51688 0 36000 36000 36000 3347 3495 Charge Normal Normal Normal 52% 2025-03-09 19:11:40 Normal Normal\r\n";
  } else if (command.rfind("bat", 0) == 0) {
    return bat;
  } else if (command.rfind("info", 0) == 0 && info_answers) {
    out += "Device name : US2000C\r\nSpecification : 48V/50AH\r\nCell Number : 15\r\n";
  }
  return out + "Command completed successfully\r\n$$\r\npylon>";
}
static std::string us_cycle(Poller& poller, std::uint64_t& now, int modules, const std::string& bat, bool info_answers, std::vector<std::string>* asked = nullptr) {
  for (int turn = 0; turn < 4000; ++turn) {
    const std::vector<std::uint8_t> tx = poller.next_tx(now);
    now += 40;
    if (!tx.empty()) {
      std::string command(tx.begin(), tx.end());
      while (!command.empty() && command.front() == '\r') command.erase(0, 1);
      while (!command.empty() && command.back() == '\r') command.pop_back();
      if (asked != nullptr) asked->push_back(command);
      const std::string reply = us_console(command, modules, bat, info_answers);
      poller.on_rx(reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size(), now);
    }
    std::string topic, payload;
    if (poller.take_message(now, topic, payload)) return payload;
  }
  return "";
}
static void test_pylontech_remaining_capacity() {
  std::vector<double> cells;
  double remaining = 0;
  // a real US2000: the cells differ (26,039 to 30,541 mAh) and the module's charge is the lowest, which is its 52 % of 50 Ah
  CHECK(pylontech::parse_bat(kUs2000Bat, cells, &remaining) == 15 && remaining > 26.038 && remaining < 26.040);
  std::string percent_only = "@\r\nBattery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  Coulomb\r\n";
  for (int i = 0; i < 15; ++i) percent_only += std::to_string(i) + " 3425 0 36000 Charge Normal Normal Normal 60%\r\n";
  percent_only += "Command completed successfully\r\n$$\r\npylon>";
  CHECK(pylontech::parse_bat(percent_only, cells, &remaining) == 15 && remaining < 0);
  armor::json::Value document;
  for (const std::string& bat : {std::string(kUs2000Bat), percent_only}) {
    const bool has_mah = bat != percent_only;
    Poller poller(armor::config::Kind::kPylontech, "solar-1", "us2000-1", 5, 0);
    std::uint64_t now = 1000;
    CHECK(armor::json::parse(us_cycle(poller, now, 2, bat, true), document));
    const double each = has_mah ? 26.039 : 26.0;   // the cells' own figure, or 52 % of 50 Ah
    CHECK(document.get("capacity_ah") != nullptr && std::fabs(document.get("capacity_ah")->number - 2 * each) < 0.01);
    CHECK(document.get("full_capacity_ah") != nullptr && document.get("full_capacity_ah")->number == 100.0 && document.get("energy_kwh") != nullptr);
    CHECK(std::fabs(document.get("stack")->items[1].get("capacity_ah")->number - each) < 0.01);
  }
  // with no capacity told at all nothing is made up, and the module is asked again after five minutes, not half an hour; once told it is kept when `info` later stays silent
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "us2000-1", 5, 0);
  std::uint64_t now = 1000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, percent_only, false), document) && document.get("capacity_ah") == nullptr && document.get("full_capacity_ah") == nullptr);
  now += Poller::kExtrasRetryMs + 1000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, percent_only, true), document) && document.get("full_capacity_ah") != nullptr && document.get("capacity_ah") != nullptr);
  now += Poller::kExtrasRefreshMs + 1000;
  std::vector<std::string> asked;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, percent_only, false, &asked), document) && std::find(asked.begin(), asked.end(), "info 1") != asked.end());
  CHECK(document.get("full_capacity_ah") != nullptr && document.get("full_capacity_ah")->number == 50.0);
  // sixteen batteries with the setting at 0: every one is asked for its cells and counts in the totals
  Poller tall(armor::config::Kind::kPylontech, "solar-1", "us2000-1", 5, 0);
  now = 1000;
  asked.clear();
  CHECK(armor::json::parse(us_cycle(tall, now, 16, percent_only, true, &asked), document) && std::find(asked.begin(), asked.end(), "bat 16") != asked.end());
  CHECK(document.get("full_capacity_ah") != nullptr && document.get("full_capacity_ah")->number == 800.0 && document.get("stack")->items[15].get("cells_v") != nullptr);
  // a port limited to eight does not ask the ninth: no charge total, and the full capacity counts it as the others, which are all alike
  Poller limited(armor::config::Kind::kPylontech, "solar-1", "us2000-1", 5, 8);
  now = 1000;
  CHECK(armor::json::parse(us_cycle(limited, now, 9, percent_only, true), document) && document.get("full_capacity_ah")->number == 450.0 && document.get("capacity_ah") == nullptr);
  CHECK(document.get("stack")->items[8].get("cells_v") == nullptr && document.get("stack")->items[8].get("full_capacity_ah") == nullptr);
}

// A stack of eight 50 Ah batteries in which some modules' `info` is not answered (or is refused) when the whole stack is first read.
static std::string sheet_answer(const std::string& command, std::uint32_t without_info, bool refuse) {
  std::string out = "@\r\n";
  if (command == "pwr") {
    out += "Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb Time B.V.St B.T.St\r\n";
    for (int i = 1; i <= 8; ++i) out += std::to_string(i) + " 49872 -1280 22000 20000 25000 3330 3348 Dischg Normal Normal Normal 88% 2018-11-14 15:12:04 Normal Normal\r\n";
  } else if (command.rfind("bat", 0) == 0) {
    for (int i = 0; i < 15; ++i) out += std::to_string(i) + " 3340 -1000 22000 Dischg Normal Normal Normal 88% 44000 mAH N\r\n";
  } else if (command.rfind("info ", 0) == 0) {
    const int n = std::atoi(command.c_str() + 5);
    if (without_info & (1u << n)) return refuse ? "@\r\nInvalid command or fail to excute.\r\n$$\r\npylon>" : "";
    out += "Device name : US2000C\r\nSpecification : 48V/50AH\r\nCell Number : 15\r\n";
  } else if (command.rfind("stat ", 0) == 0) {
    out += "CYCLE Times : 120\r\n";
  }
  return out + "Command completed successfully\r\n$$\r\npylon>";
}
static std::string sheet_cycle(Poller& poller, std::uint64_t& now, std::uint32_t without_info, bool refuse, std::vector<std::string>* asked = nullptr) {
  for (int turn = 0; turn < 8000; ++turn) {
    const std::vector<std::uint8_t> tx = poller.next_tx(now);
    now += 40;
    if (!tx.empty()) {
      std::string command(tx.begin(), tx.end());
      while (!command.empty() && command.front() == '\r') command.erase(0, 1);
      while (!command.empty() && command.back() == '\r') command.pop_back();
      if (asked != nullptr) asked->push_back(command);
      const std::string reply = sheet_answer(command, without_info, refuse);
      if (!reply.empty()) poller.on_rx(reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size(), now);
    }
    std::string topic, payload;
    if (poller.take_message(now, topic, payload)) return payload;
  }
  return "";
}
static int modules_with_sheet(const armor::json::Value& document) {
  int n = 0;
  for (const armor::json::Value& m : document.get("stack")->items) if (m.get("full_capacity_ah") != nullptr) ++n;
  return n;
}
static int count_of(const std::vector<std::string>& asked, const std::string& prefix) {
  int n = 0;
  for (const std::string& a : asked) if (a.rfind(prefix, 0) == 0) ++n;
  return n;
}
static void test_pylontech_missing_sheets() {
  armor::json::Value document;
  for (const bool refuse : {true, false}) {
    // only modules 1 and 2 answer `info` at first: the total counts the six others as those two
    const std::uint32_t six = 0x1F8u;   // bits 3 to 8
    Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-1", 5, 0);
    std::uint64_t now = 1000;
    CHECK(armor::json::parse(sheet_cycle(poller, now, six, refuse), document) && modules_with_sheet(document) == 2);
    CHECK(document.get("full_capacity_ah") != nullptr && document.get("full_capacity_ah")->number == 400.0 && document.get("capacity_ah")->number == 352.0);
    // once the battery answers, one missing module is asked in each of the next cycles: all eight within a minute, not after half an hour
    const std::uint64_t from = now;
    for (int cycle = 0; cycle < 6; ++cycle) {
      now += 5000;
      std::vector<std::string> asked;
      CHECK(armor::json::parse(sheet_cycle(poller, now, 0, refuse, &asked), document) && count_of(asked, "info ") == 1 && count_of(asked, "data ") == 0);
      CHECK(modules_with_sheet(document) == 3 + cycle);
    }
    CHECK(now - from < 60000 && modules_with_sheet(document) == 8 && document.get("full_capacity_ah")->number == 400.0);
    now += 5000;
    std::vector<std::string> asked;
    CHECK(!sheet_cycle(poller, now, 0, refuse, &asked).empty() && count_of(asked, "info ") == 0);
  }
  // one module that never answers: the other seven give the total, the module itself says nothing it did not say, and it is not asked at every cycle for ever
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-2", 5, 0);
  std::uint64_t now = 1000;
  std::vector<std::string> asked;
  std::string last;
  for (int cycle = 0; cycle < 10; ++cycle) { last = sheet_cycle(poller, now, 1u << 7, true, &asked); now += 5000; }
  CHECK(armor::json::parse(last, document) && modules_with_sheet(document) == 7 && document.get("stack")->items[6].get("full_capacity_ah") == nullptr);
  CHECK(document.get("full_capacity_ah") != nullptr && document.get("full_capacity_ah")->number == 400.0);
  CHECK(count_of(asked, "info 7") == Poller::kExtrasQuickTries);
  now += Poller::kExtrasRetryMs;
  asked.clear();
  CHECK(!sheet_cycle(poller, now, 1u << 7, true, &asked).empty() && count_of(asked, "info 7") == 1);
  // different capacities or models: nothing is assumed
  std::vector<pylontech::Module> mixed(3);
  for (auto& m : mixed) { m.present = true; m.voltage_v = 50; m.model = "US2000C"; m.full_capacity_ah = 50; }
  mixed[2].full_capacity_ah = -1;
  mixed[2].model.clear();
  CHECK(pylontech::summarise(mixed).full_capacity_ah == 150.0 && pylontech::summarise(mixed).full_estimated);
  mixed[2].model = "US3000C";
  CHECK(pylontech::summarise(mixed).full_capacity_ah < 0);
  mixed[2].model = "US2000C";
  mixed[1].full_capacity_ah = 74;
  CHECK(pylontech::summarise(mixed).full_capacity_ah < 0);
}

// An answer that comes after its question timed out is not taken as the answer to the next one: the next question waits for the line to be quiet.
static void test_pylontech_late_answer() {
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-1", 5, 1);
  std::uint64_t now = 1000;
  const auto feed = [&](const std::string& text) { poller.on_rx(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), now); };
  const auto next = [&]() { const std::vector<std::uint8_t> tx = poller.next_tx(now); return std::string(tx.begin(), tx.end()); };
  CHECK(next().find("pwr") != std::string::npos);
  feed(sheet_answer("pwr", 0, false));
  CHECK(next() == "bat 1\r");
  feed(sheet_answer("bat 1", 0, false));
  CHECK(next() == "info 1\r");
  now += Poller::kConsoleExtraTimeoutMs - 50;
  feed("@\r\nDevice name : US2000C\r\n");   // the answer has begun, and is slow
  now += 60;
  CHECK(next().empty());                    // timed out, and the console is still talking: `stat` waits
  now += 100;
  feed("Specification : 48V/50AH\r\nCommand completed successfully\r\n$$\r\npylon>");
  CHECK(next().empty());
  now += Poller::kQuietMs;
  CHECK(next() == "stat 1\r");
}

// `pwrsys`. The capture is the one published by guybw/PylontechToESPHome (MIT, Copyright (c) 2023-2026 guybw), tests/fixtures/pwrsys.txt: eight modules, taken in its debug mode.
static const char* kPwrsys =
    "pwrsys\r\n@\r\n Power System Information\r\n ---------------------------------\r\n System is discharging\r\n Total Num                : 8\r\n Present Num              : 8\r\n Sleep Num                : 0\r\n"
    " System Volt              : 50012    mV\r\n System Curr              : -5760    mA\r\n System RC                : 396225   mAH\r\n System FCC               : 400104   mAH\r\n"
    " System SOC               : 99       %\r\n System SOH               : 95       %\r\n Highest voltage          : 3337     mV\r\n Recommend chg voltage    : 53250    mV\r\n Recommend dsg voltage    : 45000    mV\r\n"
    " Recommend chg current    : 14800    mA\r\n Recommend dsg current    : -37000   mA\r\n system Recommend chg voltage: 53250    mV\r\n system Recommend dsg voltage: 45000    mV\r\n"
    " system Recommend chg current: 84800    mA\r\n system Recommend dsg current: -212000  mA\r\nCommand completed successfully\r\n$$\r\npylon>";

static void test_pylontech_pwrsys() {
  pylontech::SystemSheet s;
  CHECK(pylontech::parse_pwrsys(kPwrsys, s) && s.valid && s.modules_total == 8 && s.modules_present == 8 && s.modules_sleep == 0 && s.soc_percent == 99 && s.health_percent == 95);
  CHECK(s.remaining_ah > 396.224 && s.remaining_ah < 396.226 && s.full_capacity_ah > 400.103 && s.full_capacity_ah < 400.105);
  CHECK(s.charge_voltage_v > 53.249 && s.charge_voltage_v < 53.251 && s.discharge_voltage_v == 45.0 && s.charge_current_a > 84.79 && s.charge_current_a < 84.81 && s.discharge_current_a == 212.0);
  pylontech::SystemSheet kept = s;
  CHECK(!pylontech::parse_pwrsys("pwrsys\r\nInvalid command or fail to excute.\r\n$$\r\npylon>", kept) && !pylontech::parse_pwrsys("Power System Information\r\n Total Num : 8\r\n", kept) && kept.soc_percent == 99);
  // a firmware with only one module's limits gives the voltages and no current
  CHECK(pylontech::parse_pwrsys("Power System Information\r\n System SOC : 50 %\r\n System SOH : 300 %\r\n Recommend chg voltage : 53250 mV\r\n Recommend chg current : 14800 mA\r\n", s));
  CHECK(s.soc_percent == 50 && s.health_percent < 0 && s.charge_voltage_v > 53 && s.charge_current_a < 0 && s.remaining_ah < 0);

  // every line the sheet lacks is left out of the message, and numbers no stack can have are not taken
  armor::json::Writer bare;
  bare.begin_object();
  write_system(bare, pylontech::SystemSheet{});
  bare.end_object();
  CHECK(bare.str() == "{\"bms_system\":{}}");
  CHECK(pylontech::parse_pwrsys("Power System Information\n\tSystem SOC : 10 %\n Total Num : 99\n Present Num : -1\n Sleep Num : x\n System RC : -5 mAH\n System FCC :\n system Recommend chg voltage: 2000000 mV\n Recommend dsg voltage : 45000 mV\n system Recommend dsg current: 99000000 mA\n no colon here\n", s));
  CHECK(s.soc_percent == 10 && s.modules_total < 0 && s.modules_present < 0 && s.modules_sleep < 0 && s.remaining_ah < 0 && s.full_capacity_ah < 0 && s.charge_voltage_v < 0 && s.discharge_voltage_v == 45.0 && s.discharge_current_a < 0);

  // a battery that stays silent to `pwrsys` still gives its reading, and is asked again only half an hour after the second silence
  {
    Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-1", 5, 1);
    poller.set_extended(true);
    std::uint64_t now = 1000;
    int asked = 0, readings = 0;
    const auto run = [&](std::uint64_t until) {
      for (; now < until; now += 40) {
        const std::vector<std::uint8_t> tx = poller.next_tx(now);
        if (!tx.empty()) {
          std::string command(tx.begin(), tx.end());
          while (!command.empty() && command.front() == '\r') command.erase(0, 1);
          while (!command.empty() && command.back() == '\r') command.pop_back();
          if (command == "pwrsys") ++asked;
          else {
            const std::string answer = sheet_answer(command, 0, false);
            poller.on_rx(reinterpret_cast<const std::uint8_t*>(answer.data()), answer.size(), now);
          }
        }
        std::string topic, payload;
        if (poller.take_message(now, topic, payload)) ++readings;
      }
    };
    run(120000);
    CHECK(asked == 2 && readings > 5);
    now += Poller::kExtendedRarelyMs;
    run(now + 60000);
    CHECK(asked >= 3);
  }

  // the poller asks it after the modules, puts it in the message, and leaves alone a battery that refuses it twice
  for (const bool knows : {true, false}) {
    Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-1", 5, 1);
    poller.set_extended(true);
    std::uint64_t now = 1000;
    int asked = 0;
    std::string last;
    for (int turn = 0; turn < 3000; ++turn) {
      const std::vector<std::uint8_t> tx = poller.next_tx(now);
      now += 40;
      if (!tx.empty()) {
        std::string command(tx.begin(), tx.end());
        while (!command.empty() && command.front() == '\r') command.erase(0, 1);
        while (!command.empty() && command.back() == '\r') command.pop_back();
        const std::string answer = command == "pwrsys" ? (knows ? std::string(kPwrsys) : std::string("@\r\nInvalid command or fail to excute.\r\n$$\r\npylon>")) : sheet_answer(command, 0, false);
        if (command == "pwrsys") ++asked;
        poller.on_rx(reinterpret_cast<const std::uint8_t*>(answer.data()), answer.size(), now);
      }
      std::string topic, payload;
      if (poller.take_message(now, topic, payload)) last = payload;
    }
    CHECK(!last.empty() && (last.find("\"bms_system\":{\"modules_total\":8") != std::string::npos) == knows);
    CHECK(knows ? asked > 2 : asked == 2);
  }
}

// End of charge ("High") is not an alarm; a protection, the system alarm column and a word never seen are. A `bat` table that lost bytes on the line is not taken.
static void test_pylontech_alarm_levels() {
  CHECK(!pylontech::state_alarms("High", false) && !pylontech::state_alarms("Low", false) && !pylontech::state_alarms("Dischg", false) && !pylontech::state_alarms("", false));
  CHECK(pylontech::state_alarms("OV", false) && pylontech::state_alarms("UnderVol", false) && pylontech::state_alarms("Protect", false) && pylontech::state_alarms("Whatever", false) && pylontech::state_alarms("High", true));
  std::vector<pylontech::Module> modules;
  pylontech::parse_pwr(kPwr, modules);
  // a full battery just after an update: SOC 100, cells 3.508 to 3.536 V, the cells column reading High
  modules[0].soc_percent = 100; modules[0].cell_low_v = 3.508; modules[0].cell_high_v = 3.536; modules[0].cell_voltage_state = "High";
  CHECK(!pylontech::summarise(modules).alarm);
  modules[0].cell_voltage_state = ""; modules[0].cell_low_v = 3.330; modules[0].cell_high_v = 3.348;
  modules[0].voltage_state = "High";
  CHECK(!pylontech::summarise(modules).alarm);
  modules[0].voltage_state = "OverVol";
  CHECK(pylontech::summarise(modules).alarm);
  modules[0].voltage_state = "Normal";
  // the lowest and highest cell of this row are 3.330 and 3.348 V: a cell read as 1.497 V comes from a spoiled table, a real weak cell would be in the row too
  CHECK(pylontech::cells_match_row(modules[0], {3.330, 3.348, 3.340}) && !pylontech::cells_match_row(modules[0], {3.348, 1.497}) && pylontech::cells_match_row(modules[0], {3.348, 0.0})   /* a 0 V cell has its own rule: held once, then told (test_pylontech_cells_not_read) */);
  modules[0].cell_low_v = 1.497;
  CHECK(pylontech::cells_match_row(modules[0], {3.348, 1.497}));
}

static void test_messages() {
  CHECK(topic("perimetro-1", "axpert-1") == "armor/solar/perimetro-1/axpert-1/state");
  CHECK(topic("Bad", "x").empty() && topic("a", "").empty() && topic("a", "b/c").empty() && topic("-a", "b").empty() && topic("a", std::string(33, 'x')).empty());
  voltronic::Status s;
  CHECK(voltronic::parse_qpigs("232.0 50.0 230.0 50.0 0161 0119 003 460 57.50 012 100 0069 0014 103.8 57.45 00000 00110110 00 00 00856", s));
  const std::string inverter = inverter_json("solar-1", "axpert-1", 123456, 'L', s, {"line_fail"});
  armor::json::Value document;
  CHECK(armor::json::parse(inverter, document) && document.is_object());
  CHECK(document.get("kind")->text == "inverter" && document.get("mode")->text == "line" && document.get("device")->text == "axpert-1");
  CHECK(document.get("pv_w")->number == 856 && document.get("battery_a")->number == 12.0 && document.get("warnings")->items.size() == 1);
  CHECK(document.get("ac_charging")->boolean == false && document.get("pv_charging")->boolean == true);
  CHECK(armor::json::parse(inverter_json("solar-1", "axpert-1", 1, 0, s, {}), document) && document.get("mode")->text == "unknown" && document.get("warnings")->items.empty());
  std::vector<pylontech::Module> modules;
  pylontech::parse_pwr(kPwr, modules);
  const std::string battery = battery_json("solar-1", "us3000-1", 99, modules);
  CHECK(armor::json::parse(battery, document) && document.get("kind")->text == "battery" && document.get("modules")->number == 2 && document.get("soc_percent")->number == 88);
  CHECK(document.get("state")->text == "discharging" && document.get("stack")->items.size() == 2 && document.get("alarm")->boolean == false);
  // the console's sixteen places: only up to the last battery is sent; an empty place between two is kept, and so is one a battery was seen in before
  {
    std::vector<pylontech::Module> places(16);
    for (int i = 0; i < 16; ++i) places[static_cast<std::size_t>(i)].number = i + 1;
    places[0].present = places[2].present = true;
    CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 1, places), document) && document.get("modules")->number == 2);
    CHECK(document.get("stack")->items.size() == 3 && !document.get("stack")->items[1].get("present")->boolean && document.get("stack")->items[2].get("n")->number == 3);
    places[2].present = false;
    CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 2, places, 3), document) && document.get("stack")->items.size() == 3 && !document.get("stack")->items[2].get("present")->boolean);
    CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 3, places), document) && document.get("stack")->items.size() == 1);
    places[0].present = false;
    CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 4, places), document) && document.get("modules")->number == 0 && document.get("stack")->items.empty());
  }
  CHECK(armor::json::parse(battery_json("solar-1", "us3000-1", 99, {}), document) && document.get("modules")->number == 0 && document.get("state") == nullptr);
}

// The counters of `stat`: the texts below are written the way the consoles of the three families print them (the same lines as the captures above, with other numbers); no real battery was
// asked for these. The meaning of a counter is not official; what is tested is that the name, the number and the message are right.
static long long counter(const pylontech::Module& m, const char* key) {
  for (const auto& item : m.counters) if (item.first == key) return item.second;
  return -1;
}
static void test_pylontech_stat_counters() {
  const std::string us2000b =
      "@\r\nDevice address           1\r\nData Items      :     2411\r\nHisData Items   :      905\r\nCharge Times    :    61822\r\nShut Times      :        3\r\nReset Times     :       12\r\nIdle Times      :     4410\r\n"
      "Bat OV Times    :        0\r\nBat HV Times    :       17\r\nBat LV Times    :        0\r\nBat UV Times    :        2\r\nPwr OV Times    :        0\r\nPwr UV Times    :        0\r\nInput OV Times  :        0\r\n"
      "COC Times       :        1\r\nDOC Times       :        0\r\nSC Times        :        0\r\nCOT Times       :        0\r\nDOT Times       :        4\r\nCUT Times       :        0\r\nDUT Times       :        0\r\n"
      "SOH Times       :        0\r\nCYCLE Times     :      812\r\nPwr Percent     :       64\r\nDsg Cap         : 88123456\r\nBMICERR Times   :        0\r\nRV Times        :        0\r\n"
      "LifeWarn Times  :        0\r\nLifeAlarm Times :        0\r\nHT Cnt          :        0 Hours\r\nLT Cnt          :        5 Hours\r\nHT@0.5C Cnt     :        0 Hours\r\nCommand completed successfully\r\n$$\r\npylon>\r\n";
  pylontech::Module b;
  CHECK(pylontech::parse_info(echoed(us2000b), b) > 20);
  CHECK(counter(b, "bat_hv_times") == 17 && counter(b, "bat_uv_times") == 2 && counter(b, "dot_times") == 4 && counter(b, "coc_times") == 1 && counter(b, "bat_ov_times") == 0);
  CHECK(counter(b, "data_items") == 2411 && counter(b, "charge_times") == 61822 && counter(b, "lt_cnt") == 5 && counter(b, "dsg_cap") == 88123456 && counter(b, "pwr_percent") == 64);
  CHECK(counter(b, "cycle_times") == -1 && b.cycles == 812);   // the cycles keep their own field
  CHECK(b.counters.size() == 30);
  // the severe ones are marked, the ones of use and of the temperature sensors' hours are not
  int severe = 0;
  for (const pylontech::StatCounterInfo& item : pylontech::stat_catalog()) if (item.severe) ++severe;
  CHECK(severe == 17);
  // a US3000C and a US5000 print fewer or other lines: only what is in the catalog is kept, whatever else is there
  const std::string us3000c = "@\r\nDevice address           1\r\nData Items      :     1938\r\nCharge Times    :    49299\r\nBat OV Times    :      755\r\nSOH Times       :        0\r\nCYCLE Times     :     1344\r\nPwr Percent     :       42\r\n"
                              "Unknown Thing   :        9\r\nSystem Error Times:        0\r\nCurrent Greater 70A Hours:        3 Hours\r\nCommand completed successfully\r\n$$\r\npylon_debug>\r\n";
  pylontech::Module c;
  CHECK(pylontech::parse_info(echoed(us3000c), c) > 0 && counter(c, "bat_ov_times") == 755 && counter(c, "system_error_times") == 0 && counter(c, "unknown_thing") == -1 && c.cycles == 1344);
  const std::string us5000 = "@\r\nDevice address           2\r\nData Items      :      311\r\nBat HV Times    :        0\r\nCOC2 Times      :        1\r\nDOC2 Times      :        0\r\nCOCA Times      :        0\r\nDOCA Times      :        0\r\n"
                             "CHT Times       :        0\r\nCLT Times       :        0\r\nDHT Times       :        0\r\nDLT Times       :        0\r\nPwr HV Times    :        0\r\nPwr LV Times    :        0\r\nBat SLP Times   :        0\r\nCYCLE Times     :       96\r\n"
                             "Command completed successfully\r\n$$\r\npylon>\r\n";
  pylontech::Module d;
  CHECK(pylontech::parse_info(echoed(us5000), d) > 10 && counter(d, "coc2_times") == 1 && counter(d, "bat_slp_times") == 0 && d.cycles == 96);
  // a value that is not a number, too long or negative is skipped; the same counter twice keeps the last
  pylontech::Module e;
  CHECK(pylontech::parse_info("Bat OV Times    :  abc\r\nBat HV Times    :  -3\r\nBat UV Times    :  12345678901\r\nCOC Times : 4\r\nCOC Times : 6\r\n", e) == 2 && counter(e, "bat_ov_times") == -1 && counter(e, "bat_hv_times") == -1 && counter(e, "bat_uv_times") == -1 && counter(e, "coc_times") == 6);
  // the key of every catalog line is a name the contract accepts, and none repeats
  for (const pylontech::StatCounterInfo& item : pylontech::stat_catalog()) {
    const std::string key = item.key;
    CHECK(!key.empty() && key.size() <= 32 && key[0] >= 'a' && key[0] <= 'z');
    for (char ch : key) CHECK((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_');
    int same = 0;
    for (const pylontech::StatCounterInfo& other : pylontech::stat_catalog()) if (key == other.key || std::string(item.console) == other.console) ++same;
    CHECK(same == 1);
  }
  // the message carries them under the module, only for a module that is there
  std::vector<pylontech::Module> modules(1);
  modules[0] = b; modules[0].number = 1; modules[0].present = true; modules[0].voltage_v = 50;
  armor::json::Value document;
  CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 99, modules), document));
  const armor::json::Value* counters = document.get("stack")->items[0].get("counters");
  CHECK(counters != nullptr && counters->is_object() && counters->get("bat_hv_times")->number == 17 && counters->get("dsg_cap")->number == 88123456);
  modules[0].counters.clear();
  CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 99, modules), document) && document.get("stack")->items[0].get("counters") == nullptr);
}

// The answers of a real battery's console, as the maker's documentation shows them (tests/pylontech_real.hpp, unchanged): `bat 1`, `soh 1`, `stat 1` and `data event 1`.
static void test_pylontech_real_console_answers() {
  CHECK(pylontech::console_done(kRealBat) && pylontech::console_done(kRealSoh) && pylontech::console_done(kRealStat) && pylontech::console_done(kRealDataEvent));
  // bat: fourteen cells, in volts
  std::vector<double> cells;
  double remaining = -1;
  int balancing = -1;
  CHECK(pylontech::parse_bat(kRealBat, cells, &remaining, &balancing) == 15 || pylontech::parse_bat(kRealBat, cells, &remaining, &balancing) == 14);
  CHECK(cells.size() >= 14 && cells.size() <= 15);
  const double last = cells.back();
  CHECK(last > 3.30 && last < 3.31);
  CHECK(remaining > 33.4 && remaining < 33.5);   // 33424 mAH
  // stat: the counters of the life of the battery
  pylontech::Module m;
  CHECK(pylontech::parse_info(kRealStat, m) >= 28);
  CHECK(counter(m, "data_items") == 628 && counter(m, "hisdata_items") == 2044 && counter(m, "charge_cnt") == 0 && counter(m, "discharge_cnt") == 761 && counter(m, "charge_times") == 35859);
  CHECK(counter(m, "status_cnt") == 760 && counter(m, "idle_times") == 38704 && counter(m, "bat_ov_times") == 44 && counter(m, "bat_hv_times") == 83 && counter(m, "bat_lv_times") == 8);
  CHECK(counter(m, "pwr_ov_times") == 220 && counter(m, "pwr_hv_times") == 278 && counter(m, "pwr_lv_times") == 8 && counter(m, "pwr_uv_times") == 1 && counter(m, "shut_times") == 7);
  CHECK(counter(m, "reset_times") == 22 && counter(m, "soh_times") == 31 && counter(m, "pwr_percent") == 67 && counter(m, "dsg_cap") == 4542819 && counter(m, "lv_cnt") == 17416);
  CHECK(counter(m, "coc_times") == 0 && counter(m, "sc_times") == 0 && counter(m, "bmicerr_times") == 0 && counter(m, "lt_half_c_cnt") == 0 && m.cycles == 11);
  // soh: nothing in it is a counter or an event
  pylontech::Module other;
  pylontech::Event none;
  CHECK(pylontech::parse_info(kRealSoh, other) == 0 && !pylontech::parse_event(kRealSoh, none) && !pylontech::parse_event(kRealStat, none));
  // data event: the entry, the mnemonics as the console printed them
  pylontech::Event e;
  CHECK(pylontech::parse_event(kRealDataEvent, e) && e.valid);
  CHECK(e.index == 1 && e.time == "17-05-13 20:52:58" && e.voltage_v > 54.014 && e.voltage_v < 54.016 && e.current_a > 10.64 && e.current_a < 10.65 && e.temperature_c > 28.9 && e.temperature_c < 29.1 && e.percent == 100);
  CHECK(e.base_state == "Charge" && e.voltage_state == "High" && e.current_state == "Normal" && e.temperature_state == "Normal" && e.coulomb_state == "Full");
  CHECK(e.power_events == "0x2 PHV" && e.battery_events == "0x0" && e.system_fault == "0x0");
  // no index or no time: not an event
  pylontech::Event bad;
  CHECK(!pylontech::parse_event("Time : 17-05-13 20:52:58\r\nVoltage : 54015 mV\r\n", bad) && !pylontech::parse_event("Item Index : 3\r\nVoltage : 54015 mV\r\n", bad) && !pylontech::parse_event("", bad));
  // the message carries the last event of a module under its own words
  std::vector<pylontech::Module> modules(1);
  modules[0] = m; modules[0].number = 1; modules[0].present = true; modules[0].voltage_v = 54; modules[0].model = "US2000B"; modules[0].last_event = e;
  armor::json::Value document;
  CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 99, modules), document));
  const armor::json::Value* item = document.get("stack")->items[0].get("last_event");
  CHECK(item != nullptr && item->get("index")->number == 1 && item->get("power_events")->text == "0x2 PHV" && item->get("time")->text == "17-05-13 20:52:58");
  CHECK(document.get("stack")->items[0].get("model")->text == "US2000B");
}

// The console is woken at 1200 baud with this frame (the maker's documentation gives it as hexadecimal: 7E 32 30 30 31 34 36 38 32 43 30 30 34 38 35 32 30 46 43 43 33 0D).
static void test_pylontech_wake_frame() {
  CHECK(pylontech::build_frame(0x20, 0x01, 0x46, 0x82, "8520") == "~20014682C0048520FCC3\r");
  pylontech::Frame frame;
  CHECK(pylontech::parse_frame("~20014682C0048520FCC3\r", frame) && frame.cid1 == 0x46 && frame.cid2 == 0x82 && frame.info == "8520");
}

// The speed of a Pylontech port: the console always runs at 115200; 9600 is the speed of the RS485 protocol (the battery's DIP switch 1 on). Both are valid on a hardware port.
static void test_pylontech_port_speeds() {
  armor::config::Settings s;
  s.ports[0].kind = "pylontech";
  s.ports[0].baud = 0;
  CHECK(armor::config::effective_baud(s.ports[0]) == 115200);
  for (int baud : {9600, 115200}) {
    s.ports[0].baud = baud;
    bool refused = false;
    for (const auto& problem : armor::config::validate(s)) if (problem.path == "ports.0.baud") refused = true;
    CHECK(!refused);
  }
}

// The answers of a real US2000 (V1.5, prompt `pylon>`, tests/pylontech_us2000_real.hpp, unchanged): the header of `pwr` has a name of two words and the row a state of one, the date
// and the hour are two words, `getpwr` prints "value#", `data` prints the mnemonic after the hexadecimal number and `info` tells the limits and the speed of the console.
static void test_pylontech_real_us2000() {
  CHECK(pylontech::console_done(kUs2000Pwr) && pylontech::console_done(kUs2000Bat) && pylontech::console_done(kUs2000Data) && pylontech::console_done(kUs2000GetPwr) && pylontech::console_done(kUs2000Info) && pylontech::console_done(kUs2000Soh));
  CHECK(pylontech::console_done("pylon>pwr\r\n@\r\nPower Volt\r\nCommand completed successfully\r\n$$\r\n\r\npylon>") && pylontech::console_done("x\r\n$$\r\npylon_debug>\r\n"));
  CHECK(!pylontech::console_done("pylon>pwr\r") && !pylontech::console_done("@\r\nPower Volt Curr"));
  // pwr: ten rows, two modules there
  std::vector<pylontech::Module> modules;
  CHECK(pylontech::parse_pwr(kUs2000Pwr, modules) == 10);
  CHECK(modules.size() == 10 && modules[0].present && modules[1].present && !modules[2].present && !modules[9].present && modules[2].base_state == "Absent");
  CHECK(modules[0].voltage_v > 51.687 && modules[0].voltage_v < 51.689 && modules[0].soc_percent == 52 && modules[0].voltage_state == "OV" && modules[0].base_state == "Charge");
  CHECK(modules[0].cell_voltage_state == "Normal" && modules[0].cell_temperature_state == "Normal" && modules[0].cell_low_v > 3.346 && modules[0].cell_high_v < 3.496);
  CHECK(modules[1].current_a < -0.014 && modules[1].current_a > -0.016 && modules[1].soc_percent == 29 && modules[1].base_state == "Dischg" && modules[1].cell_high_v > 3.973);
  // the same table with the state of the battery's lifecycle in a column of its own on a header of two words, whatever its order
  std::vector<pylontech::Module> shifted;
  CHECK(pylontech::parse_pwr("Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb LC. St Time B.V.St B.T.St\r\n1 51000 0 30000 30000 30000 3400 3450 Charge Normal Normal Normal 50% ON 2025-03-09 19:11:40 High Normal\r\n", shifted) == 1 && shifted[0].cell_voltage_state == "High" && shifted[0].cell_temperature_state == "Normal");
  // bat: fifteen cells from 0, the weak one is the seventh
  std::vector<double> cells;
  double remaining = -1;
  int balancing = -1;
  CHECK(pylontech::parse_bat(kUs2000Bat, cells, &remaining, &balancing) == 15 && cells[6] > 3.345 && cells[6] < 3.347 && cells[10] > 3.497 && remaining > 26.03 && remaining < 26.04);
  // info: identity, the limits of the module and the speed of the console of an old unit
  pylontech::Module m;
  pylontech::parse_info(kUs2000Info, m);
  CHECK(m.model == "US2000" && m.full_capacity_ah == 50.0 && m.cell_count == 15 && m.max_charge_a > 52.9 && m.max_charge_a < 53.1 && m.max_discharge_a > 54.9 && m.max_discharge_a < 55.1 && m.console_baud == 19200);
  pylontech::Module odd;
  pylontech::parse_info("Cell Number : 99\r\nMax Charge Curr : abc\r\nMax Dischg Curr : 0mA\r\nConsole Port rate : 7\r\n", odd);
  CHECK(odd.cell_count == -1 && odd.max_charge_a < 0 && odd.max_discharge_a < 0 && odd.console_baud == -1);
  // soh: the weak cell is the one the battery calls Abnormal; a count alone does not make a cell abnormal
  pylontech::Module health;
  CHECK(pylontech::parse_soh(kUs2000Soh, health) == 15 && health.soh_counts.size() == 15);
  CHECK(health.soh_counts[6] == 13 && health.soh_counts[10] == 12 && health.soh_counts[0] == 0 && health.soh_abnormal.size() == 1 && health.soh_abnormal[0] == 6);
  // the numbering is the console's, from 0, the same as the `bat` table's: the cell the battery calls abnormal here is cells_v[6], the lowest of the pack, which a person calls cell 7
  std::vector<double> pack;
  CHECK(pylontech::parse_bat(kUs2000Bat, pack) == 15 && std::min_element(pack.begin(), pack.end()) - pack.begin() == health.soh_abnormal[0]);
  pylontech::Module health_doc;
  CHECK(pylontech::parse_soh(kRealSoh, health_doc) == 15 && health_doc.soh_abnormal.empty());   // the documentation's table, printed with dashes
  pylontech::Module none;
  CHECK(pylontech::parse_soh(kUs2000Pwr, none) == 0 && none.soh_counts.empty() && pylontech::parse_soh("", none) == 0);
  CHECK(pylontech::parse_soh("Battery Voltage SOHCount SOHStatus\r\n0 3400 0 Normal\r\n2 3400 0 Normal\r\n", none) == 0);   // a cell missing: the table is not trusted
  // getpwr: the whole snapshot in one answer
  pylontech::PowerSnapshot snapshot;
  CHECK(pylontech::parse_getpwr(kUs2000GetPwr, snapshot));
  CHECK(snapshot.voltage_v > 51.70 && snapshot.voltage_v < 51.702 && snapshot.current_a == 0 && snapshot.temperature_c == 36.0 && snapshot.lowest_cell_ah > 26.03 && snapshot.lowest_cell_ah < 26.04);
  CHECK(snapshot.base_state == "Charge" && snapshot.voltage_state == "OV" && snapshot.current_state == "Normal" && snapshot.temperature_state == "Normal");
  CHECK(snapshot.cells_v.size() == 15 && snapshot.cells_v[6] > 3.344 && snapshot.cells_v[6] < 3.346 && snapshot.cell_temperatures_c[0] == 36.0 && snapshot.cell_states[0] == "Normal Normal");
  pylontech::PowerSnapshot nothing;
  CHECK(!pylontech::parse_getpwr(kUs2000Pwr, nothing) && !pylontech::parse_getpwr("", nothing) && !pylontech::parse_getpwr("abc# 1# 2# 3# a# b# c# d#\r\n", nothing));
  // data with no argument: one record, the mnemonic after the hexadecimal number, the old names of the protections in one list
  pylontech::Event e;
  CHECK(pylontech::parse_event(kUs2000Data, e) && e.index == 754 && e.time == "25-03-09 19:01:42" && e.voltage_v > 51.75 && e.voltage_v < 51.76 && e.percent == 52);
  CHECK(e.voltage_state == "OV" && e.coulomb_state == "Normal" && e.power_events == "0x0" && e.battery_events == "0x4 BVNOR" && e.system_fault == "0x0");
  // the two firmwares print the same record in their own words: both read
  pylontech::Event newer;
  CHECK(pylontech::parse_event(kRealDataEvent, newer) && newer.power_events == "0x2 PHV" && newer.battery_events == "0x0");
  // the message carries what the module told about itself
  std::vector<pylontech::Module> stack(1);
  stack[0] = m; stack[0].number = 1; stack[0].present = true; stack[0].voltage_v = 51.7; stack[0].soh_counts = health.soh_counts; stack[0].soh_abnormal = health.soh_abnormal;
  armor::json::Value document;
  CHECK(armor::json::parse(battery_json("solar-1", "us2000-1", 99, stack), document));
  const armor::json::Value& first = document.get("stack")->items[0];
  CHECK(first.get("cell_count")->number == 15 && first.get("max_charge_a")->number == 53 && first.get("max_discharge_a")->number == 55 && first.get("console_baud")->number == 19200);
  CHECK(first.get("soh_counts")->items.size() == 15 && first.get("soh_counts")->items[6].number == 13 && first.get("soh_abnormal")->items.size() == 1 && first.get("soh_abnormal")->items[0].number == 6);
  // the speed an old unit tells is a speed a port may be set to
  armor::config::Settings s;
  s.ports[0].kind = "pylontech";
  s.ports[0].baud = 19200;
  bool refused = false;
  for (const auto& problem : armor::config::validate(s)) if (problem.path == "ports.0.baud") refused = true;
  CHECK(!refused);
}

// Real captures of the logs of other batteries (tests/pylontech_logs_real.hpp, the serial number removed): `info` with the board and the software, a worn battery's `stat` with high counters,
// and the tables of `datalist` (event and history).
static void test_pylontech_real_logs() {
  pylontech::Module m;
  pylontech::parse_info(kRealInfoUs2000c, m);
  CHECK(m.model == "US2000C" && m.board_version == "V10R04" && m.main_soft_version == "B68.2.0.0" && m.soft_version == "V2.6" && m.cell_count == 15 && m.max_charge_a == 90 && m.max_discharge_a == 90 && m.console_baud == 115200);
  CHECK(m.full_capacity_ah == 50.0);
  pylontech::Module worn;
  pylontech::parse_info(kRealStatWorn, worn);
  CHECK(counter(worn, "bat_ov_times") == 2428 && counter(worn, "bat_hv_times") == 530 && counter(worn, "coca_times") == 3 && counter(worn, "soh_times") == 237 && counter(worn, "data_items") == 1919 && counter(worn, "misc_data_items") == -1 && worn.cycles == 29);
  pylontech::DataList list;
  CHECK(pylontech::parse_datalist(kRealDatalistEvent, list) == 20 && list.rows.size() == 20 && list.save_every_s == -1);
  CHECK(list.rows[0].item == 0 && list.rows[0].date == "25-10-28" && list.rows[0].time == "16:27:27" && list.rows[0].voltage_v > 50.908 && list.rows[0].voltage_v < 50.91 && list.rows[0].current_a == 0 && list.rows[0].temperature_c == 30.0);
  CHECK(list.rows[0].cell_low_v > 3.377 && list.rows[0].cell_low_v < 3.379 && list.rows[0].cell_high_v > 3.494 && list.rows[0].base_state == "Charge" && list.rows[0].voltage_state == "Normal" && list.rows[0].percent == 89 && list.rows[0].events == "BHV" && !list.rows[0].history);
  CHECK(list.rows[1].events == "BOV" && list.rows[1].current_a > 13.68 && list.rows[1].current_a < 13.69);
  CHECK(list.rows[15].item == 15 && list.rows[15].events == "BOV");   // the first row after the "Press [Enter]" line is read too
  CHECK(list.rows[19].item == 19);
  pylontech::DataList history;
  CHECK(pylontech::parse_datalist(kRealDatalistHistory, history) == 6 && history.save_every_s == 1800);
  CHECK(history.rows[0].history && history.rows[0].error_code == "0x0" && history.rows[0].mosfet_state == "Normal" && history.rows[0].percent == 89 && history.rows[0].voltage_v > 50.5 && history.rows[0].voltage_v < 50.6 && history.rows[0].current_a > 3.9 && history.rows[0].time == "18:18:04");
  CHECK(history.rows[4].base_state == "Dischg" && history.rows[4].current_a < -1.8);
  pylontech::DataList nothing;
  CHECK(pylontech::parse_datalist("", nothing) == 0 && pylontech::parse_datalist(kRealStatWorn, nothing) == 0 && nothing.save_every_s == -1);
  CHECK(pylontech::parse_datalist("1 25-10-28 16:27:41 51549 13683 31000 29000 29000 3407 99999 Charge Normal Normal Normal 89% BOV\r\n", nothing) == 0);   // a cell of 99.999 V is not a row
  // the counters of a worn battery: a rise of the severe ones is what the server tells (see the server's tests); here the parsed values are the ones a rise is judged by
  pylontech::Module again = worn;
  pylontech::parse_info("Bat OV Times    :     2429\r\n", again);
  CHECK(counter(again, "bat_ov_times") == 2429 && counter(again, "bat_hv_times") == 530);
}

// A console that waits for the module it was asked about shows no prompt until the answer arrives, and drops it when the next question comes. A module that answers only after six
// seconds gives nothing within the usual time, and its sheet the first time it is asked again, which waits longer.
static void test_pylontech_slow_module() {
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "bank-1", 5, 0);
  std::uint64_t now = 1000;
  std::string held, last;
  std::uint64_t held_at = 0;
  int cycles = 0, asked_info = 0;
  armor::json::Value document;
  for (int turn = 0; turn < 8000 && cycles < 2; ++turn) {
    const std::vector<std::uint8_t> tx = poller.next_tx(now);
    now += 40;
    if (!tx.empty()) {
      std::string command(tx.begin(), tx.end());
      while (!command.empty() && command.front() == '\r') command.erase(0, 1);
      while (!command.empty() && command.back() == '\r') command.pop_back();
      held.clear();   // a new question ends the wait for the last one
      std::string reply = sheet_answer(command, 0, false);
      if (command == "info 7") { ++asked_info; held = reply; held_at = now + 6000; reply = "info 7\n\r"; }
      if (!reply.empty()) poller.on_rx(reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size(), now);
    }
    if (!held.empty() && now >= held_at) { poller.on_rx(reinterpret_cast<const std::uint8_t*>(held.data()), held.size(), now); held.clear(); }
    std::string topic, payload;
    if (poller.take_message(now, topic, payload)) {
      ++cycles;
      CHECK(armor::json::parse(payload, document) && modules_with_sheet(document) == (cycles == 1 ? 7 : 8));
    }
  }
  CHECK(cycles == 2 && asked_info == 2);
}

// Why a port that hears bytes reads nothing.
static void test_port_diagnosis() {
  namespace diag = armor::solar::portdiag;
  const std::vector<std::uint8_t> sent = {'\r', 'p', 'w', 'r', '\r'};
  CHECK(std::string(diag::classify({}, sent, true)).empty());
  CHECK(std::string(diag::classify(std::vector<std::uint8_t>(40, 0x00), sent, true)) == "flat");
  CHECK(std::string(diag::classify(std::vector<std::uint8_t>(40, 0xFF), sent, false)) == "flat");
  std::vector<std::uint8_t> echo = sent;
  echo.insert(echo.end(), sent.begin(), sent.end());
  CHECK(std::string(diag::classify(echo, sent, true)) == "echo");
  CHECK(std::string(diag::classify({0x9A, 0x13, 0xE0, 0x81, 0xC4, 0x05, 'a', 0x00}, sent, true)) == "noise");
  CHECK(std::string(diag::classify({'H', 'e', 'l', 'l', 'o', '\r', '\n'}, sent, true)) == "text");
  CHECK(std::string(diag::classify({0x9A, 0x13, 0xE0, 0x81}, {0x01, 0x03}, false)) == "frames");
  CHECK(diag::sample_hex({1, 2, 3, 4, 5, 6, 7, 0xAB}) == "03 04 05 06 07 AB" && diag::sample_hex({0}) == "00" && diag::sample_hex({}).empty());
  CHECK(diag::log_line(3, "flat", 115200, 16, 15, false, 230, "00 00") == "port 3 unreadable flat 115200bd rx16 tx15 n230 [00 00]");
  CHECK(diag::log_line(10, "frames", 1000000, 48, 47, true, 4294967295u, "00 00 00 00 00 00") == "port 10 unreadable frames 1000000bd rx48 tx47 inv n4294967295 [00 00 00 00 00 00]");
  // it fits the 96 characters of the health report with the log's tag in front
  CHECK(std::string("armor-ports: ").size() + diag::log_line(10, "frames", 1000000, 48, 47, true, 4294967295u, "00 00 00 00 00 00").size() <= 96);

  diag::LogSchedule schedule;
  CHECK(schedule.take(2, 0));                                         // at once, even at time zero
  CHECK(!schedule.take(3, 1000));                                     // another port waits its turn
  CHECK(schedule.take(3, diag::LogSchedule::kGapMs));
  CHECK(!schedule.take(2, diag::LogSchedule::kGapMs * 2));            // not repeated before five minutes
  CHECK(schedule.take(2, diag::LogSchedule::kRepeatMs));
  schedule.clear(2);
  CHECK(schedule.take(2, diag::LogSchedule::kRepeatMs + diag::LogSchedule::kGapMs) && !schedule.take(99, 99999999));

  // ten unreadable ports take turns: every one is said, never two closer than the gap
  diag::LogSchedule ten;
  std::uint32_t said = 0;
  std::uint64_t last_said = 0;
  for (std::uint64_t t = 0; t <= 30ULL * 60ULL * 1000ULL; t += 1000) {
    for (std::size_t port = 0; port < 10; ++port) {
      if (!ten.take(port, t)) continue;
      CHECK(said == 0 || t - last_said >= diag::LogSchedule::kGapMs);
      said |= 1u << port;
      last_said = t;
    }
  }
  CHECK(said == 0x3FFu);
  // any bytes get one of the five words, and the sample never grows
  std::uint32_t seed = 12345;
  const auto random = [&]() { seed = seed * 1103515245u + 12345u; return static_cast<std::uint8_t>(seed >> 16); };
  for (int round = 0; round < 500; ++round) {
    std::vector<std::uint8_t> heard(static_cast<std::size_t>(random()) + 1), asked(static_cast<std::size_t>(random() % 8));
    for (auto& b : heard) b = round % 5 == 0 ? static_cast<std::uint8_t>(random() & 1 ? 0xFF : 0x00) : random();
    for (auto& b : asked) b = random();
    const std::string word = diag::classify(heard, asked, round % 2 == 0);
    CHECK(word == "flat" || word == "echo" || word == "noise" || word == "text" || word == "frames");
    CHECK(diag::sample_hex(heard).size() <= diag::kSampleBytes * 3 - 1);
  }
  // an inverter port that hears rubbish and a Modbus port that never gets a frame
  for (const auto& which : {std::make_pair(armor::config::Kind::kVoltronic, std::string("noise")), std::make_pair(armor::config::Kind::kModbus, std::string("frames"))}) {
    Poller bad(which.first, "solar-1", "device-1", 5, 0);
    std::uint64_t at = 1000;
    const std::uint8_t rubbish[4] = {0x9A, 0x13, 0xE0, 0x81};
    for (int turn = 0; turn < 8000 && bad.stats().timeouts + bad.stats().replies_bad < 4; ++turn) {
      const std::vector<std::uint8_t> tx = bad.next_tx(at);
      at += 40;
      if (!tx.empty()) bad.on_rx(rubbish, sizeof rubbish, at);
    }
    CHECK(bad.unreadable_cause(at) == which.second);
  }

  // the console of a battery that is not the first of its stack (invented figures): it is said so, and it is not read as a stack of one
  const std::string slave_sheet =
      "pwr\n\r@\r\r\n ----------------------------\r\r\n Power  4\r\r\n\r\r\n Voltage         : 50100       mV\r\r\n Current         : -1500       mA\r\r\n Temperature     : 21000       mC\r\r\n"
      " Coulomb         : 77          %\r\r\n Total Coulomb   : 50000       mAH\r\r\n Basic Status    : Dischg  \r\r\n ----------------------------\r\n\rCommand completed successfully\r\n\r$$\r\n\rpylon>";
  CHECK(armor::solar::pylontech::parse_pwr_slave(slave_sheet) == 4);
  {
    const armor::solar::pylontech::SlaveSheet said = armor::solar::pylontech::read_slave_sheet(slave_sheet);
    CHECK(said.place == 4 && said.current_known && said.current_ma == -1500 && said.quiet);
    std::string trouble = slave_sheet;
    trouble.replace(trouble.find("Dischg  "), 8, "Protect ");
    CHECK(!armor::solar::pylontech::read_slave_sheet(trouble).quiet);       // a status that has not been seen counts as an alarm
    CHECK(armor::solar::pylontech::read_slave_sheet("").place == 0);
  }
  CHECK(armor::solar::pylontech::parse_pwr_slave("Power 4\r\nBattery Voltage SOHCount SOHStatus\r\n0 3300 0 Normal\r\n") == 0);
  CHECK(armor::solar::pylontech::parse_pwr_slave("Power 99\r\nVoltage : 50000 mV\r\n") == 0 && armor::solar::pylontech::parse_pwr_slave("Voltage : 50000 mV\r\n") == 0 && armor::solar::pylontech::parse_pwr_slave("") == 0);
  {
    Poller slave(armor::config::Kind::kPylontech, "solar-1", "battery-6", 5, 0);
    std::uint64_t at = 1000;
    std::string answer = slave_sheet, topic, payload;
    const auto run = [&](std::uint32_t bad) {
      for (int turn = 0; turn < 2000 && slave.stats().replies_bad < bad; ++turn) {
        const std::vector<std::uint8_t> tx = slave.next_tx(at);
        at += 40;
        if (!tx.empty()) slave.on_rx(reinterpret_cast<const std::uint8_t*>(answer.data()), answer.size(), at);
      }
    };
    run(1);
    CHECK(slave.unreadable_cause(at) == "slave" && slave.slave_module() == 4 && slave.state(at) == "garbled" && !slave.take_message(at, topic, payload));
    // the cable moved to the first battery: the table comes back and the notice goes
    at += 6000;
    std::string table = sheet_answer("pwr", 0, false);
    for (int turn = 0; turn < 50 && slave.slave_module() != 0; ++turn) {
      const std::vector<std::uint8_t> tx = slave.next_tx(at);
      at += 40;
      if (!tx.empty()) slave.on_rx(reinterpret_cast<const std::uint8_t*>(table.data()), table.size(), at);
    }
    CHECK(slave.slave_module() == 0 && slave.unreadable_cause(at).empty());
  }
  CHECK(diag::log_line(6, "slave", 115200, 2, 1, false, 900, "", 4) == "port 6 unreadable slave 115200bd rx2 tx1 mod4 n900 []");
  CHECK(std::string("armor-ports: ").size() + diag::log_line(10, "slave", 1000000, 48, 47, true, 4294967295u, "", 16).size() <= 96);

  // a battery port that hears only zeros
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "battery-3", 10, 0);
  std::uint64_t now = 1000;
  for (int turn = 0; turn < 2000; ++turn) {
    const std::vector<std::uint8_t> tx = poller.next_tx(now);
    now += 40;
    const std::uint8_t zero = 0;
    if (!tx.empty()) { CHECK(poller.asking() == "pwr"); poller.on_rx(&zero, 1, now); }
    if (poller.stats().timeouts == 1) CHECK(poller.unreadable_cause(now).empty());   // one silence is not enough to say
    if (poller.stats().timeouts >= 3) break;
  }
  CHECK(poller.state(now) == "garbled" && poller.unreadable_cause(now) == "flat" && poller.heard_sample().compare(0, 5, "00 00") == 0);
  // a port that hears nothing is silent, not unreadable
  Poller silent(armor::config::Kind::kPylontech, "solar-1", "battery-4", 10, 0);
  for (std::uint64_t t = 1000; t < 60000; t += 40) silent.next_tx(t);
  CHECK(silent.unreadable_cause(60000).empty() && silent.heard_sample().empty());
}

// A `bat` table of fifteen cells, all well but the ones given: those read 0 V (invented figures).
static std::string bat_with_dead(std::uint32_t dead) {
  std::string out = "@\r\nBattery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  Coulomb\r\n";
  for (int i = 0; i < 15; ++i)
    out += std::to_string(i) + ((dead & (1u << i)) ? " 0 0 36000 Charge UV Normal Normal 60%\r\n" : " 3425 0 36000 Charge Normal Normal Normal 60%\r\n");
  return out + "Command completed successfully\r\n$$\r\npylon>";
}
static bool has_flag(const armor::json::Value& module, const char* column, const char* word, int cell) {
  const armor::json::Value* flags = module.get("flags");
  if (flags == nullptr) return false;
  for (const armor::json::Value& f : flags->items)
    if (f.string_or("column", "") == column && f.string_or("word", "") == word && (cell < 0 ? f.get("cell") == nullptr : f.get("cell") != nullptr && f.get("cell")->number == cell)) return true;
  return false;
}
static void test_pylontech_cells_not_read() {
  const std::uint32_t three = 0x38u;   // cells 3, 4 and 5 (from 0)
  armor::json::Value document;
  Poller poller(armor::config::Kind::kPylontech, "solar-1", "battery-1", 5, 0);
  std::uint64_t now = 1000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, bat_with_dead(0), true), document) && document.get("stack")->items[0].get("cells_unread") == nullptr);
  CHECK(document.get("alarm")->boolean == false);
  // the maker the model tells, when `info` names none
  CHECK(document.string_or("manufacturer", "") == "Pylon" && document.string_or("manufacturer_source", "") == "deduced");
  // one table with cells at 0 V may be a spoiled line: it is not published
  now += 6000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, bat_with_dead(three), true), document));
  CHECK(document.get("stack")->items[0].get("cells_unread") == nullptr && document.get("stack")->items[0].get("cells_v")->items[3].number > 3.4);
  now += 6000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, bat_with_dead(0), true), document) && document.get("stack")->items[0].get("cells_unread") == nullptr);
  // read as 0 V twice in a row: the cells are told as not read, the cells' own states with them, and the stack's lowest cell is not a silent 0
  now += 6000;
  us_cycle(poller, now, 1, bat_with_dead(three), true);
  now += 6000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, bat_with_dead(three), true), document));
  const armor::json::Value& module = document.get("stack")->items[0];
  CHECK(module.get("cells_unread") != nullptr && module.get("cells_unread")->items.size() == 3 && module.get("cells_unread")->items[0].number == 3 && module.get("cells_unread")->items[2].number == 5);
  CHECK(module.get("cells_v")->items.size() == 15 && module.get("cells_v")->items[4].number == 0);
  CHECK(has_flag(module, "voltage", "UV", 3) && has_flag(module, "voltage", "UV", 5) && !has_flag(module, "voltage", "UV", 6));
  CHECK(document.get("alarm")->boolean == true && std::fabs(document.get("cell_min_v")->number - 3.425) < 0.001);
  CHECK(has_flag(module, "system", "0x40 VOLT err", -1));   // the module is faulty: its own sheet was asked and the code goes with its states
  // and when the cells are read again the fault ends
  now += 6000;
  CHECK(armor::json::parse(us_cycle(poller, now, 1, bat_with_dead(0), true), document) && document.get("stack")->items[0].get("cells_unread") == nullptr);
}
static void test_pylontech_impossible_figures() {
  // a row whose last cell column ran into the next one, a temperature and a current beyond the contract: those figures are left out, the rest of the reading goes
  std::vector<pylontech::Module> modules;
  const std::string header = "Power Volt Curr Tempr Tlow Thigh Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb\r\n";
  pylontech::parse_pwr(header + "1 49872 -1280 22000 20000 25000 3330 33483350 SysError Normal Normal Normal 88%\r\n2 49872 -9999999 999000 20000 25000 3330 3348 Dischg High OV Normal 88%\r\n2 49000 0 22000 20000 25000 3330 3348 Idle Normal Normal Normal 88%\r\n", modules);
  CHECK(modules.size() == 2 && modules[0].cell_high_v == 0 && modules[0].cell_low_v == 0 && modules[0].has_voltage && modules[1].has_voltage && !modules[1].has_current && !modules[1].has_temperature);
  armor::json::Value document;
  CHECK(armor::json::parse(battery_json("solar-1", "battery-1", 5, modules), document));
  CHECK(document.get("cell_max_v") != nullptr && std::fabs(document.get("cell_max_v")->number - 3.348) < 0.001 && document.get("current_a") == nullptr && document.get("state") == nullptr);
  CHECK(document.get("alarm")->boolean == true && has_flag(document.get("stack")->items[0], "base", "SysError", -1));
  CHECK(has_flag(document.get("stack")->items[1], "voltage", "High", -1) && has_flag(document.get("stack")->items[1], "current", "OV", -1));
  CHECK(document.get("stack")->items[1].get("current_a") == nullptr && document.get("stack")->items[1].get("temperature_c") == nullptr && document.get("stack")->items[1].get("voltage_v") != nullptr);
  // and the message says which figures were left out
  const armor::json::Value* left = document.get("stack")->items[1].get("left_out");
  CHECK(left != nullptr && left->items.size() == 2 && left->items[0].text == "current" && left->items[1].text == "temperature");
  CHECK(document.get("stack")->items[0].get("left_out") != nullptr && document.get("stack")->items[0].get("left_out")->items[0].text == "cells");
  // figures that were simply not told are not "left out"
  pylontech::Module quiet_one;
  quiet_one.number = 1; quiet_one.present = true;
  pylontech::keep_plausible(quiet_one);
  CHECK(quiet_one.left_out.empty());
  pylontech::Module wild;
  wild.soc_percent = 300; wild.capacity_ah = -7; wild.full_capacity_ah = 1e9; wild.cycles = -5; wild.health_percent = 200; wild.cell_count = 99; wild.max_charge_a = 1e6; wild.temperatures_c = {900}; wild.cells_v.assign(40, 3.3);
  pylontech::keep_plausible(wild);
  pylontech::keep_plausible(wild);   // twice, as the parser and the message do: nothing is named twice
  CHECK(wild.left_out.size() == 8);
  // the code of the module's own sheet: only of the module that was asked, only when it is not zero, at most 16 characters
  const std::string sheet = " Power  3\r\n System Fault    : 0x40       VOLT err \r\n";
  CHECK(pylontech::parse_system_fault(sheet, 3) == "0x40 VOLT err");
  CHECK(pylontech::parse_system_fault(sheet, 2).empty() && pylontech::parse_system_fault("System Fault : 0x40", 1).empty());
  CHECK(pylontech::parse_system_fault(" Power 1\r\nSystem Fault : 0x0 \r\n", 1).empty() && pylontech::parse_system_fault(" Power 1\r\nSystem Fault : none\r\n", 1).empty());
  CHECK(pylontech::parse_system_fault(" Power 1\r\nSystem Fault : 0x10000100 VOLT err TMPR err\r\n", 1) == "0x10000100 VOLT ");
  // whatever protocol: a cell beyond 10 V or a figure that is not a number never reaches the message
  pylontech::Module odd;
  odd.number = 1; odd.present = true; odd.cell_low_v = 3.3; odd.cell_high_v = 33.4; odd.cells_v = {3.3, 33.4}; odd.voltage_v = std::nan(""); odd.current_a = 1; odd.base_state = "Idle";
  odd.manufacturer = "Daly"; odd.manufacturer_deduced = true;
  const std::string message = battery_json("solar-1", "battery-2", 5, {odd});
  CHECK(armor::json::parse(message, document) && message.find("null") == std::string::npos);
  CHECK(document.get("cell_max_v") == nullptr && document.get("voltage_v") == nullptr && document.get("stack")->items[0].get("cells_v") == nullptr);
  CHECK(document.string_or("manufacturer", "") == "Daly" && document.string_or("manufacturer_source", "") == "deduced");
  // the maker `info` says is told as said
  pylontech::Module told;
  CHECK(pylontech::parse_info("Manufacturer        : Pylon\r\nDevice name         : US3000C\r\n", told) == 1 && told.manufacturer == "Pylon" && !told.manufacturer_deduced);
  pylontech::Module blank;
  pylontech::parse_info("Manufacturer        : \r\nDevice name         : XY100\r\n", blank);
  blank.present = true;
  pylontech::deduce_maker(blank);
  CHECK(blank.manufacturer.empty());   // a model that tells nothing: no maker is made up
}

// The INFO of the RS485 replies, against frames captured by others (pylontech_rs485_real.hpp) and the values their library reads from them.
static void test_pylontech_rs485_info() {
  namespace p = armor::solar::pylontech;
  p::Frame frame;
  std::vector<p::AnalogModule> modules;

  CHECK(p::parse_frame(rs485_real::kUs2000Three, frame) && frame.cid2 == 0);
  CHECK(p::read_analog(frame.info, true, modules) && modules.size() == 3);
  if (modules.size() == 3) {
    const p::AnalogModule& first = modules[0];
    CHECK(first.cells_mv.size() == 15 && first.cells_mv[0] == 3303 && first.cells_mv[14] == 3302);
    CHECK(first.temperatures_dc.size() == 5 && first.temperatures_dc[0] == 230 && first.temperatures_dc[4] == 220);
    CHECK(first.current_da == -26 && first.voltage_mv == 49545 && first.cycles == 31);
    CHECK(first.remaining_mah == 33500 && first.total_mah == 50000);
    CHECK(modules[2].current_da == -27 && modules[2].voltage_mv == 49504);
  }

  CHECK(p::parse_frame(rs485_real::kUs3000Four, frame));
  CHECK(p::read_analog(frame.info, true, modules) && modules.size() == 4);
  if (modules.size() == 4) {
    CHECK(modules[0].current_da == 0 && modules[0].cycles == 47 && modules[0].remaining_mah == 61420 && modules[0].total_mah == 74000);
    CHECK(modules[3].voltage_mv == 49593 && modules[3].cycles == 107 && modules[3].remaining_mah == 60680);
  }

  CHECK(p::parse_frame(rs485_real::kMixedTwo, frame));
  CHECK(p::read_analog(frame.info, true, modules) && modules.size() == 2);
  if (modules.size() == 2) {
    CHECK(modules[0].total_mah == 74000 && modules[0].cycles == 564 && modules[0].current_da == -61);
    CHECK(modules[1].total_mah == 50000 && modules[1].cycles == 658 && modules[1].remaining_mah == 24500);
  }

  CHECK(p::parse_frame(rs485_real::kUp2500One, frame));
  CHECK(p::read_analog(frame.info, false, modules) && modules.size() == 1);
  if (modules.size() == 1) {
    CHECK(modules[0].cells_mv.size() == 8 && modules[0].cells_mv[0] == 3330 && modules[0].voltage_mv == 26638);
    CHECK(modules[0].current_da == -6 && modules[0].remaining_mah == 95460 && modules[0].total_mah == 111000 && modules[0].cycles == 0);
  }

  // a text that is cut, too long or not hexadecimal gives nothing
  CHECK(!p::read_analog(frame.info.substr(0, frame.info.size() - 2), false, modules) && modules.empty());
  CHECK(!p::read_analog(frame.info + "00", false, modules) && modules.empty());
  CHECK(!p::read_analog("1002GG", false, modules));
  CHECK(!p::read_analog("", true, modules));

  p::ManagementInfo management;
  CHECK(p::parse_frame(rs485_real::kUp2500Management, frame));
  CHECK(p::read_management(frame.info, management));
  CHECK(management.charge_voltage_mv == 28400 && management.discharge_voltage_mv == 23200);
  CHECK(management.charge_current_da == 555 && management.discharge_current_da == -555);
  CHECK(management.charge_enable && management.discharge_enable && !management.charge_immediately_1 && !management.charge_immediately_2 && !management.full_charge_request);
  CHECK(!p::read_management("026EF0", management));
}

// The table reader against the driver of the same registers: the same answer of a unit gives the same figures.
static void test_modbus_table() {
  namespace d = armor::solar::drivers;
  constexpr std::size_t kFields = sizeof d::kSrneMpptTable / sizeof d::kSrneMpptTable[0];
  const std::vector<d::TableBlock> blocks = d::plan_reads(d::kSrneMpptTable, kFields);
  CHECK(blocks.size() == 2 && blocks[0].first == 0x0100 && blocks[0].count == 3 && blocks[1].first == 0x0107 && blocks[1].count == 3);
  const std::vector<d::TableBlock> joined = d::plan_reads(d::kSrneMpptTable, kFields, 32, 4);
  CHECK(joined.size() == 1 && joined[0].first == 0x0100 && joined[0].count == 10);
  CHECK(d::plan_reads(d::kSrneMpptTable, kFields, 2, 4).size() == 4);
  const std::vector<std::uint8_t> request = d::block_request(1, joined[0]);
  CHECK(request.size() == 8 && request[0] == 1 && request[1] == 0x03);   // the only function it can ask for

  const std::uint8_t data[20] = {0, 87, 2, 20, 0, 125, 0x19, 0x19, 0, 0, 0, 0, 0, 0, 3, 0xE8, 0, 25, 1, 244};
  std::vector<d::TableValue> values;
  CHECK(d::decode_block(joined[0], data, sizeof data, d::kSrneMpptTable, kFields, values) && values.size() == kFields);
  if (values.size() == kFields) {
    CHECK(values[0].value == 87 && std::fabs(values[1].value - 53.2) < 1e-9 && std::fabs(values[2].value - 12.5) < 1e-9);
    CHECK(std::fabs(values[3].value - 100.0) < 1e-9 && std::fabs(values[4].value - 2.5) < 1e-9 && values[5].value == 500);
  }
  CHECK(!d::decode_block(joined[0], data, sizeof data - 1, d::kSrneMpptTable, kFields, values));

  const d::TableField wide[] = {{10, d::RegisterType::kS32, 1, "a"}, {12, d::RegisterType::kS16, 1, "b"}};
  const std::vector<d::TableBlock> both = d::plan_reads(wide, 2);
  CHECK(both.size() == 1 && both[0].first == 10 && both[0].count == 3);
  const std::uint8_t negative[6] = {0xFF, 0xFF, 0xFF, 0xFE, 0x80, 0x00};
  values.clear();
  CHECK(d::decode_block(both[0], negative, sizeof negative, wide, 2, values) && values.size() == 2);
  if (values.size() == 2) CHECK(values[0].value == -2 && values[1].value == -32768);

  d::ModbusDriver driver(d::ModbusMap::kSrneMppt);
  std::vector<std::uint8_t> next = driver.begin();
  std::vector<std::uint8_t> reply{1, 0x03, 20};
  reply.insert(reply.end(), data, data + sizeof data);
  const std::uint16_t crc = d::crc16_modbus(reply.data(), reply.size());
  reply.push_back(static_cast<std::uint8_t>(crc));
  reply.push_back(static_cast<std::uint8_t>(crc >> 8));
  armor::solar::drivers::Rx last = armor::solar::drivers::Rx::kMore;
  for (const std::uint8_t byte : reply) last = driver.feed(byte, next);
  CHECK(last == armor::solar::drivers::Rx::kNext);
  std::vector<std::uint8_t> ignored;
  last = driver.timeout(ignored);   // the second block is optional: without it the first one stands
  CHECK(last == armor::solar::drivers::Rx::kDone);
  CHECK(driver.out().status.battery_percent == 87 && std::fabs(driver.out().status.battery_v - 53.2) < 1e-9 && std::fabs(driver.out().status.battery_charge_a - 12.5) < 1e-9);
  CHECK(std::fabs(driver.out().status.pv_v - 100.0) < 1e-9 && std::fabs(driver.out().status.pv_a - 2.5) < 1e-9 && driver.out().status.pv_w == 500);
}

int main() {
  test_pylontech_rs485_info();
  test_modbus_table();
  test_pylontech_cells_not_read();
  test_pylontech_impossible_figures();
  test_voltronic_commands();
  test_voltronic_replies();
  test_voltronic_frames();
  test_pylontech_console();
  test_pylontech_frames();
  test_pylontech_cells_and_capacity();
  test_pylontech_real_formats();
  test_pylontech_real_captures();
  test_pylontech_data_plate();
  test_pylontech_stat_counters();
  test_pylontech_real_console_answers();
  test_pylontech_wake_frame();
  test_pylontech_real_us2000();
  test_pylontech_pwrsys();
  test_pylontech_remaining_capacity();
  test_pylontech_missing_sheets();
  test_pylontech_late_answer();
  test_pylontech_slow_module();
  test_port_diagnosis();
  test_pylontech_real_logs();
  test_pylontech_port_speeds();
  test_messages();
  test_pylontech_alarm_levels();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
