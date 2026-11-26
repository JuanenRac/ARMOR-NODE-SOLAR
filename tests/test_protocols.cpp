// ARMOR-NODE-SOLAR - host tests of the protocol catalogue and drivers, and of what keeps an inverter port reading when part of its answers fails.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The JBD frames and the Modbus requests are the examples printed in the makers' documents; the DALY, JK, SMG II, SRNE and VE.Direct frames are built here from the layout those
// documents describe. No equipment has been connected.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../core/poller.hpp"

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

using namespace armor;
using namespace armor::solar;
using Bytes = std::vector<std::uint8_t>;

static std::vector<std::uint8_t> as_bytes(const std::string& text) { return std::vector<std::uint8_t>(text.begin(), text.end()); }
static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }
static bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

static Bytes hex(const std::string& text) {
  Bytes out;
  int high = -1;
  for (const char c : text) {
    int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (v < 0) continue;
    if (high < 0) high = v;
    else { out.push_back(static_cast<std::uint8_t>(high * 16 + v)); high = -1; }
  }
  return out;
}

// Feeds a whole frame; the last result that was not "more".
static drivers::Rx give(drivers::Driver& d, const Bytes& frame, Bytes& next) {
  drivers::Rx last = drivers::Rx::kMore;
  for (const std::uint8_t b : frame) {
    const drivers::Rx r = d.feed(b, next);
    if (r != drivers::Rx::kMore) last = r;
  }
  return last;
}

