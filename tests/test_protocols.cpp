// ARMOR-NODE-SOLAR - host tests of the protocol catalogue and drivers, and of what keeps an inverter port reading when part of its answers fails.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The JBD frames and the Modbus requests are the examples printed in the makers' documents; the DALY, JK, SMG II, SRNE and VE.Direct frames are built here from the layout those
// documents describe. No equipment has been connected.
#include <cmath>
#include <cstdio>
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
  CHECK(give(d, hex("DD 04 00 1E 0F 66 0F 63 0F 63 0F 64 0F 3E 0F 63 0F 37 0F 5B 0F 65 0F 3B 0F 63 0F 63 0F 3C 0F 66 0F 3D F9 F9 77"), next) == drivers::Rx::kDone);
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 58.88) && near(m.current_a, 0) && near(m.capacity_ah, 7.2) && near(m.full_capacity_ah, 10) && m.soc_percent == 72);
  CHECK(m.cells_v.size() == 15 && near(m.cells_v.front(), 3.942) && near(m.cells_v.back(), 3.901) && m.cell_count == 15);
  CHECK(m.temperatures_c.size() == 2 && near(m.temperatures_c[0], 20.3) && near(m.temperatures_c[1], 21.5) && !m.protecting);
  // a damaged frame is refused, and without the cells the basic information is still a reading
  d.begin();
  CHECK(give(d, hex("DD 03 00 1B 17 01 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next) == drivers::Rx::kBad && std::string(d.error()) == "crc");
  d.begin();
  CHECK(give(d, hex("DD 03 00 1B 17 00 00 00 02 D0 03 E8 00 00 20 78 00 00 00 00 00 00 10 48 03 0F 02 0B 76 0B 82 FB FF 77"), next) == drivers::Rx::kNext);
  CHECK(d.timeout(next) == drivers::Rx::kDone && d.out().modules.front().cells_v.empty());
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
  CHECK(give(d, daly(0x96, "01 41 43 00 00 00 00 00"), next) == drivers::Rx::kNext && next[2] == 0x98);
  CHECK(give(d, daly(0x98, "00 00 00 00 00 00 00 00"), next) == drivers::Rx::kDone);
  const pylontech::Module& m = d.out().modules.front();
  CHECK(near(m.voltage_v, 52.8) && near(m.current_a, -12.5) && m.soc_percent == 87 && m.base_state == "Dischg" && near(m.capacity_ah, 50));
  CHECK(m.cells_v.size() == 4 && near(m.cells_v[3], 3.303) && m.temperatures_c.size() == 2 && near(m.temperatures_c[0], 25) && m.system_alarm_state == "Normal");
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
  CHECK(give(d, jk_frame("79 06 01 0C E4 02 0C EE 80 00 19 81 00 65 82 00 16 83 14 A0 84 25 1C 85 40 8A 00 02 8B 00 00"), next) == drivers::Rx::kDone);
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
  CHECK(give(srne, modbus_reply(ac), next) == drivers::Rx::kDone);
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
  test_driver_through_the_poller();
  test_catalogue();
  test_noise();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
