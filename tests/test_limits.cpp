// ARMOR-SOLAR - host tests of what the inverter parsers do with bad input: a reply that was damaged on the wire, cut short, shifted or full of absurd numbers.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The replies are typical ones of the public documents of these protocols, written by hand and then damaged on purpose; no inverter was read.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/json.hpp"
#include "../core/solar_config.hpp"
#include "../core/voltronic.hpp"
#include "../core/voltronic_pi18.hpp"

static int failures = 0;
static int checks = 0;
static int known_gaps = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)
// A behaviour that is wanted and not there yet: it is reported, and it does not fail the run; when the behaviour arrives the check passes and the report goes away.
#define KNOWN_GAP(condition)                                                          \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++known_gaps;                                                                   \
      std::printf("KNOWN GAP %s:%d  %s\n", __FILE__, __LINE__, #condition);           \
    }                                                                                 \
  } while (0)

using namespace armor::solar;
namespace config = armor::config;

static const char* kQpigs = "000.0 00.0 230.0 50.0 0161 0119 003 460 57.50 012 100 0069 0014 103.8 57.45 00000 00110110 00 00 00856 010";
static const char* kGs18 = "2320,500,2300,500,0161,0119,003,575,575,000,012,100,030,030,030,00856,00000,1038,0000,0,2,2,1,1,1,1,0";

static std::vector<std::uint8_t> reply(const std::string& text) {
  std::vector<std::uint8_t> frame{'('};
  frame.insert(frame.end(), text.begin(), text.end());
  const auto crc = voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]); frame.push_back(crc[1]); frame.push_back('\r');
  return frame;
}
static std::vector<std::uint8_t> reply18(const std::string& payload) {
  std::string head = "^D";
  const std::size_t declared = payload.size() + 3;
  head += static_cast<char>('0' + (declared / 100) % 10);
  head += static_cast<char>('0' + (declared / 10) % 10);
  head += static_cast<char>('0' + declared % 10);
  std::vector<std::uint8_t> frame(head.begin(), head.end());
  frame.insert(frame.end(), payload.begin(), payload.end());
  const auto crc = voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]); frame.push_back(crc[1]); frame.push_back('\r');
  return frame;
}

// ---- damage on the wire ------------------------------------------------------------------------------------------------------------------------

static void test_damaged_replies_are_refused() {
  const std::vector<std::uint8_t> good = reply(kQpigs);
  std::string text;
  CHECK(voltronic::parse_reply(good.data(), good.size(), text) && text == kQpigs);
  // every single bit of the reply turned over: the check catches it (the CRC sees every one-bit error; the delimiter rule only changes a CRC byte by one)
  std::size_t accepted = 0;
  for (std::size_t byte = 0; byte < good.size(); ++byte) {
    for (int bit = 0; bit < 8; ++bit) {
      std::vector<std::uint8_t> damaged = good;
      damaged[byte] = static_cast<std::uint8_t>(damaged[byte] ^ (1u << bit));
      if (voltronic::parse_reply(damaged.data(), damaged.size(), text)) ++accepted;
    }
  }
  CHECK(accepted == 0);
  // every shorter piece of it is refused, whatever its length
  for (std::size_t length = 0; length < good.size(); ++length) CHECK(!voltronic::parse_reply(good.data(), length, text));
  // something added at the end, or in front, is refused too
  std::vector<std::uint8_t> longer = good;
  longer.insert(longer.end() - 1, '0');
  CHECK(!voltronic::parse_reply(longer.data(), longer.size(), text));
  std::vector<std::uint8_t> shifted(good.begin() + 1, good.end());
  CHECK(!voltronic::parse_reply(shifted.data(), shifted.size(), text));
  CHECK(!voltronic::parse_reply(nullptr, 0, text) && text.empty());
  // the empty reply "()" with its CRC is a reply of nothing, and is accepted as nothing
  const std::vector<std::uint8_t> nothing = reply("");
  CHECK(voltronic::parse_reply(nothing.data(), nothing.size(), text) && text.empty());
  // the other dialect: the one-byte checksum, and the standard CRC still passes
  std::string revo_text = "B";
  std::vector<std::uint8_t> revo{'(', 'B'};
  unsigned sum = 1;
  for (const std::uint8_t b : revo) sum += b;
  revo.push_back(static_cast<std::uint8_t>(sum & 0xFF));
  revo.push_back('\r');
  CHECK(voltronic::parse_reply_revo(revo.data(), revo.size(), text) && text == revo_text);
  CHECK(!voltronic::parse_reply(revo.data(), revo.size(), text));   // the standard check does not take it
  revo[2] = static_cast<std::uint8_t>(revo[2] + 1);
  CHECK(!voltronic::parse_reply_revo(revo.data(), revo.size(), text) && text.empty());
  CHECK(voltronic::parse_reply_revo(good.data(), good.size(), text) && text == kQpigs);
  for (std::size_t length = 0; length < 4; ++length) CHECK(!voltronic::parse_reply_revo(good.data(), length, text));
}