static void test_jbd() {
  CHECK(drivers::JbdDriver::request(0x03) == hex("DD A5 03 00 FF FD 77"));
  CHECK(drivers::JbdDriver::request(0x04) == hex("DD A5 04 00 FF FC 77"));
  drivers::JbdDriver d;
  Bytes next;
  CHECK(d.begin() == hex("DD A5 03 00 FF FD 77") && !d.inverter());
  CHECK(give(d, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next) == drivers::Rx::kNext);
  CHECK(next == hex("DD A5 04 00 FF FC 77"));
  CHECK(give(d, hex("DD 04 00 1E 0F 66 0F 63 0F 63 0F 64 0F 3E 0F 63 0F 37 0F 5B 0F 65 0F 3B 0F 63 0F 63 0F 3C 0F 66 0F 3D F9 F9 77"), next) == drivers::Rx::kNext);
  CHECK(next == hex("DD A5 05 00 FF FB 77") && give(d, hex("DD 05 00 04 41 42 43 31 FF 05 77"), next) == drivers::Rx::kDone && d.out().modules.front().board_version == "ABC1");
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 58.88) && near(m.current_a, 0) && near(m.capacity_ah, 7.2) && near(m.full_capacity_ah, 10) && m.soc_percent == 72);
  CHECK(m.cells_v.size() == 15 && near(m.cells_v.front(), 3.942) && near(m.cells_v.back(), 3.901) && m.cell_count == 15);
  CHECK(m.temperatures_c.size() == 2 && near(m.temperatures_c[0], 20.3) && near(m.temperatures_c[1], 21.5) && !m.protecting);
  // a damaged frame is refused, and without the cells the basic information is still a reading
  d.begin();
  CHECK(give(d, hex("DD 03 00 1B 17 01 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next) == drivers::Rx::kBad && std::string(d.error()) == "crc");
  d.begin();
  CHECK(give(d, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next) == drivers::Rx::kNext);
  CHECK(d.timeout(next) == drivers::Rx::kDone && d.out().modules.front().cells_v.empty() && d.out().modules.front().board_version == "ABC1");
  // the version is asked once: a board that has answered is not asked again, and one that never does is left alone after the first try
  d.begin();
  give(d, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next);
  CHECK(give(d, hex("DD 04 00 02 0F 66 FF 89 77"), next) == drivers::Rx::kDone);
  drivers::JbdDriver mute;
  mute.begin();
  give(mute, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next);
  CHECK(give(mute, hex("DD 04 00 02 0F 66 FF 89 77"), next) == drivers::Rx::kNext && mute.timeout(next) == drivers::Rx::kDone && mute.out().modules.front().board_version.empty());
  mute.begin();
  give(mute, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next);
  CHECK(give(mute, hex("DD 04 00 02 0F 66 FF 89 77"), next) == drivers::Rx::kDone);
}

static Bytes daly(std::uint8_t id, const std::string& data) {
  Bytes f{0xA5, 0x01, id, 0x08};
  const Bytes d = hex(data);
  f.insert(f.end(), d.begin(), d.end());
  f.resize(12, 0);
  unsigned sum = 0;
  for (const std::uint8_t b : f) sum += b;
  f.push_back(static_cast<std::uint8_t>(sum));
  return f;
}

static void test_daly() {
  drivers::DalyDriver d;
  Bytes next;
  const Bytes first = d.begin();
  CHECK(first == hex("A5 40 90 08 00 00 00 00 00 00 00 00 7D"));
  // 52.8 V, charging current offset 30000 (here -12.5 A), 87.3 %
  CHECK(give(d, first, next) == drivers::Rx::kMore);   // the request heard back is not an answer
  CHECK(give(d, daly(0x90, "02 10 02 10 74 B3 03 69"), next) == drivers::Rx::kNext && next[2] == 0x93);
  CHECK(give(d, daly(0x93, "02 01 01 05 00 00 C3 50"), next) == drivers::Rx::kNext && next[2] == 0x94);
  CHECK(give(d, daly(0x94, "04 02 00 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x95);
  CHECK(give(d, daly(0x95, "01 0C E4 0C E5 0C E6 00"), next) == drivers::Rx::kMore);
  CHECK(give(d, daly(0x95, "02 0C E7 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x96);
  CHECK(give(d, daly(0x96, "01 41 43 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x97);
  CHECK(give(d, daly(0x97, "05 00 00 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x98);
  CHECK(give(d, daly(0x98, "00 00 00 00 00 00 00 00"), next) == drivers::Rx::kDone);
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 52.8) && near(m.current_a, -12.5) && m.soc_percent == 87 && m.base_state == "Dischg" && near(m.capacity_ah, 50));
  CHECK(m.cells_v.size() == 4 && near(m.cells_v[3], 3.303) && m.temperatures_c.size() == 2 && near(m.temperatures_c[0], 25) && m.system_alarm_state == "Normal" && m.balancing == 2);
  // cell frames numbered from 0, as the document says
  drivers::DalyDriver zero;
  zero.begin();
  give(zero, daly(0x90, "02 10 02 10 74 B3 03 69"), next);
  give(zero, daly(0x93, "02 01 01 05 00 00 C3 50"), next);
  CHECK(give(zero, daly(0x94, "04 00 00 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x95);
  CHECK(give(zero, daly(0x95, "00 0C E4 0C E5 0C E6 00"), next) == drivers::Rx::kMore && give(zero, daly(0x95, "01 0C E7 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x97);
  CHECK(zero.timeout(next) == drivers::Rx::kNext && next[2] == 0x98 && give(zero, daly(0x98, "00 00 00 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x92);
  CHECK(give(zero, daly(0x92, "42 01 3C 02 00 00 00 00"), next) == drivers::Rx::kDone);
  const pylontech::Module& z = zero.out().modules.front();
  CHECK(z.cells_v.size() == 4 && near(z.cells_v[0], 3.300) && near(z.cells_v[3], 3.303) && near(z.temperature_high_c, 26) && near(z.temperature_low_c, 20) && z.has_temperature);
  // only the first question is essential
  d.begin();
  CHECK(d.timeout(next) == drivers::Rx::kBad);
  d.begin();
  give(d, daly(0x90, "02 10 02 10 75 30 03 69"), next);
  CHECK(d.timeout(next) == drivers::Rx::kNext && next[2] == 0x94);
}

static Bytes jk_frame(const std::string& data) {
  const Bytes info = hex(data);
  Bytes f{0x4E, 0x57, 0, 0, 0, 0, 0, 0, 0x06, 0x00, 0x01};
  f.insert(f.end(), info.begin(), info.end());
  f.insert(f.end(), {0, 0, 0, 0, 0x68});
  const std::size_t length = f.size() + 4 - 2;
  f[2] = static_cast<std::uint8_t>(length >> 8);
  f[3] = static_cast<std::uint8_t>(length);
  unsigned sum = 0;
  for (const std::uint8_t b : f) sum += b;
  f.insert(f.end(), {0, 0, static_cast<std::uint8_t>(sum >> 8), static_cast<std::uint8_t>(sum)});
  return f;
}

static void test_jk() {
  CHECK(drivers::JkDriver::request() == hex("4E 57 00 13 00 00 00 00 06 03 00 00 00 00 00 00 68 00 00 01 29"));
  drivers::JkDriver d;
  Bytes next;
  d.begin();
  // two cells, tube 25, box -1 (101), battery 22, 52.80 V, current (10000 - 9500) = +5.00 A, 64 %
  CHECK(give(d, jk_frame("79 06 01 0C E4 02 0C EE 80 00 19 81 00 65 82 00 16 83 14 A0 84 25 1C 85 40 8A 00 02 8B 00 00 B7 56 31 2E 32 00 00 00 00 00 00 00 00 00 00 00"), next) == drivers::Rx::kDone);
  CHECK(d.out().modules.front().soft_version == "V1.2");
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 52.8) && near(m.current_a, 5.0) && m.soc_percent == 64 && m.cells_v.size() == 2 && near(m.cells_v[1], 3.31) && m.cell_count == 2);
  CHECK(m.temperatures_c.size() == 3 && near(m.temperatures_c[1], -1) && near(m.temperature_low_c, -1) && near(m.temperature_high_c, 25));
  d.begin();
  Bytes broken = jk_frame("83 14 A0");
  broken[12] ^= 1;
  CHECK(give(d, broken, next) == drivers::Rx::kBad && std::string(d.error()) == "crc");
}

static Bytes modbus_reply(const std::vector<int>& registers) {
  Bytes f{1, 3, static_cast<std::uint8_t>(registers.size() * 2)};
  for (const int r : registers) {
    f.push_back(static_cast<std::uint8_t>((r >> 8) & 0xFF));
    f.push_back(static_cast<std::uint8_t>(r & 0xFF));
  }
  const std::uint16_t crc = drivers::crc16_modbus(f.data(), f.size());
  f.push_back(static_cast<std::uint8_t>(crc));
  f.push_back(static_cast<std::uint8_t>(crc >> 8));
  return f;
}

static void test_modbus() {
  // the two examples of the SMG II document
  CHECK(drivers::modbus_read(1, 202, 3) == hex("01 03 00 CA 00 03 25 F5"));
  const Bytes sample = hex("01 03 06 08 FC 13 88 04 B0 F7 F3");
  CHECK(drivers::crc16_modbus(sample.data(), sample.size() - 2) == 0xF3F7);
  drivers::ModbusDriver d(drivers::ModbusMap::kSmg2);
  Bytes next;
  CHECK(d.begin() == drivers::modbus_read(1, 201, 34) && d.inverter());
  std::vector<int> r(34, 0);
  const auto at = [&r](int reg, int value) { r[static_cast<std::size_t>(reg - 201)] = value; };
  at(201, 3); at(202, 2300); at(203, 5000); at(210, 2301); at(212, 5000); at(213, 850); at(214, 900); at(215, 528); at(219, 3200); at(220, 30); at(223, 960); at(224, 400);
  at(225, 17); at(226, 41); at(227, 38); at(229, 76); at(232, 0x10000 - 125);
  CHECK(give(d, modbus_reply(r), next) == drivers::Rx::kNext && next == drivers::modbus_read(1, 100, 2));
  CHECK(give(d, modbus_reply({0, 0}), next) == drivers::Rx::kNext && next == drivers::modbus_read(1, 108, 2));
  CHECK(give(d, modbus_reply({0, 0x0402}), next) == drivers::Rx::kDone);
  const drivers::Out& o = d.out();
  CHECK(o.mode == 'B' && near(o.status.grid_v, 230) && near(o.status.out_v, 230.1) && near(o.status.out_w, 850) && near(o.status.battery_v, 52.8) && near(o.status.battery_a(), -12.5));
  CHECK(near(o.status.pv_v, 320) && near(o.status.pv_w, 960) && near(o.status.battery_percent, 76) && near(o.status.heatsink_c, 41) && o.status.load_on && o.status.scc_charging);
  CHECK(o.warnings_known && o.warnings.size() == 2 && o.warnings[0] == "mains_waveform" && o.warnings[1] == "overload");
  // a refusal of the essential block loses the cycle; of an optional one only leaves it out
  d.begin();
  Bytes refusal = hex("01 83 02");
  const std::uint16_t crc = drivers::crc16_modbus(refusal.data(), 3);
  refusal.push_back(static_cast<std::uint8_t>(crc));
  refusal.push_back(static_cast<std::uint8_t>(crc >> 8));
  CHECK(give(d, refusal, next) == drivers::Rx::kBad && std::string(d.error()) == "nak");
  d.begin();
  give(d, modbus_reply(r), next);
  CHECK(give(d, refusal, next) == drivers::Rx::kNext && d.timeout(next) == drivers::Rx::kDone && !d.out().warnings_known);

  drivers::ModbusDriver srne(drivers::ModbusMap::kSrneHybrid);
  CHECK(srne.begin() == drivers::modbus_read(1, 0x0100, 15));
  std::vector<int> dc(15, 0), ac(18, 0);
  dc[0] = 55; dc[1] = 512; dc[2] = 0x10000 - 200; dc[7] = 3000; dc[8] = 40; dc[9] = 1200;
  ac[0] = 5; ac[3] = 0; ac[6] = 2300; ac[8] = 5000; ac[11] = 700; ac[12] = 760; ac[15] = 14; ac[16] = 350; ac[17] = 400;
  CHECK(give(srne, modbus_reply(dc), next) == drivers::Rx::kNext && next == drivers::modbus_read(1, 0x0210, 18));
  CHECK(give(srne, modbus_reply(ac), next) == drivers::Rx::kNext && next == drivers::modbus_read(1, 0x0204, 4));
  CHECK(give(srne, modbus_reply({1, 14, 0, 0}), next) == drivers::Rx::kDone && srne.out().warnings_known && srne.out().warnings.size() == 2 && srne.out().warnings[1] == "fault_code_14");
  CHECK(srne.out().mode == 'B' && near(srne.out().status.battery_a(), 20) && near(srne.out().status.out_v, 230) && near(srne.out().status.out_w, 700) && near(srne.out().status.heatsink_c, 40));

  drivers::ModbusDriver mppt(drivers::ModbusMap::kSrneMppt);
  mppt.begin();
  CHECK(give(mppt, modbus_reply({80, 271, 52, 0x1E19, 0, 0, 0, 355, 40, 142}), next) == drivers::Rx::kNext);
  CHECK(mppt.timeout(next) == drivers::Rx::kDone);
  CHECK(near(mppt.out().status.battery_v, 27.1) && near(mppt.out().status.battery_charge_a, 5.2) && near(mppt.out().status.heatsink_c, 30) && near(mppt.out().status.pv_w, 142) && near(mppt.out().status.out_w, 0));
}

static Bytes vedirect(const std::string& lines) {
  std::string block = lines + "\r\nChecksum\t";
  unsigned sum = 0;
  for (const char c : block) sum += static_cast<std::uint8_t>(c);
  Bytes f(block.begin(), block.end());
  f.push_back(static_cast<std::uint8_t>(0x100 - (sum & 0xFF)));
  return f;
}

static void test_vedirect() {
  drivers::VedirectDriver monitor(false);
  Bytes next;
  CHECK(monitor.begin().empty() && !monitor.inverter());
  CHECK(give(monitor, vedirect("\r\nH1\t-10\r\nH2\t5"), next) == drivers::Rx::kMore);   // the history block
  Bytes noisy = hex("3A 41 34 0A");                                                       // a hexadecimal frame in between
  CHECK(give(monitor, noisy, next) == drivers::Rx::kMore);
  CHECK(give(monitor, vedirect("\r\nV\t26480\r\nI\t-1500\r\nSOC\t876\r\nAlarm\tOFF"), next) == drivers::Rx::kDone);
  const pylontech::Module& m = monitor.out().modules.front();
  CHECK(near(m.voltage_v, 26.48) && near(m.current_a, -1.5) && m.soc_percent == 88 && !m.has_temperature && m.cells_v.empty());
  Bytes broken = vedirect("\r\nV\t26480");
  broken[4] ^= 1;
  monitor.begin();
  CHECK(give(monitor, broken, next) == drivers::Rx::kMore);

  drivers::VedirectDriver charger(true);
  charger.begin();
  CHECK(charger.inverter() && give(charger, vedirect("\r\nV\t53200\r\nI\t8200\r\nVPV\t98000\r\nPPV\t450\r\nCS\t3\r\nERR\t0"), next) == drivers::Rx::kDone);
  CHECK(near(charger.out().status.battery_v, 53.2) && near(charger.out().status.pv_w, 450) && charger.out().status.scc_charging && charger.out().warnings_known && charger.out().warnings.empty());
}

static Bytes reply(const std::string& text) {
  Bytes f{'('};
  f.insert(f.end(), text.begin(), text.end());
  const auto crc = voltronic::wire_crc(f.data(), f.size());
  f.push_back(crc[0]);
  f.push_back(crc[1]);
  f.push_back('\r');
  return f;
}

static const char kQpigs[] = "230.0 50.0 230.0 50.0 0450 0400 009 380 52.80 012 076 0035 05.2 310.4 00.00 00000 00010110 00 00 01612 010";

// A stand-in inverter: `mode` and `flags` empty means that question is never answered.
static std::vector<std::string> run(Poller& p, const std::string& qpigs, const std::string& mode, const std::string& flags, std::uint64_t until_ms) {
  std::vector<std::string> messages;
  for (std::uint64_t now = 0; now < until_ms; now += 10) {
    const Bytes out = p.next_tx(now);
    if (out.size() > 3) {
      const std::string command(out.begin(), out.end() - 3);
      const std::string& answer = command == "QPIGS" ? qpigs : command == "QMOD" ? mode : command == "QPIWS" ? flags : std::string();
      if (!answer.empty()) { const Bytes f = reply(answer); p.on_rx(f.data(), f.size(), now); }
    }
    std::string topic, payload;
    if (p.take_message(1700000000000ULL + now, topic, payload)) messages.push_back(payload);
  }
  return messages;
}

static void test_inverter_that_does_not_answer_everything() {
  // an inverter that never answers QPIWS was never published at all: from the third try the reading goes out and says the warnings are unknown
  Poller silent_flags(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "pi30");
  std::vector<std::string> m = run(silent_flags, kQpigs, "L", "", 40000);
  CHECK(!m.empty() && has(m.front(), "\"warnings_known\":false") && has(m.front(), "\"mode\":\"line\"") && silent_flags.stats().last_error == "timeout");
  Poller refused(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "pi30");
  m = run(refused, kQpigs, "NAK", "NAK", 40000);
  CHECK(!m.empty() && has(m.front(), "\"mode\":\"unknown\"") && has(m.front(), "\"warnings_known\":false"));
  // more flags than the documents name
  Poller longer(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "pi30");
  m = run(longer, kQpigs, "B", std::string("00000100000000000000000000000000000000"), 8000);
  CHECK(!m.empty() && has(m.front(), "line_fail") && !has(m.front(), "warnings_known"));
  // the diagnosis says what was asked, what it was sent as and the last error
  Poller nothing(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "pi30");
  run(nothing, "", "", "", 9000);
  CHECK(has(nothing.diagnosis(), "asked: QPIGS | 51 50 49 47 53 B7 A9 0D") && has(nothing.diagnosis(), "heard 0,") && has(nothing.diagnosis(), "last error: timeout"));
}

static void test_pi41_and_pi34() {
  voltronic::Status s;
  CHECK(voltronic::parse_qpigs_pi41("230.0 50.0 230.0 50.0 0450 0400 009 380 52.80 012 076 0035 05.2 310.4 00.00 00000 01100010 00 00 01612 000", s));
  CHECK(s.ac_charging && s.scc_charging && s.load_on && near(s.pv_w, 1612));
  CHECK(voltronic::parse_qpigs_pi34("120.5 53.20 22.50 12.00 10.50 1197 +041 53.1 +025 0000 01000000", s));
  CHECK(near(s.pv_v, 120.5) && near(s.battery_charge_a, 22.5) && near(s.pv_w, 1197) && near(s.heatsink_c, 41) && s.scc_charging && near(s.out_w, 0));
  Poller p(config::Kind::kVoltronic, "solar-1", "mppt-1", 5, 0, "pi34");
  const std::vector<std::string> m = run(p, "120.5 53.20 22.50 12.00 10.50 1197 +041 53.1 +025 0000 01000000", "", "", 4000);
  CHECK(!m.empty() && has(m.front(), "\"kind\":\"inverter\"") && has(m.front(), "\"warnings_known\":false") && p.detail() == "pi34");
}

// ---- PI00: questions without CRC ---------------------------------------------------------------------------------------------------------
// A stand-in that answers by the exact bytes it is asked with (invented frames).
static Bytes plain(const std::string& text) {
  Bytes f{'('};
  f.insert(f.end(), text.begin(), text.end());
  f.push_back('\r');
  return f;
}
static Bytes ask(const char* command, bool crc) {
  Bytes out;
  if (crc) voltronic::build_command(command, out);
  else voltronic::build_plain_command(command, out);
  return out;
}
struct Unit {
  std::vector<std::pair<Bytes, Bytes>> answers;
  std::vector<Bytes> sent;
  std::vector<std::string> messages;
  std::uint64_t now = 0;
  void run(Poller& p, std::uint64_t for_ms) {
    const std::uint64_t until = now + for_ms;
    for (; now < until; now += 100) {
      const Bytes out = p.next_tx(now);
      if (!out.empty()) {
        sent.push_back(out);
        for (const auto& a : answers)
          if (a.first == out) { p.on_rx(a.second.data(), a.second.size(), now + 50); break; }
      }
      std::string topic, payload;
      if (p.take_message(1700000000000ULL + now, topic, payload)) messages.push_back(payload);
    }
  }
};

static const char* kOldQpigs = "229.0 50.0 230.0 50.0 0300 0250 005 380 51.20 004 076 0031 0003 098.0 51.30 00000 00010110";
static const char* kPi00Qpigs = "231.0 001200 50.0 0005.0 230.0 000690 50.0 003.0 023 380.0 379.0 052.4 000.0 064 00400 00250 00000 210.0 190.0 000.0 041.0 0000101010";

static void test_pi00_frames() {
  Bytes out;
  CHECK(voltronic::build_plain_command("QPIGS", out) && out == hex("51 50 49 47 53 0D"));
  CHECK(!voltronic::build_plain_command("PCP00", out) && out.empty() && !voltronic::build_plain_command("Q P", out));
  std::string text;
  const Bytes good = plain("L");
  CHECK(voltronic::parse_reply_plain(good.data(), good.size(), text) && text == "L");
  Bytes bad = plain(std::string("L") + '\x01');
  CHECK(!voltronic::parse_reply_plain(bad.data(), bad.size(), text) && text.empty());
  CHECK(!voltronic::parse_reply_plain(good.data(), good.size() - 1, text) && !voltronic::parse_reply_plain(good.data() + 1, good.size() - 1, text));

  voltronic::Status s;
  CHECK(voltronic::parse_qpigs_pi00(kPi00Qpigs, s) && near(s.grid_v, 231) && near(s.out_w, 690) && near(s.out_va, 690) && near(s.load_percent, 23) && near(s.bus_v, 380));
  CHECK(near(s.battery_v, 52.4) && near(s.battery_percent, 64) && near(s.pv_w, 650) && near(s.pv_v, 210) && near(s.heatsink_c, 41) && s.load_on && s.charging && !s.ac_charging && s.scc_charging);
  // the form of the document's worked example: a battery current after the capacity (discharging here)
  CHECK(voltronic::parse_qpigs_pi00("231.0 001200 50.0 0005.0 230.0 000690 50.0 003.0 023 380.0 379.0 052.4 000.0 064 012.5 00000 00000 00000 000.0 000.0 000.0 030.0 0000110000", s) && near(s.battery_discharge_a, 12.5) && !s.charging && near(s.pv_w, 0));
  CHECK(!voltronic::parse_qpigs_pi00(kOldQpigs, s) && !voltronic::parse_qpigs_pi00("", s));
  CHECK(!voltronic::parse_qpigs_pi00(std::string(kPi00Qpigs).substr(0, std::strlen(kPi00Qpigs) - 1), s));   // nine status digits
  std::string letters = kPi00Qpigs;
  letters[1] = 'x';
  CHECK(!voltronic::parse_qpigs_pi00(letters, s) && !voltronic::parse_qpigs(kPi00Qpigs, s));

  char mode = 0;
  CHECK(voltronic::parse_qmod_pi00("G", mode) && mode == 'L' && voltronic::parse_qmod_pi00("T", mode) && mode == 'B' && voltronic::parse_qmod_pi00("S", mode) && mode == 'S');
  CHECK(!voltronic::parse_qmod_pi00("Z", mode) && !voltronic::parse_qmod_pi00("GG", mode) && !voltronic::parse_qmod("G", mode));
  std::vector<std::string> flags;
  std::string warnings(128, '0');
  warnings[3] = '1'; warnings[18] = '1'; warnings[100] = '1';
  CHECK(voltronic::parse_qpiws_pi00(warnings, flags) && flags.size() == 2 && flags[0] == "pv_loss" && flags[1] == "overload");
  CHECK(!voltronic::parse_qpiws_pi00(warnings + "0", flags) && !voltronic::parse_qpiws_pi00("0a0", flags) && !voltronic::parse_qpiws_pi00("", flags));
  CHECK(config::valid_dialect(config::Kind::kVoltronic, "pi00") && !config::valid_dialect(config::Kind::kModbus, "pi00"));
}

static void test_pi00_port() {
  // "auto" on a unit that only understands questions without CRC: PI30 with CRC is silent, then the same question without it
  Poller p(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "auto");
  Unit old;
  old.answers = {{ask("QPIGS", false), plain(kOldQpigs)}, {ask("QMOD", false), plain("B")}, {ask("QPIWS", false), plain(std::string(32, '0'))}};
  old.run(p, 12000);
  CHECK(old.sent.size() >= 4 && old.sent[0] == ask("QPIGS", true) && old.sent[1] == ask("QPIGS", false) && old.sent[2] == ask("QMOD", false) && old.sent[3] == ask("QPIWS", false));
  CHECK(p.detail() == "pi00" && !old.messages.empty() && has(old.messages.front(), "\"mode\":\"battery\"") && has(old.messages.front(), "\"battery_v\":51.2"));
  CHECK(has(p.diagnosis(), "(no CRC) | 51 ") && !has(p.diagnosis(), "automatic, in turn"));

  // while it looks, the raw view says every question it sends in turn
  Poller silent(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "auto");
  Unit none;
  none.run(silent, 3500);
  CHECK(has(silent.diagnosis(), "automatic, in turn") && has(silent.diagnosis(), "pi30 = 51 50 49 47 53 B7 A9 0D") && has(silent.diagnosis(), "pi00, no CRC = 51 50 49 47 53 0D") && has(silent.diagnosis(), "pi18 = 5E 50 30 30 35 47 53"));
  none.run(silent, 12000);
  CHECK(none.sent.size() >= 5 && none.sent[0] == ask("QPIGS", true) && none.sent[1] == ask("QPIGS", false) && none.sent[2][0] == '^' && none.sent[3][0] == '^' && none.sent[3] != none.sent[2] && none.sent[4] == ask("QPIGS", true));

  // chosen by hand, with the PI00 arrangement of the answers
  Poller fixed(config::Kind::kVoltronic, "solar-1", "infini-1", 5, 0, "pi00");
  Unit infini;
  std::string warnings(128, '0');
  warnings[22] = '1';
  infini.answers = {{ask("QPIGS", false), plain(kPi00Qpigs)}, {ask("QMOD", false), plain("G")}, {ask("QPIWS", false), plain(warnings)}};
  infini.run(fixed, 4000);
  CHECK(!infini.messages.empty() && has(infini.messages.front(), "\"pv_w\":650") && has(infini.messages.front(), "\"mode\":\"line\"") && has(infini.messages.front(), "over_temperature") && fixed.detail() == "pi00");
  CHECK(has(fixed.diagnosis(), "asked: QPIWS (no CRC) | "));

  // a PI30 unit that missed the first question answers the second with a CRC: it is asked in PI30 again and stays PI30
  Poller late(config::Kind::kVoltronic, "solar-1", "axpert-1", 5, 0, "auto");
  Unit axpert;
  axpert.run(late, 500);
  axpert.answers = {{ask("QPIGS", false), reply(kOldQpigs)}, {ask("QPIGS", true), reply(kOldQpigs)}, {ask("QMOD", true), reply("L")}, {ask("QPIWS", true), reply(std::string(32, '0'))}};
  axpert.run(late, 15000);
  CHECK(late.detail() == "pi30" && !axpert.messages.empty());

  // a unit that wants no CRC but answers with one: PI30 is silent when asked again, and PI00 is kept
  Poller mixed(config::Kind::kVoltronic, "solar-1", "odd-1", 5, 0, "auto");
  Unit odd;
  odd.answers = {{ask("QPIGS", false), reply(kOldQpigs)}, {ask("QMOD", false), reply("L")}, {ask("QPIWS", false), reply(std::string(32, '0'))}};
  odd.run(mixed, 30000);
  CHECK(mixed.detail() == "pi00" && !odd.messages.empty());

  // a refusal without CRC while looking goes on to the next protocol
  Poller refused(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "auto");
  Unit nak;
  nak.answers = {{ask("QPIGS", false), plain("NAK")}};
  nak.run(refused, 6000);
  CHECK(nak.sent.size() >= 3 && nak.sent[2][0] == '^' && refused.detail().empty() && nak.messages.empty());

  // damaged answers are named and nothing is published
  Poller damaged(config::Kind::kVoltronic, "solar-1", "easun-1", 5, 0, "pi00");
  Unit noise;
  noise.answers = {{ask("QPIGS", false), plain(std::string(kOldQpigs).substr(0, 40))}};
  noise.run(damaged, 3000);
  CHECK(noise.messages.empty() && damaged.stats().last_error == "format");
  noise.answers = {{ask("QPIGS", false), plain(std::string("12") + '\x02')}};
  noise.run(damaged, 6000);
  CHECK(noise.messages.empty() && damaged.stats().last_error == "crc");
}

// ---- PI16, PI17, the PIP-GK letters, VE.Direct HEX and the Phoenix inverter (invented frames) ----------------------------------------------
static const char* kPi16Qpigs = "227.0 000400 50.0 0001.8 229.0 00420 49.9 001.9 012 421.0 421.0 053.1 ---.- 081 00510 00200 ----- 188.0 ---.- ---.- 029.0 A---101001";

static void test_pi16_and_variants() {
  voltronic::Status s;
  bool pi16 = false;
  CHECK(voltronic::parse_qpigs_pi00(kPi16Qpigs, s, &pi16) && pi16 && near(s.grid_v, 227) && near(s.out_w, 420) && near(s.pv_w, 710) && near(s.pv_v, 188) && near(s.battery_v, 53.1) && near(s.heatsink_c, 29));
  CHECK(voltronic::parse_qpigs_pi00(kPi00Qpigs, s, &pi16) && !pi16);
  std::string broken = kPi16Qpigs;
  broken.back() = '7';
  CHECK(!voltronic::parse_qpigs_pi00(broken, s));
  std::vector<std::string> flags;
  CHECK(voltronic::parse_qpiws_pi00("1-0000000000--01", flags, true) && flags.size() == 2 && flags[0] == "inverter_fault" && flags[1] == "battery_low");
  CHECK(voltronic::parse_qpiws_pi00("1", flags, false) && flags[0] == "pv_fail" && !voltronic::parse_qpiws_pi00("2", flags, true));
  char mode = 0;
  CHECK(voltronic::parse_qmod("C", mode) && mode == 'L' && voltronic::parse_qmod("Y", mode) && mode == 'L' && voltronic::parse_qmod("E", mode) && mode == 'H');
  // the two digits of a fault code among the flags of the MAX and PIP-MK
  std::string warnings(36, '0');
  warnings[5] = '1'; warnings[31] = '5'; warnings[32] = '3';
  CHECK(voltronic::parse_qpiws(warnings, flags) && flags.size() == 1 && flags[0] == "line_fail");
  warnings[30] = '2';
  CHECK(!voltronic::parse_qpiws(warnings, flags));
}

static Bytes pi17_reply(const std::string& payload) {
  const std::size_t n = payload.size() + 3;
  std::string head = "^D";
  head += static_cast<char>('0' + n / 100); head += static_cast<char>('0' + (n / 10) % 10); head += static_cast<char>('0' + n % 10);
  head += payload;
  Bytes f(head.begin(), head.end());
  const auto crc = voltronic::wire_crc(f.data(), f.size());
  f.push_back(crc[0]); f.push_back(crc[1]); f.push_back('\r');
  return f;
}
static Bytes ask17(const char* command) {
  Bytes out;
  voltronic::pi17::build_command(command, out);
  return out;
}
static const char* kPi17Gs = "3200,2900,0050,0040,0528,076,-00125,2300,0000,0000,5000,0010,0000,0000,2301,0000,0000,5001,0035,0000,0000,038,045,025,0";
static const char* kPi17Ps = "00900,00300,,00000,00000,00000,00000,00800,00000,00000,00800,00850,00000,00000,00850,016,1,1,1,2,2,0";

static void test_pi17() {
  CHECK(ask17("GS") == hex("5E 50 30 30 33 47 53 0D") && ask17("MOD") == hex("5E 50 30 30 34 4D 4F 44 0D"));
  Bytes refused;
  CHECK(!voltronic::pi17::build_command("POP", refused) && refused.empty());
  voltronic::Status s;
  CHECK(voltronic::pi17::parse_gs(kPi17Gs, s) && near(s.battery_v, 52.8) && near(s.battery_discharge_a, 12.5) && near(s.grid_v, 230) && near(s.out_v, 230.1) && near(s.out_hz, 50.01) && near(s.heatsink_c, 45) && near(s.pv_v, 320));
  CHECK(voltronic::pi17::parse_ps(kPi17Ps, s) && near(s.pv_w, 1200) && near(s.out_w, 800) && near(s.out_va, 850) && near(s.load_percent, 16) && s.load_on && !s.charging);
  CHECK(!voltronic::pi17::parse_gs("1,2,3", s) && !voltronic::pi17::parse_ps("a,b", s));
  char mode = 0;
  std::vector<std::string> flags;
  CHECK(voltronic::pi17::parse_mod("06", mode) && mode == 'L' && !voltronic::pi17::parse_mod("07", mode));
  CHECK(voltronic::pi17::parse_ws("0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0", flags) && flags.size() == 2 && flags[0] == "battery_low" && flags[1] == "overload" && !voltronic::pi17::parse_ws("0,1", flags));

  // "auto" reaches it after the three other forms, and keeps it
  Poller p(config::Kind::kVoltronic, "solar-1", "infini-3", 5, 0, "auto");
  Unit unit;
  unit.answers = {{ask17("GS"), pi17_reply(kPi17Gs)}, {ask17("PS"), pi17_reply(kPi17Ps)}, {ask17("MOD"), pi17_reply("03")}, {ask17("WS"), pi17_reply("0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0")}};
  unit.run(p, 20000);
  CHECK(p.detail() == "pi17" && !unit.messages.empty() && has(unit.messages.front(), "\"pv_w\":1200") && has(unit.messages.front(), "\"mode\":\"battery\"") && has(unit.messages.front(), "battery_low") && !has(unit.messages.front(), "warnings_known"));
  Poller damaged(config::Kind::kVoltronic, "solar-1", "infini-3", 5, 0, "pi17");
  Unit bad;
  Bytes corrupt = pi17_reply(kPi17Gs);
  corrupt[9] ^= 1;
  bad.answers = {{ask17("GS"), corrupt}};
  bad.run(damaged, 3000);
  CHECK(bad.messages.empty() && damaged.stats().last_error == "crc" && has(damaged.diagnosis(), "asked: ^P003GS | 5E 50"));
}

static Bytes hex_answer(std::uint16_t id, std::uint32_t value, std::size_t size, std::uint8_t flags = 0) {
  std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(id), static_cast<std::uint8_t>(id >> 8), flags};
  for (std::size_t i = 0; i < size; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
  unsigned sum = 7;
  for (const std::uint8_t b : bytes) sum += b;
  bytes.push_back(static_cast<std::uint8_t>(0x55 - sum));
  static const char kDigits[] = "0123456789ABCDEF";
  Bytes out{':', '7'};
  for (const std::uint8_t b : bytes) { out.push_back(static_cast<std::uint8_t>(kDigits[b >> 4])); out.push_back(static_cast<std::uint8_t>(kDigits[b & 15])); }
  out.push_back('\n');
  return out;
}

static void test_vedirect_hex_and_inverter() {
  drivers::VedirectHexDriver d;
  Bytes next;
  CHECK(as_bytes(":78DED00D4\n") == d.begin() && !d.inverter());
  CHECK(give(d, as_bytes("\r\nV\t26000"), next) == drivers::Rx::kMore);                       // a text block in between
  CHECK(give(d, d.request(0xED8D), next) == drivers::Rx::kMore);                              // our own question heard back is too short to be an answer
  CHECK(give(d, hex_answer(0xED8D, 2648, 2), next) == drivers::Rx::kNext && next == d.request(0xED8F));
  CHECK(give(d, hex_answer(0xED8F, 0xFFF1, 2), next) == drivers::Rx::kNext);                  // -1.5 A
  CHECK(give(d, hex_answer(0x0FFF, 8760, 2), next) == drivers::Rx::kNext);
  CHECK(give(d, hex_answer(0xEDEC, 29815, 2), next) == drivers::Rx::kNext);                   // 25.0 C
  CHECK(give(d, hex_answer(0x1000, 200, 2), next) == drivers::Rx::kNext && next == d.request(0x0303));
  CHECK(give(d, hex_answer(0x0303, 41, 4), next) == drivers::Rx::kDone);
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 26.48) && near(m.current_a, -1.5) && m.soc_percent == 88 && m.has_temperature && near(m.temperature_c, 25.0) && near(m.full_capacity_ah, 200) && m.cycles == 41);
  // a damaged or refused voltage loses the cycle, any other register is left out
  d.begin();
  Bytes wrong = hex_answer(0xED8D, 2648, 2);
  wrong[8] = wrong[8] == '0' ? '1' : '0';
  CHECK(give(d, wrong, next) == drivers::Rx::kBad && std::string(d.error()) == "crc");
  d.begin();
  CHECK(give(d, hex_answer(0xED8D, 0, 2, 1), next) == drivers::Rx::kBad && std::string(d.error()) == "nak");
  d.begin();
  CHECK(d.timeout(next) == drivers::Rx::kBad);
  d.begin();
  give(d, hex_answer(0xED8D, 2648, 2), next);
  CHECK(d.timeout(next) == drivers::Rx::kNext && give(d, hex_answer(0x0FFF, 0xFFFF, 2), next) == drivers::Rx::kNext && d.timeout(next) == drivers::Rx::kNext);
  CHECK(d.timeout(next) == drivers::Rx::kNext && d.timeout(next) == drivers::Rx::kDone && d.out().modules.front().soc_percent < 0 && !d.out().modules.front().has_temperature);

  drivers::VedirectDriver phoenix(drivers::VedirectUse::kInverter);
  phoenix.begin();
  CHECK(phoenix.inverter() && give(phoenix, vedirect("\r\nV\t25600"), next) == drivers::Rx::kMore);
  CHECK(give(phoenix, vedirect("\r\nV\t25600\r\nAC_OUT_V\t23000\r\nAC_OUT_I\t12\r\nAC_OUT_S\t276\r\nMODE\t2\r\nAR\t0\r\nWARN\t256"), next) == drivers::Rx::kDone);
  CHECK(near(phoenix.out().status.out_v, 230) && near(phoenix.out().status.out_va, 276) && near(phoenix.out().status.battery_v, 25.6) && phoenix.out().mode == 'B' && phoenix.out().status.load_on);
  CHECK(phoenix.out().warnings.size() == 1 && phoenix.out().warnings.front() == "overload");
}

static void test_driver_through_the_poller() {
  Poller p(config::Kind::kJbd, "solar-1", "bms-1", 10, 0);
  CHECK(!p.is_inverter() && p.next_tx(0) == hex("DD A5 03 00 FF FD 77"));
  const Bytes basic = hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77");
  p.on_rx(basic.data(), basic.size(), 50);
  CHECK(p.next_tx(60) == hex("DD A5 04 00 FF FC 77"));
  std::string topic, payload;
  p.next_tx(4000);   // the cells are not answered: the reading goes out without them
  CHECK(p.take_message(1, topic, payload) && has(payload, "\"kind\":\"battery\"") && has(payload, "\"voltage_v\":58.88") && !has(payload, "cells_v") && !has(payload, "cell_min_v"));
  Poller v(config::Kind::kVedirect, "solar-1", "bmv-1", 3, 0, "monitor");
  CHECK(v.next_tx(0).empty() && v.busy());
  const Bytes block = vedirect("\r\nV\t26480\r\nI\t0");
  v.on_rx(block.data(), block.size(), 900);
  CHECK(v.take_message(1, topic, payload) && has(payload, "\"voltage_v\":26.48") && !has(payload, "temperature"));
}

// Noise with a fixed seed: no driver may crash, hang or hand out a reading with a number outside the contract.
static void test_noise() {
  std::uint32_t state = 12345;
  const auto next_byte = [&state] { state = state * 1103515245u + 12345u; return static_cast<std::uint8_t>(state >> 16); };
  for (const protocols::Protocol& p : protocols::kProtocols) {
    config::Kind kind = config::Kind::kRaw;
    config::kind_from_text(p.port, kind);
    const auto d = protocols::make_driver(kind, p.dialect);
    if (d == nullptr) continue;
    Bytes next;
    d->begin();
    for (int i = 0; i < 60000; ++i) {
      std::uint8_t b = next_byte();
      if (i % 97 == 0) { static const std::uint8_t kStarts[6] = {0x01, 0xA5, 0xDD, 0x4E, 0x0D, 0x57}; b = kStarts[(i / 97) % 6]; }
      const drivers::Rx r = d->feed(b, next);
      if (r == drivers::Rx::kDone) for (const pylontech::Module& m : d->out().modules) CHECK(m.voltage_v >= 0 && m.voltage_v <= 1000 && m.current_a >= -1000 && m.current_a <= 1000 && m.cells_v.size() <= 32);
      if (r == drivers::Rx::kDone || r == drivers::Rx::kBad) d->begin();
    }
    CHECK(d->out().modules.size() <= 1);
  }
}

// The refusals and odd answers of every driver.
static void test_odd_answers() {
  Bytes next;
  drivers::JbdDriver jbd;
  jbd.begin();
  CHECK(give(jbd, hex("DD 03 80 00 FF 80 77"), next) == drivers::Rx::kBad && std::string(jbd.error()) == "nak");
  jbd.begin();
  CHECK(give(jbd, hex("DD 03 00 00 00 00 66"), next) == drivers::Rx::kBad && std::string(jbd.error()) == "format");   // no end byte
  jbd.begin();
  CHECK(give(jbd, hex("DD 03 00 02 00 01 FF FD 77"), next) == drivers::Rx::kBad && std::string(jbd.error()) == "format");   // too short to be the basic information
  jbd.begin();
  CHECK(give(jbd, hex("55 DD 05 DD 04"), next) == drivers::Rx::kMore && jbd.timeout(next) == drivers::Rx::kBad);          // noise and an answer to something else

  drivers::DalyDriver daly_driver;
  daly_driver.begin();
  Bytes broken = daly(0x90, "02 10 02 10 75 30 03 69");
  broken.back() ^= 1;
  CHECK(give(daly_driver, broken, next) == drivers::Rx::kBad && std::string(daly_driver.error()) == "crc");
  daly_driver.begin();
  CHECK(give(daly_driver, daly(0x92, "00"), next) == drivers::Rx::kMore);                                                 // an answer to a question that was not asked
  CHECK(give(daly_driver, daly(0x90, "02 10 02 10 79 18 03 E8"), next) == drivers::Rx::kNext);
  CHECK(give(daly_driver, daly(0x93, "01 01 01 05 00 00 00 00"), next) == drivers::Rx::kNext);
  CHECK(daly_driver.timeout(next) == drivers::Rx::kNext && next[2] == 0x98);                                              // no counts: no cells and no sensors are asked
  CHECK(give(daly_driver, daly(0x98, "01"), next) == drivers::Rx::kNext && next[2] == 0x91);                             // no list of cells: their extremes
  CHECK(give(daly_driver, daly(0x91, "0D 02 01 0C E4 02 00 00"), next) == drivers::Rx::kNext && next[2] == 0x92);
  CHECK(daly_driver.timeout(next) == drivers::Rx::kDone && daly_driver.out().modules.front().system_alarm_state == "Alarm");
  CHECK(near(daly_driver.out().modules.front().cell_high_v, 3.33) && near(daly_driver.out().modules.front().cell_low_v, 3.3));
  CHECK(daly_driver.out().modules.front().base_state == "Charge" && daly_driver.out().modules.front().soc_percent == 100);

  drivers::JkDriver jk_driver;
  jk_driver.begin();
  CHECK(give(jk_driver, drivers::JkDriver::request(), next) == drivers::Rx::kMore);                                       // our own request heard back
  CHECK(give(jk_driver, hex("4E 00 4E 57 FF FF"), next) == drivers::Rx::kMore);                                           // noise and an impossible length
  jk_driver.begin();
  CHECK(give(jk_driver, jk_frame("85 40"), next) == drivers::Rx::kBad && std::string(jk_driver.error()) == "format");   // no voltage
  jk_driver.begin();
  CHECK(give(jk_driver, jk_frame("79 40 01"), next) == drivers::Rx::kBad);                                                // a list of cells longer than the frame
  jk_driver.begin();
  Bytes no_end = jk_frame("83 14 A0");
  no_end[no_end.size() - 5] = 0x00;
  CHECK(give(jk_driver, no_end, next) == drivers::Rx::kBad && std::string(jk_driver.error()) == "format");
  // version 1 of the current, the other identifiers walked over, a protection and an unknown identifier that ends the list
  jk_driver.begin();
  CHECK(give(jk_driver, jk_frame("83 14 A0 84 87 D0 86 02 87 00 05 89 00 00 00 01 8B 00 01 8C 00 03 8E 10 00 9D 01 9E 00 64 A9 10 AA 00 00 00 64 AB 01 AD 00 00 AE 01 B0 00 01 B1 14 "
                                 "B3 00 B8 00 BB 00 BE 0C 00 C0 01 FE 00"), next) == drivers::Rx::kDone);
  CHECK(near(jk_driver.out().modules.front().current_a, 20) && jk_driver.out().modules.front().protecting && jk_driver.out().modules.front().cycles == 5);
  CHECK(near(jk_driver.out().modules.front().full_capacity_ah, 100) && !jk_driver.out().modules.front().has_temperature);
  CHECK(drivers::jk::size_of(0x89) == 4 && drivers::jk::size_of(0xB2) == 10 && drivers::jk::size_of(0xB4) == 8 && drivers::jk::size_of(0xB5) == 4 && drivers::jk::size_of(0xB7) == 15 &&
        drivers::jk::size_of(0xB9) == 4 && drivers::jk::size_of(0xBA) == 24 && drivers::jk::size_of(0x10) == 0);

  drivers::ModbusDriver modbus(drivers::ModbusMap::kSmg2);
  modbus.begin();
  CHECK(give(modbus, hex("55 01 05 00 01 03 02"), next) == drivers::Rx::kMore);                                         // noise, another function, another length
  Bytes damaged = modbus_reply(std::vector<int>(34, 0));
  damaged.back() ^= 1;
  modbus.begin();
  CHECK(give(modbus, damaged, next) == drivers::Rx::kBad && std::string(modbus.error()) == "crc");
  modbus.begin();
  CHECK(modbus.timeout(next) == drivers::Rx::kBad && std::string(modbus.error()) == "timeout");
  modbus.begin();
  std::vector<int> odd_mode(34, 0);
  odd_mode[0] = 99;
  odd_mode[31] = 50;
  CHECK(give(modbus, modbus_reply(odd_mode), next) == drivers::Rx::kNext);
  CHECK(give(modbus, modbus_reply({0x8000, 0x0001}), next) == drivers::Rx::kNext && give(modbus, modbus_reply({0xFFFF, 0xFFFF}), next) == drivers::Rx::kDone);
  CHECK(modbus.out().mode == 0 && modbus.out().status.charging && !modbus.out().warnings.empty());
  drivers::ModbusDriver mppt(drivers::ModbusMap::kSrneMppt);
  mppt.begin();
  CHECK(give(mppt, modbus_reply({50, 270, 0, 0x8500, 0, 0, 0, 0, 0, 0}), next) == drivers::Rx::kNext);
  CHECK(give(mppt, modbus_reply({0, 0x0000, 0x0005}), next) == drivers::Rx::kDone && near(mppt.out().status.heatsink_c, -5) && mppt.out().warnings.size() == 2);

  drivers::VedirectDriver monitor(false);
  monitor.begin();
  CHECK(give(monitor, vedirect("\r\nV\t12000\r\nT\t-4\r\nAlarm\tON\r\nSOC\t---\r\nI\t\r\nA_VERY_LONG_LABEL_HERE\t123456789012345678901234567890"), next) == drivers::Rx::kDone);
  CHECK(monitor.out().modules.front().has_temperature && near(monitor.out().modules.front().temperature_c, -4) && monitor.out().modules.front().system_alarm_state == "Alarm" && monitor.out().modules.front().soc_percent < 0);
  CHECK(monitor.timeout(next) == drivers::Rx::kBad);
  drivers::VedirectDriver charger(true);
  charger.begin();
  CHECK(give(charger, vedirect("\r\nV\t26000"), next) == drivers::Rx::kMore);                                            // a monitor's block, not a charger's
  CHECK(give(charger, vedirect("\r\nV\t26000\r\nI\t-500\r\nCS\t2\r\nERR\t33\r\nVPV\t500"), next) == drivers::Rx::kDone);
  CHECK(charger.out().mode == 'F' && charger.out().warnings.size() == 1 && charger.out().warnings.front() == "charger_error_33" && near(charger.out().status.battery_a(), -0.5));
  charger.begin();
  CHECK(give(charger, vedirect("\r\nV\t26000\r\nCS\t0\r\nPPV\t999999999999"), next) == drivers::Rx::kDone && charger.out().mode == 'S');

  voltronic::Status status;
  CHECK(!voltronic::parse_qpigs_pi34("1 2 3", status) && !voltronic::parse_qpigs_pi34("x 53.2 1 1 1 1 1 1 1 1 01000000", status) && !voltronic::parse_qpigs_pi34("1 53.2 1 1 1 1 1 1 1 1 2", status));
  CHECK(!voltronic::parse_qpigs_pi41("1 2 3", status));
}

// The timeouts through the poller, and what the diagnosis says for the other kinds.
static void test_driver_timeouts_through_the_poller() {
  std::string topic, payload;
  Poller silent(config::Kind::kDaly, "solar-1", "daly-1", 10, 0);
  CHECK(!silent.next_tx(0).empty());
  CHECK(silent.next_tx(4000).empty() && silent.stats().last_error == "timeout" && !silent.take_message(1, topic, payload));
  CHECK(has(silent.diagnosis(), "asked: 0x90 voltage and current | A5 40 90"));
  Poller modbus(config::Kind::kModbus, "solar-1", "smg-1", 5, 0, "smg2");
  modbus.next_tx(0);
  const Bytes main_block = modbus_reply(std::vector<int>(34, 0));
  modbus.on_rx(main_block.data(), main_block.size(), 100);
  CHECK(!modbus.next_tx(110).empty());
  CHECK(!modbus.next_tx(4000).empty());                     // the faults are not answered: the warnings are asked
  CHECK(modbus.next_tx(8000).empty() && modbus.take_message(1, topic, payload) && has(payload, "\"warnings_known\":false") && modbus.is_inverter());
  Poller refused(config::Kind::kModbus, "solar-1", "smg-1", 5, 0, "smg2");
  refused.next_tx(0);
  Bytes damaged = main_block;
  damaged.back() ^= 1;
  refused.on_rx(damaged.data(), damaged.size(), 100);
  CHECK(refused.stats().last_error == "crc" && refused.stats().replies_bad == 1);
  Poller raw(config::Kind::kRaw, "solar-1", "raw-1", 0, 0);
  CHECK(raw.diagnosis().empty() && !raw.is_inverter() && raw.next_tx(0).empty());
  Poller battery(config::Kind::kPylontech, "solar-1", "us-1", 5, 0);
  CHECK(has(battery.diagnosis(), "nothing sent yet") && has(battery.diagnosis(), "last error: none"));
  battery.next_tx(0);
  CHECK(has(battery.diagnosis(), "asked:"));
}

static void test_catalogue() {
  for (const protocols::Protocol& p : protocols::kProtocols) {
    config::Kind kind = config::Kind::kRaw;
    CHECK(config::kind_from_text(p.port, kind));
    CHECK(config::info_of(kind).publishes);
    if (std::string(p.dialect) != "") CHECK(config::valid_dialect(kind, p.dialect));
    const bool driver = protocols::make_driver(kind, p.dialect) != nullptr;
    if (driver) CHECK(protocols::make_driver(kind, p.dialect)->inverter() == (std::string(p.message) == "inverter"));
  }
  CHECK(!config::valid_dialect(config::Kind::kVoltronic, "smg2") && !config::valid_dialect(config::Kind::kModbus, "pi30"));
}

int main() {
  test_jbd();
  test_daly();
  test_jk();
  test_modbus();
  test_vedirect();
  test_inverter_that_does_not_answer_everything();
  test_pi41_and_pi34();
  test_pi00_frames();
  test_pi00_port();
  test_pi16_and_variants();
  test_pi17();
  test_vedirect_hex_and_inverter();
  test_driver_through_the_poller();
  test_odd_answers();
  test_driver_timeouts_through_the_poller();
  test_catalogue();
  test_noise();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