static void test_revo_checksum_that_looks_like_the_end() {
  // the REVO checksum is a plain byte and may be 0x0D, the byte that ends a reply: the framer must not stop there
  std::vector<std::uint8_t> frame;
  bool found = false;
  for (int a = 'A'; a <= 'Z' && !found; ++a) {
    for (int b = 'A'; b <= 'Z' && !found; ++b) {
      for (int c = 'A'; c <= 'Z' && !found; ++c) {
        std::vector<std::uint8_t> candidate{'(', 'M', 'O', 'D', static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(c)};
        unsigned sum = 1;
        for (const std::uint8_t byte : candidate) sum += byte;
        if ((sum & 0xFF) != 0x0D) continue;
        candidate.push_back(0x0D);   // the checksum
        candidate.push_back('\r');   // the real end
        frame = candidate;
        found = true;
      }
    }
  }
  CHECK(found);
  std::string text;
  CHECK(voltronic::parse_reply_revo(frame.data(), frame.size(), text) && text.size() == 6);
  voltronic::ReplyFramer revo;
  revo.set_revo(true);
  std::vector<std::uint8_t> got;
  int whole = 0;
  for (const std::uint8_t byte : frame) if (revo.feed(byte, got)) { ++whole; CHECK(got == frame); }
  CHECK(whole == 1);
  // without the dialect on, the framer ends at the first CR, as the standard says
  voltronic::ReplyFramer standard;
  whole = 0;
  for (const std::uint8_t byte : frame) if (standard.feed(byte, got)) { ++whole; CHECK(got.size() == frame.size() - 1); }
  CHECK(whole == 1);
  // and the stream goes on afterwards
  const std::vector<std::uint8_t> next = reply("B");
  whole = 0;
  voltronic::ReplyFramer again;
  again.set_revo(true);
  for (const std::uint8_t byte : frame) again.feed(byte, got);
  for (const std::uint8_t byte : next) if (again.feed(byte, got)) { ++whole; CHECK(got == next); }
  CHECK(whole == 1);
}

static void test_pi18_damage() {
  const std::vector<std::uint8_t> good = reply18(kGs18);
  voltronic::pi18::Kind kind = voltronic::pi18::Kind::kNak;
  std::string payload;
  CHECK(voltronic::pi18::parse_reply(good.data(), good.size(), kind, payload) && kind == voltronic::pi18::Kind::kData && payload == kGs18);
  std::size_t accepted = 0;
  for (std::size_t byte = 0; byte < good.size(); ++byte) {
    for (int bit = 0; bit < 8; ++bit) {
      std::vector<std::uint8_t> damaged = good;
      damaged[byte] = static_cast<std::uint8_t>(damaged[byte] ^ (1u << bit));
      if (voltronic::pi18::parse_reply(damaged.data(), damaged.size(), kind, payload)) ++accepted;
    }
  }
  CHECK(accepted == 0);
  for (std::size_t length = 0; length < good.size(); ++length) CHECK(!voltronic::pi18::parse_reply(good.data(), length, kind, payload));
  // a declared length that is not a number
  std::vector<std::uint8_t> letters = good;
  letters[3] = 'x';
  const auto crc = voltronic::wire_crc(letters.data(), letters.size() - 3);
  letters[letters.size() - 3] = crc[0]; letters[letters.size() - 2] = crc[1];
  CHECK(!voltronic::pi18::parse_reply(letters.data(), letters.size(), kind, payload));
  // an acknowledgement with something after it is neither an ack nor data
  std::vector<std::uint8_t> ack{'^', '1', 0, 0, '\r'};
  const auto ack_crc = voltronic::wire_crc(ack.data(), 2);
  ack[2] = ack_crc[0]; ack[3] = ack_crc[1];
  CHECK(voltronic::pi18::parse_reply(ack.data(), ack.size(), kind, payload) && kind == voltronic::pi18::Kind::kAck && payload.empty());
  // the commands the node may send, and nothing else, with the length each declares
  std::vector<std::uint8_t> command;
  for (const char* name : {"GS", "MOD", "FWS", "PIRI", "ID", "VFW", "PI", "FLAG", "T"}) {
    CHECK(voltronic::pi18::allowed(name) && voltronic::pi18::build_command(name, command));
    const std::size_t declared = std::string(name).size() + 3;
    CHECK(command.size() == std::string(name).size() + 5 + 3 && command[0] == '^' && command[1] == 'P' && command[2] == '0' && command[3] == static_cast<std::uint8_t>('0' + declared / 10) &&
          command[4] == static_cast<std::uint8_t>('0' + declared % 10) && command.back() == '\r');
    const auto expected = voltronic::wire_crc(command.data(), command.size() - 3);
    CHECK(command[command.size() - 3] == expected[0] && command[command.size() - 2] == expected[1]);
  }
  for (const char* name : {"", "gs", "GS ", "GSX", "PCP0", "POP0", "PSDV56.0", "MCHGCR", "DAT2610181200", "^GS", "ET", "EY2018"}) CHECK(!voltronic::pi18::allowed(name) && !voltronic::pi18::build_command(name, command) && command.empty());
  // the framer starts over on '^', drops a runaway frame, and hands on only whole replies
  voltronic::pi18::ReplyFramer framer;
  std::vector<std::uint8_t> got;
  for (int i = 0; i < 400; ++i) framer.feed('a', got);   // noise outside a frame is ignored
  framer.feed('^', got);
  for (int i = 0; i < 400; ++i) CHECK(!framer.feed('a', got));
  CHECK(!framer.feed('\r', got));
  int whole = 0;
  for (const std::uint8_t byte : good) if (framer.feed(byte, got)) { ++whole; CHECK(got == good); }
  CHECK(whole == 1);
}

// ---- numbers that cannot be true ---------------------------------------------------------------------------------------------------------------

static void test_numbers() {
  double v = -1;
  CHECK(voltronic::detail::number("0", v) && v == 0 && voltronic::detail::number("230.0", v) && v == 230.0 && voltronic::detail::number("0161", v) && v == 161 && voltronic::detail::number("-12", v) && v == -12 &&
        voltronic::detail::number("+5", v) && v == 5 && voltronic::detail::number("5.", v) && v == 5 && voltronic::detail::number(".5", v) && v == 0.5 && voltronic::detail::number("-0.25", v) && v == -0.25);
  CHECK(voltronic::detail::number("123456789012", v) && v == 123456789012.0);
  v = 77;
  for (const char* bad : {"", "-", "+", ".", "-.", "1.2.3", "1e5", "0x10", "12a", "a12", " 1", "1 ", "--1", "+-1", "1,5", "NaN", "inf", "1234567890123", "\xC2\xB2"}) CHECK(!voltronic::detail::number(bad, v) && v == 77);
  std::uint32_t bits = 0;
  CHECK(voltronic::detail::bits("10110", 5, bits) && bits == 0x16 && voltronic::detail::bits("0", 1, bits) && bits == 0);
  CHECK(!voltronic::detail::bits("1011", 5, bits) && !voltronic::detail::bits("101100", 5, bits) && !voltronic::detail::bits("10x10", 5, bits) && voltronic::detail::bits("", 0, bits) && bits == 0);
  const std::vector<std::string> f = voltronic::split_fields("  a  b c   ");
  CHECK(f.size() == 3 && f[0] == "a" && f[2] == "c" && voltronic::split_fields("").empty() && voltronic::split_fields("   ").empty() && voltronic::split_fields("x").size() == 1);
  const std::vector<std::string> g = voltronic::pi18::split_commas("1,,3,");
  CHECK(g.size() == 4 && g[1].empty() && g[3].empty() && voltronic::pi18::split_commas("").size() == 1 && voltronic::pi18::split_commas(",").size() == 2);
  // a field that is empty is not a number: the status is refused, not read as zero
  voltronic::Status status;
  CHECK(!voltronic::pi18::parse_gs(std::string(kGs18).substr(0, 4) + ",,2300,500,0161,0119,003,575,575,000,012,100,030,030,030,00856,00000,1038,0000,0,2,2,1,1,1,1,0", status));
}

static void test_absurd_values_are_held_at_the_limit() {
  // a sensor that sends 255 for "no reading", or an overload above 200 %, would make the server throw the whole message away; the number is held at the limit instead
  voltronic::Status s;
  CHECK(voltronic::parse_qpigs("999.0 99.9 999.0 99.9 99999 99999 255 460 99.99 999 255 -99 999 9999.9 57.45 999 00110110", s));
  CHECK(s.grid_v == 600 && s.out_v == 600 && s.grid_hz <= 100 && s.out_hz <= 100 && s.load_percent == 200 && s.battery_percent == 100 && s.heatsink_c == -50 && s.pv_v == 1500 && s.pv_a == 500 &&
        s.battery_charge_a == 999 && s.out_va <= 100000 && s.out_w <= 100000);
  voltronic::Status revo;
  CHECK(voltronic::parse_qpigs_revo("999.0 99.9 999.0 99.9 99999 99999 255 460 99.99 999 255 -99 99999 9999.9 57.45 00000 00110110", revo));
  CHECK(revo.grid_v == 600 && revo.load_percent == 200 && revo.battery_percent == 100 && revo.heatsink_c == -50 && revo.pv_v == 1500 && revo.pv_w <= 100000 && revo.has_pv_w);
  voltronic::Status held;
  held.load_percent = 250; held.out_va = 1e9; held.battery_percent = -3; held.heatsink_c = 500; held.pv_w = -1;
  voltronic::hold_limits(held);
  CHECK(held.load_percent == 200 && held.out_va == 100000 && held.battery_percent == 0 && held.heatsink_c == 200 && held.pv_w == 0);
  CHECK(voltronic::hold(5, 0, 10) == 5 && voltronic::hold(-1, 0, 10) == 0 && voltronic::hold(11, 0, 10) == 10 && voltronic::hold(0, 0, 10) == 0 && voltronic::hold(10, 0, 10) == 10);
  // the same rule for the third dialect: a load of 255 % does not take the message down
  voltronic::Status pi18;
  std::string absurd = kGs18;
  absurd.replace(absurd.find("003,"), 4, "255,");
  CHECK(voltronic::pi18::parse_gs(absurd, pi18));
  KNOWN_GAP(pi18.load_percent <= 200);
  std::string huge = kGs18;
  huge.replace(huge.find("0161,"), 5, "99999999,");
  voltronic::Status huge_status;
  CHECK(voltronic::pi18::parse_gs(huge, huge_status));
  KNOWN_GAP(huge_status.out_va <= 100000);
}

// ---- the other replies --------------------------------------------------------------------------------------------------------------------------

static void test_warning_bits() {
  using voltronic::parse_qpiws;
  std::vector<std::string> active;
  CHECK(std::string(voltronic::warning_name(0)) == "pv_loss" && std::string(voltronic::warning_name(1)) == "inverter_fault" && std::string(voltronic::warning_name(16)) == "overload" && std::string(voltronic::warning_name(35)) == "battery_equalisation");
  CHECK(std::string(voltronic::warning_name(36)).empty() && std::string(voltronic::warning_name(1000)).empty());
  for (const std::size_t reserved : {std::size_t{13}, std::size_t{30}, std::size_t{34}}) CHECK(std::string(voltronic::warning_name(reserved)).empty());
  // each flag alone names itself (or nothing for a reserved bit), and the three weak-battery bits are one warning
  for (std::size_t bit = 0; bit < 36; ++bit) {
    std::string flags(36, '0');
    flags[bit] = '1';
    CHECK(parse_qpiws(flags, active));
    if (std::string(voltronic::warning_name(bit)).empty()) CHECK(active.empty());
    else CHECK(active.size() == 1 && active[0] == voltronic::warning_name(bit));
  }
  CHECK(parse_qpiws(std::string(31, '0') + "111" + "00", active) && active.size() == 1 && active[0] == "battery_weak");
  CHECK(parse_qpiws(std::string(36, '1'), active) && !active.empty());
  for (const std::string& name : active) CHECK(!name.empty());
  // the length: up to 36 characters, fewer is fine, 37 or an odd character is not
  CHECK(parse_qpiws("1", active) && active.size() == 1 && active[0] == "pv_loss");
  CHECK(!parse_qpiws(std::string(37, '0'), active) && active.empty());
  CHECK(!parse_qpiws("0000200", active) && !parse_qpiws(" 0000", active) && !parse_qpiws("0000\r", active));
}

static void test_other_replies() {
  using namespace voltronic;
  std::string serial, firmware;
  double watt_hours = -1;
  CHECK(parse_qid("92931701100510", serial) && serial == "92931701100510" && parse_qid("ABCDEFGH", serial) && parse_qid(std::string(20, '7'), serial));
  CHECK(!parse_qid("1234567", serial) && !parse_qid(std::string(21, '7'), serial) && !parse_qid("1234 5678", serial) && !parse_qid("1234567-", serial) && !parse_qid("", serial) && serial == std::string(20, '7'));
  CHECK(parse_qvfw("VERFW:00041.54", firmware) && firmware == "00041.54" && parse_qvfw("VERFW:1.2-rc_3", firmware));
  CHECK(!parse_qvfw("00041.54", firmware) && !parse_qvfw("VERFW:", firmware) && !parse_qvfw("VERFW:" + std::string(25, '1'), firmware) && !parse_qvfw("VERFW:1 2", firmware) && !parse_qvfw("VERFW:1;2", firmware));
  CHECK(parse_qet("00123456", watt_hours) && watt_hours == 123456 && parse_qet("0", watt_hours) && watt_hours == 0 && parse_qet(std::string(12, '9'), watt_hours) && watt_hours == 999999999999.0);
  CHECK(!parse_qet("", watt_hours) && !parse_qet(std::string(13, '1'), watt_hours) && !parse_qet("12.5", watt_hours) && !parse_qet("-1", watt_hours) && !parse_qet("12a", watt_hours));
  Pv2 second;
  CHECK(parse_qpigs2("03.1 327.3 01026", second) && second.current_a == 3.1 && second.voltage_v == 327.3 && second.power_w == 1026 && parse_qpigs2("03.1 327.3 01026 ", second));
  CHECK(!parse_qpigs2("03.1 327.3", second) && !parse_qpigs2("NAK", second) && !parse_qpigs2("-1 327.3 100", second) && !parse_qpigs2("501 100 100", second) && !parse_qpigs2("1 1501 100", second) &&
        !parse_qpigs2("1 100 100001", second) && !parse_qpigs2("a b c", second));
  CHECK(std::string(mode_name('P')) == "power_on" && std::string(mode_name('H')) == "power_saving" && std::string(mode_name('D')) == "shutdown" && std::string(mode_name('F')) == "fault" && std::string(mode_name('x')) == "unknown" &&
        std::string(mode_name(0)) == "unknown");
  char mode = 0;
  for (const char letter : {'P', 'S', 'L', 'B', 'F', 'H', 'D'}) CHECK(parse_qmod(std::string(1, letter), mode) && mode == letter);
  CHECK(!parse_qmod("p", mode) && !parse_qmod("\r", mode) && !parse_qmod(" ", mode));
  CHECK(std::string(battery_type_name(0)) == "agm" && std::string(battery_type_name(2)) == "user" && std::string(battery_type_name(3)).empty() && std::string(battery_type_name(-1)).empty() &&
        std::string(input_range_name(1)) == "ups" && std::string(input_range_name(2)).empty() && std::string(output_priority_name(2)) == "sbu" && std::string(output_priority_name(3)).empty() &&
        std::string(charger_priority_name(3)) == "solar_only" && std::string(charger_priority_name(4)).empty() && std::string(charger_priority_name(-1)).empty());
  // the ratings: a non-number anywhere is a refusal, extra fields at the end are ignored
  Ratings r;
  const std::string rated = "230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 1 1 01 01 1 0 54.0 0 1";
  CHECK(parse_qpiri(rated, r) && r.charger_priority == 1);
  CHECK(!parse_qpiri("230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 x 30 060 0 1 1", r));
  CHECK(!parse_qpiri("230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 1", r));   // one field short
  CHECK(parse_qpiri("230.0 21.7 230.0 50.0 21.7 5000 5000 48.0 46.0 42.0 56.4 54.0 0 30 060 0 1 1", r) && r.battery_type == 0);
  // one unit of a parallel system
  ParallelUnit unit;
  const std::string good = "1 92931701100510 B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110 1 2 060 120 10 04 000";
  CHECK(parse_qpgs(good, unit) && unit.serial == "92931701100510" && unit.mode == 'B' && unit.fault == "00" && unit.out_w == 141 && unit.battery_percent == 100 && unit.total_out_w == 312);
  CHECK(!parse_qpgs("0" + good.substr(1), unit));   // a unit that is not there
  CHECK(!parse_qpgs("1 92931701100510 B 0 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110", unit));   // a one-digit fault code
  CHECK(!parse_qpgs("1 92931701100510 BB 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110", unit));
  CHECK(!parse_qpgs("1 92931701100510 B 0x 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110", unit));
  CHECK(!parse_qpgs("1 " + std::string(25, '1') + " B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110", unit));
  CHECK(!parse_qpgs("1 92931701100510 B 00 700.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 10100110", unit));   // 700 V on the grid
  CHECK(!parse_qpgs("1 92931701100510 B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 101 083.3 002 00574 00312 003 10100110", unit));   // 101 % of the battery
  CHECK(!parse_qpgs("1 92931701100510 B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 -03 10100110", unit));   // a negative total
  CHECK(!parse_qpgs("1 92931701100510 B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002", unit));   // cut short
  CHECK(!parse_qpgs("", unit));
}

// ---- the broker password belongs to its broker ------------------------------------------------------------------------------------------------------

// The password stored for a broker is that broker's: pointing the node at another host without typing a password must not send the stored one there, or a stolen panel session could read it
// out by naming a broker of its own. The settings of the radar node do this; these settings do not yet.
static config::Problems read(config::Settings& s, const std::string& text) {
  armor::json::Value document;
  const bool ok = armor::json::parse(text, document);
  CHECK(ok);
  config::Problems problems;
  config::read_settings(document, s, problems);
  return problems;
}

static void test_password_follows_its_broker() {
  config::Settings s = config::default_settings("a1b2c3");
  s.mqtt.enabled = true; s.mqtt.uri = "mqtt://broker.local:1883"; s.mqtt.username = "node"; s.mqtt.password = "secret-pass";
  config::Settings same = s;
  CHECK(read(same, "{\"mqtt\":{\"uri\":\"mqtt://BROKER.local:1884\"}}").empty() && same.mqtt.password == "secret-pass");
  config::Settings typed = s;
  CHECK(read(typed, "{\"mqtt\":{\"uri\":\"mqtt://elsewhere.example\",\"password\":\"typed-pass\"}}").empty() && typed.mqtt.password == "typed-pass");
  config::Settings moved = s;
  CHECK(read(moved, "{\"mqtt\":{\"uri\":\"mqtt://elsewhere.example:1883\"}}").empty());
  KNOWN_GAP(moved.mqtt.password.empty());
  s.mqtt.backup = {{"mqtt://b1.local", "u1", "p1"}};
  config::Settings slot = s;
  CHECK(read(slot, "{\"mqtt\":{\"backup\":[{\"uri\":\"mqtt://elsewhere.example\",\"username\":\"u1\"}]}}").empty());
  KNOWN_GAP(slot.mqtt.backup.size() == 1 && slot.mqtt.backup[0].password.empty());
}

int main() {
  test_damaged_replies_are_refused();
  test_revo_checksum_that_looks_like_the_end();
  test_pi18_damage();
  test_numbers();
  test_absurd_values_are_held_at_the_limit();
  test_warning_bits();
  test_other_replies();
  test_password_follows_its_broker();
  std::printf("%d checks, %d failures, %d known gaps\n", checks, failures, known_gaps);
  return failures == 0 ? 0 : 1;
}
