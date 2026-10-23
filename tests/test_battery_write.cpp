// ARMOR-NODE-SOLAR - host tests of writing to a Pylontech battery: the typed operations and their ranges, the preconditions, the one-time token, the XMODEM-1K sender, the chip and image checks and
// every operation run against the simulated console (battery_bench.hpp). Nothing has been tried on a real battery; the service password in these tests is a made-up value.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "battery_bench.hpp"
#include "../core/battery_ops.hpp"
#include "../core/battery_stage.hpp"
#include "../core/console_probe.hpp"
#include "../core/solar_config.hpp"

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
using namespace armor::solar::battery;
namespace json = armor::json;
namespace config = armor::config;

static json::Value J(const std::string& text) {
  json::Value v;
  const bool ok = json::parse(text, v);
  if (!ok) std::printf("bad test json: %s\n", text.c_str());
  return v;
}

static Identity identity_of(const bench::Battery& b) {
  Identity id;
  id.valid = true; id.model = b.model; id.board = b.board; id.serial = b.serial; id.soft_version = b.soft; id.specification = b.spec; id.capacity_ah = b.model == "US3000C" ? 74 : b.model == "US2000C" ? 50 : 100; id.cells = b.cells;
  return id;
}

static Request request(Op op, const std::string& params, const Identity& id) {
  Request r;
  r.op = op; r.params = J(params); r.identity = id;
  r.local_year = 2026; r.local_month = 10; r.local_day = 19; r.local_hour = 14; r.local_minute = 5; r.local_second = 9;
  return r;
}

static std::string why_not(Op op, const std::string& params, const Identity& id) {
  Plan plan; std::string why;
  return build_plan(request(op, params, id), plan, why) ? "" : why;
}

static bool all_lack(const std::string& text, const std::string& secret) { return text.find(secret) == std::string::npos; }
static bool result_lacks_secret(const RunResult& r, const std::vector<Capture>& caps) {
  const std::string pw = bench::kDummyPassword;
  if (!all_lack(r.transcript, pw) || !all_lack(r.before, pw) || !all_lack(r.after, pw) || !all_lack(r.code, pw)) return false;
  for (const Capture& c : caps) if (!all_lack(c.text, pw)) return false;
  return true;
}

static void run_plan(const Plan& plan, bench::Battery& b, RunResult& out, const std::vector<std::uint8_t>* image = nullptr, std::vector<Capture>* caps = nullptr, std::string pw = bench::kDummyPassword) {
  Runner runner(plan, pw, image);
  bench::drive(runner, b);
  out = runner.result();
  if (caps != nullptr) *caps = runner.captures();
  CHECK(runner.finished());
  CHECK(result_lacks_secret(out, runner.captures()));
}

// ---- XMODEM ----------------------------------------------------------------------------------------------------------------------------------

static void test_xmodem() {
  using namespace xmodem;
  const std::string digits = "123456789";
  CHECK(crc16(reinterpret_cast<const std::uint8_t*>(digits.data()), digits.size()) == 0x31C3);   // the CRC-16/XMODEM check value
  std::vector<std::uint8_t> image(2500);
  for (std::size_t i = 0; i < image.size(); ++i) image[i] = static_cast<std::uint8_t>(i);
  CHECK(packet_count(image.size()) == 3 && packet_count(1024) == 1 && packet_count(1025) == 2);
  const std::vector<std::uint8_t> p3 = packet(image, 2);
  CHECK(p3.size() == 1029 && p3[0] == kStx && p3[1] == 3 && p3[2] == 0xFC);
  CHECK(p3[3 + 2500 - 2048 - 1] == image[2499] && p3[3 + 2500 - 2048] == kPad && p3[3 + 1023] == kPad);   // the last block is padded with 0x1A
  CHECK((static_cast<unsigned>(p3[1027]) << 8 | p3[1028]) == crc16(p3.data() + 3, 1024));
  // the block number wraps past 255
  std::vector<std::uint8_t> big(kBlock * 260, 1);
  CHECK(packet(big, 254)[1] == 255 && packet(big, 255)[1] == 0 && packet(big, 256)[1] == 1);

  // a receiver that accepts everything
  Sender s(image);
  s.begin(0);
  CHECK(s.next_tx(10).empty() && !s.committed());
  const std::uint8_t c = 'C';
  s.on_rx(&c, 1, 20);
  std::uint64_t now = 20;
  std::size_t packets = 0, eots = 0;
  for (int guard = 0; guard < 20 && !s.done() && !s.failed(); ++guard) {
    const std::vector<std::uint8_t> out = s.next_tx(now);
    if (out.size() == 1029) ++packets; else if (out.size() == 1 && out[0] == kEot) ++eots;
    if (out.empty()) break;
    const std::uint8_t ack = kAck;
    s.on_rx(&ack, 1, now += 5);
  }
  CHECK(s.done() && packets == 3 && eots == 1 && s.committed() && s.sent() == 3);
  // the receiver NAKs the EOT once, as the protocol allows
  {
    std::vector<std::uint8_t> small(100, 7);
    Sender t(small);
    t.begin(0);
    t.on_rx(&c, 1, 1);
    t.next_tx(2);
    const std::uint8_t ack = kAck, nak = kNak;
    t.on_rx(&ack, 1, 3);
    CHECK(t.next_tx(4).size() == 1);
    t.on_rx(&nak, 1, 5);
    CHECK(t.next_tx(6).size() == 1 && !t.done());
    t.on_rx(&ack, 1, 7);
    CHECK(t.done() && t.resends() == 1);
  }
  // twenty resends are allowed, the twenty-first stops it
  {
    Sender t(image);
    t.begin(0);
    t.on_rx(&c, 1, 1);
    t.next_tx(2);
    const std::uint8_t nak = kNak;
    int sent = 0;
    for (int i = 0; i < 25 && !t.failed(); ++i) { t.on_rx(&nak, 1, 3 + i); if (!t.next_tx(3 + i).empty()) ++sent; }
    CHECK(t.failed() && t.error() == "resends_used_up" && sent == 20);
  }
  // two CAN stop it; one does not
  {
    Sender t(image);
    t.begin(0);
    t.on_rx(&c, 1, 1);
    t.next_tx(2);
    const std::uint8_t can = kCan, ack = kAck;
    t.on_rx(&can, 1, 3);
    CHECK(!t.failed());
    t.on_rx(&ack, 1, 4);
    CHECK(!t.failed());
    const std::uint8_t two[2] = {kCan, kCan};
    t.on_rx(two, 2, 5);
    CHECK(t.failed() && t.error() == "receiver_cancelled");
  }
  // silence: no 'C' in five seconds fails before anything is sent; a receiver that wants the checksum is refused
  {
    Sender t(image);
    t.begin(0);
    CHECK(t.next_tx(4000).empty() && !t.failed());
    CHECK(t.next_tx(5100).empty() && t.failed() && t.error() == "no_crc_request" && !t.committed());
    Sender u(image);
    u.begin(0);
    const std::uint8_t nak = kNak;
    u.on_rx(&nak, 1, 1);
    CHECK(u.failed() && u.error() == "checksum_mode_requested" && !u.committed());
    Sender silent(image);
    silent.begin(0);
    silent.on_rx(&c, 1, 1);
    silent.next_tx(2);
    std::uint64_t at = 2;
    int resent = 0;
    while (!silent.failed() && at < 200000) { at += 2001; if (!silent.next_tx(at).empty()) ++resent; }
    CHECK(silent.failed() && silent.error() == "receiver_silent" && resent == 20);
    Sender none(std::vector<std::uint8_t>{});
    none.begin(0);
    CHECK(none.failed());
  }
}

// ---- the identity, the chip and the image --------------------------------------------------------------------------------------------------------

static void test_identity_and_chip() {
  bench::Battery b;
  b.model = "US2000C"; b.board = "NF4.E2"; b.serial = "H221014C32213244"; b.soft = "V2.4";
  const std::string text = "Device address      : 1\r\nManufacturer        : Pylon\r\nDevice name         : US2000C\r\nBoard version       : V10R04\r\nBoard               : NF4.E2\r\nMain Soft version   : B69.20.0.0\r\nSoft  version       : V2.4\r\nBoot  version       : V1.0\r\nBarcode             : H221014C32213244\r\nSpecification       : 48V/50AH\r\nCell Number         : 15\r\n";
  const Identity id = parse_info(text);
  CHECK(id.valid && id.model == "US2000C" && id.board == "NF4.E2" && id.board_version == "V10R04" && id.soft_version == "V2.4" && id.main_soft_version == "B69.20.0.0");
  CHECK(id.serial == "H221014C32213244" && id.specification == "48V/50AH" && id.capacity_ah == 50 && id.cells == 15);
  CHECK(chip_of(id) == Chip::kNewer);   // C3 at the 8th and 9th characters, Board NF4.E2
  CHECK(!parse_info("nothing here").valid && !parse_info("").valid);
  const Identity blank = parse_info("Device name         : US3000C\nBarcode             :                 \nSpecification       : 48V/74AH\n");
  CHECK(blank.valid && blank.serial.empty() && chip_of(blank) == Chip::kUnknown && blank.capacity_ah == 74);
  CHECK(parse_total_coul_mah(" Total Coul: 50000 mAH\r\nCommand completed successfully") == 50000 && parse_total_coul_mah("nothing") == -1);

  Identity older; older.valid = true; older.model = "US3000C"; older.serial = "PPTBH02200409223";
  CHECK(chip_of(older) == Chip::kOlder);
  Identity mismatch = older; mismatch.board = "NF4.E2";
  CHECK(chip_of(mismatch) == Chip::kUnknown);              // the board says newer, the serial says older
  Identity mismatch2 = id; mismatch2.board = "";
  CHECK(chip_of(mismatch2) == Chip::kUnknown);             // the serial says newer, the battery says nothing
  Identity strange = older; strange.board = "XYZ";
  CHECK(chip_of(strange) == Chip::kUnknown);
  Identity us5; us5.valid = true; us5.model = "US5000"; us5.serial = "PPTBH02C50409223"; us5.board = "NF4.E3";
  CHECK(chip_of(us5) == Chip::kNewer);
  us5.board = "";
  CHECK(chip_of(us5) == Chip::kUnknown);
  us5.serial = "PPTBH02200409223";
  CHECK(chip_of(us5) == Chip::kOlder);
  Identity other = older; other.model = "US2KBPL";
  CHECK(chip_of(other) == Chip::kUnknown);
  CHECK(!valid_serial("short") && !valid_serial("has space 12345") && !valid_serial(std::string(30, 'A')) && valid_serial("H221014C32213244") && valid_serial("ABC-12345_xyz"));
}

static void test_image_checks() {
  Identity older; older.valid = true; older.model = "US3000C"; older.serial = "PPTBH02200409223";
  Identity newer; newer.valid = true; newer.model = "US3000C"; newer.serial = "H221014C32213244"; newer.board = "NF4.E2";
  Identity us5old; us5old.valid = true; us5old.model = "US5000"; us5old.serial = "PPTBH02200409223";
  Identity us5new; us5new.valid = true; us5new.model = "US5000"; us5new.serial = "PPTBH02C50409223"; us5new.board = "NF4.E3";
  const auto img = [](const char* board, int major, int minor) { return bench::make_image(board, major, minor); };
  CHECK(check_image(img("US2000C", 2, 4), older).ok);
  CHECK(!check_image(img("US2000C_NF4", 2, 4), older).ok && check_image(img("US2000C_NF4", 2, 4), older).code == "board_mismatch");
  CHECK(check_image(img("US2000C_NF4", 2, 4), newer).ok);
  CHECK(!check_image(img("US2000C", 2, 4), newer).ok);             // the image of the older chip on a newer one: refused
  CHECK(check_image(img("US5000", 1, 4), us5old).ok && !check_image(img("US2000C", 1, 4), us5old).ok);
  CHECK(check_image(img("E32000C_NF4", 1, 7), us5new).ok && !check_image(img("E32000C_NF4", 2, 0), us5new).ok);   // below 2.0 one board, 2.0 and up the other
  CHECK(check_image(img("US2000C_NF4", 2, 0), us5new).ok && !check_image(img("US2000C_NF4", 1, 9), us5new).ok);
  CHECK(check_image(img("US2000C_NF4", 2, 4), newer).major == 2 && check_image(img("US2000C_NF4", 2, 4), newer).minor == 4);
  Identity unknown = older; unknown.board = "NF4.E2";             // serial and board disagree
  CHECK(check_image(img("US2000C", 2, 4), unknown).code == "chip_unknown");
  Identity bms = older; bms.model = "US2KBPL";
  CHECK(check_image(img("US2000C", 2, 4), bms).code == "unsupported_model");
  CHECK(check_image(std::vector<std::uint8_t>(100, 0), older).code == "image_size" && check_image(std::vector<std::uint8_t>(600'000, 0), older).code == "image_size");
  std::vector<std::uint8_t> nameless = bench::make_image("", 2, 4);
  CHECK(check_image(nameless, older).code == "image_header");
  std::vector<std::uint8_t> garbled = img("US2000C", 2, 4);
  garbled[0x418] = 0x07;
  CHECK(check_image(garbled, older).code == "image_header");
  Identity nothing;
  CHECK(check_image(img("US2000C", 2, 4), nothing).code == "unsupported_model");
  CHECK(crc32(reinterpret_cast<const std::uint8_t*>("123456789"), 9) == 0xCBF43926u);
}

// ---- the typed operations: values ---------------------------------------------------------------------------------------------------------------

static void test_values_are_bounded() {
  bench::Battery b;
  const Identity id = identity_of(b);
  // the clock
  CHECK(why_not(Op::kSetClock, "{}", id).empty());
  Plan plan; std::string why;
  Request r = request(Op::kSetClock, "{}", id);
  r.local_year = 2019;
  CHECK(!build_plan(r, plan, why) && why == refusal::kInvalidParams);
  r.local_year = 2026; r.local_month = 13;
  CHECK(!build_plan(r, plan, why));
  r.local_month = 2; r.local_hour = 24;
  CHECK(!build_plan(r, plan, why));
  r.local_hour = 23; r.local_second = 60;
  CHECK(!build_plan(r, plan, why));
  r = request(Op::kSetClock, "{}", id);
  CHECK(build_plan(r, plan, why) && plan.canonical == "set_clock");
  bool wrote_time = false;
  for (const Step& s : plan.steps) if (s.write) { wrote_time = s.send == "time 26 10 19 14 05 09"; }
  CHECK(wrote_time);

  // clear
  CHECK(why_not(Op::kClearData, "{\"which\":\"event\"}", id).empty() && why_not(Op::kClearData, "{\"which\":\"history\"}", id).empty());
  CHECK(why_not(Op::kClearData, "{\"which\":\"all\"}", id) == refusal::kInvalidParams && why_not(Op::kClearData, "{}", id) == refusal::kInvalidParams);

  // the capacity is the maker's value for the model, and only that
  CHECK(build_plan(request(Op::kSocCapacity, "{}", id), plan, why) && plan.canonical == "soc_capacity 74");
  Identity us2 = id; us2.model = "US2000C"; us2.capacity_ah = 50;
  CHECK(build_plan(request(Op::kSocCapacity, "{}", us2), plan, why) && plan.canonical == "soc_capacity 50");
  Identity up5 = id; up5.model = "UP5000"; up5.capacity_ah = 100;
  CHECK(build_plan(request(Op::kSocCapacity, "{}", up5), plan, why) && plan.canonical == "soc_capacity 100");
  Identity us5 = id; us5.model = "US5000"; us5.capacity_ah = 100;
  CHECK(why_not(Op::kSocCapacity, "{}", us5) == refusal::kNoOfficialValue);                 // the procedure names no value for this one
  Identity wrongspec = id; wrongspec.capacity_ah = 50;                                      // the battery says 50 Ah but is a US3000C
  CHECK(why_not(Op::kSocCapacity, "{}", wrongspec) == refusal::kNoOfficialValue);
  CHECK(why_not(Op::kSocCapacity, "{\"ah\":999}", id).empty());                             // an Ah that is sent is ignored: the value comes from the model

  // limits: whole groups, inside the bounds, in order
  const std::string good_module = "{\"group\":\"module_voltage\",\"values\":{\"pov\":58000,\"povr\":55000,\"phv\":57600,\"phvr\":55500,\"plv\":47000,\"plvr\":48000,\"puv\":44000,\"puvr\":45000,\"pslp\":42000}}";
  CHECK(build_plan(request(Op::kLimitsSet, good_module, id), plan, why));
  int config_lines = 0; bool saved = false;
  for (const Step& s : plan.steps) { if (s.send.rfind("config ", 0) == 0 && s.send != "config save") ++config_lines; if (s.send == "config save") saved = true; }
  CHECK(config_lines == 9 && saved);
  const auto with = [&](const std::string& key, const std::string& value) {
    std::string t = good_module;
    const std::size_t at = t.find("\"" + key + "\":");
    const std::size_t end = t.find_first_of(",}", at);
    t.replace(at, end - at, "\"" + key + "\":" + value);
    return t;
  };
  CHECK(why_not(Op::kLimitsSet, with("pov", "65001"), id) == refusal::kInvalidParams);       // above the absolute bound
  CHECK(why_not(Op::kLimitsSet, with("pslp", "19999"), id) == refusal::kInvalidParams);      // below it
  CHECK(why_not(Op::kLimitsSet, with("pov", "50000"), id) == refusal::kInvalidParams);       // the over-voltage protection below the high-voltage alarm
  CHECK(why_not(Op::kLimitsSet, with("puv", "50000"), id) == refusal::kInvalidParams);       // the under-voltage protection above the low-voltage alarm
  CHECK(why_not(Op::kLimitsSet, with("phv", "57600.5"), id) == refusal::kInvalidParams);     // not a whole number
  CHECK(why_not(Op::kLimitsSet, with("phv", "\"57600\""), id) == refusal::kInvalidParams);   // a text
  std::string missing = good_module;
  missing.erase(missing.find(",\"pslp\":42000"), 13);
  CHECK(why_not(Op::kLimitsSet, missing, id) == refusal::kInvalidParams);                    // a group is accepted whole only
  std::string extra = good_module; extra.insert(extra.size() - 2, ",\"zzz\":1");
  CHECK(why_not(Op::kLimitsSet, extra, id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kLimitsSet, "{\"group\":\"current\",\"values\":{}}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kLimitsSet, "{}", id) == refusal::kInvalidParams);
  const std::string good_temp = "{\"group\":\"temperature\",\"values\":{\"cot\":60000,\"cotr\":55000,\"cht\":50000,\"chtr\":45000,\"clt\":5000,\"cltr\":8000,\"cut\":-5000,\"cutr\":-2000}}";
  CHECK(build_plan(request(Op::kLimitsSet, good_temp, id), plan, why));
  bool wire_names = false;
  for (const Step& s : plan.steps) if (s.send == "config bot 60000") wire_names = true;       // the console's name for the over-temperature protection
  CHECK(wire_names);
  CHECK(why_not(Op::kLimitsSet, "{\"group\":\"temperature\",\"values\":{\"cot\":100001,\"cotr\":55000,\"cht\":50000,\"chtr\":45000,\"clt\":5000,\"cltr\":8000,\"cut\":-5000,\"cutr\":-2000}}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kLimitsSet, "{\"group\":\"temperature\",\"values\":{\"cot\":60000,\"cotr\":55000,\"cht\":50000,\"chtr\":45000,\"clt\":5000,\"cltr\":8000,\"cut\":-40001,\"cutr\":-2000}}", id) == refusal::kInvalidParams);
  const std::string good_cell = "{\"group\":\"cell_voltage\",\"values\":{\"bov\":3800,\"bovr\":3700,\"bhv\":3650,\"bhvr\":3600,\"blv\":3100,\"blvr\":3200,\"buv\":2900,\"buvr\":3000,\"bslp\":2700}}";
  CHECK(build_plan(request(Op::kLimitsSet, good_cell, id), plan, why));
  CHECK(why_not(Op::kLimitsSet, "{\"group\":\"cell_voltage\",\"values\":{\"bov\":7001,\"bovr\":3700,\"bhv\":3650,\"bhvr\":3600,\"blv\":3100,\"blvr\":3200,\"buv\":2900,\"buvr\":3000,\"bslp\":2700}}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kLimitsDefault, "{}", id).empty());

  // protections
  CHECK(why_not(Op::kProtectionsSet, "{\"items\":{\"bov\":\"en\",\"coc\":\"en\"}}", id).empty());
  CHECK(why_not(Op::kProtectionsSet, "{\"items\":{\"bov\":\"dis\"}}", id) == refusal::kRiskNotAccepted);   // switching a protection off needs the risk accepted
  CHECK(why_not(Op::kProtectionsSet, "{\"items\":{\"bov\":\"dis\"},\"accept_risk\":true}", id).empty());
  CHECK(why_not(Op::kProtectionsSet, "{\"items\":{\"nope\":\"en\"}}", id) == refusal::kInvalidParams && why_not(Op::kProtectionsSet, "{\"items\":{\"bov\":\"maybe\"}}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kProtectionsSet, "{\"items\":{}}", id) == refusal::kInvalidParams && why_not(Op::kProtectionsSet, "{}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kProtectionsDefault, "{}", id).empty());

  // outputs
  for (const char* o : {"cfet", "dfet", "buzz", "heat"}) for (const char* s : {"on", "off"}) CHECK(why_not(Op::kOutputSet, std::string("{\"output\":\"") + o + "\",\"state\":\"" + s + "\"}", id).empty());
  CHECK(why_not(Op::kOutputSet, "{\"output\":\"mostest\",\"state\":\"on\"}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kOutputSet, "{\"output\":\"cfet\",\"state\":\"toggle\"}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kOutputSet, "{\"output\":\"cfet; shut\",\"state\":\"on\"}", id) == refusal::kInvalidParams);

  // the serial number
  CHECK(why_not(Op::kWriteBarcode, "{\"serial\":\"H221014C32213244\"}", id).empty());
  CHECK(why_not(Op::kWriteBarcode, "{\"serial\":\"short\"}", id) == refusal::kInvalidParams && why_not(Op::kWriteBarcode, "{\"serial\":\"H22101 4C32213244\"}", id) == refusal::kInvalidParams && why_not(Op::kWriteBarcode, "{}", id) == refusal::kInvalidParams);
  Identity noserial = id; noserial.serial.clear();
  CHECK(why_not(Op::kSocReset, "{}", noserial) == refusal::kSerialUnreadable);              // the erase takes the serial with it: it must be known to be written back
  CHECK(why_not(Op::kEepromErase, "{}", noserial) == refusal::kSerialUnreadable);
  CHECK(why_not(Op::kEepromErase, "{\"restore_barcode\":false}", noserial).empty());
  CHECK(why_not(Op::kPcbSetup, "{\"serial\":\"H221014C32213244\"}", noserial).empty() && why_not(Op::kPcbSetup, "{}", noserial) == refusal::kInvalidParams);

  // an unknown operation, and the read-only plan cannot be asked for
  Op op;
  CHECK(op_from_name("set_clock", op) && op == Op::kSetClock && !op_from_name("preflight", op) && !op_from_name("format", op) && !op_from_name("", op));
}

static void test_expert_line() {
  CHECK(expert_line("pwr") == "pwr" && expert_line("  bat   3 ") == "bat 3" && expert_line("ctrl cfet on") == "ctrl cfet on" && expert_line("datalist event 0") == "datalist event 0");
  for (const char* blocked : {"updata", "eepe 0", "eepw 0 1 2 3", "eepr 0 0 16", "tbar X", "tmbar", "flaw 0 1 2", "flae", "mm 1 2 3", "login x", "logout", "re 1 info", "m2s", "save", "UPDATA", "Tbar X123456789", "rtcw 1 2", "socw 1 2", "urates 9600", "setprt lg"}) CHECK(expert_line(blocked).empty());
  CHECK(expert_line("pwr tbar").empty() && expert_line("help eepe").empty());                  // not even as a later word
  for (const char* bad : {"", " ", "pwr;shut", "pwr\nshut", "pwr\r", "pwr|x", "pwr&", "$(x)", "bat `1`", "ctrl cfet on\x01", "caf\xC3\xA9"}) CHECK(expert_line(bad).empty());
  CHECK(expert_line(std::string(49, 'a')).empty() && !expert_line(std::string(48, 'a')).empty());
  bench::Battery eb;
  CHECK(why_not(Op::kExpertCommand, "{\"line\":\"tbar X123456789\"}", identity_of(eb)) == refusal::kExpertBlocked);
}

// ---- who may start what ------------------------------------------------------------------------------------------------------------------------------

static void test_preconditions() {
  bench::Battery b;
  Facts f;
  f.pylontech_port = true; f.present = true; f.current_ma = 200; f.identity = identity_of(b); f.typed_serial = b.serial;
  const json::Value none = J("{}");
  CHECK(preconditions(Op::kSocCapacity, f, none) == refusal::kNone);
  Facts g = f; g.pylontech_port = false;
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kNotPylontech);
  g = f; g.present = false;
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kNoBattery);
  g = f; g.identity.valid = false;
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kUnknownModel);
  g = f; g.identity.model = "US9999";
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kUnknownModel);
  g = f; g.identity.soft_version.clear();
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kUnknownFirmware);
  g = f; g.typed_serial = "PPTBH02200409224";
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kSerialMismatch);
  g = f; g.typed_serial = "pptbh02200409223";
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kSerialMismatch);                   // it is compared as typed
  g = f; g.typed_serial.clear();
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kSerialMismatch);
  g = f; g.current_ma = 1500;
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kNotIdle);
  g = f; g.current_ma = -1500;
  CHECK(preconditions(Op::kOutputSet, g, J("{\"output\":\"cfet\",\"state\":\"on\"}")) == refusal::kNotIdle);
  CHECK(preconditions(Op::kSetClock, g, none) == refusal::kNone);                                 // the clock does not care about the current
  g = f; g.critical_alarm = true;
  CHECK(preconditions(Op::kSocCapacity, g, none) == refusal::kCriticalAlarm);
  CHECK(preconditions(Op::kLimitsSet, g, none) == refusal::kCriticalAlarm);
  CHECK(preconditions(Op::kRestart, g, none) == refusal::kNone && preconditions(Op::kShutdown, g, none) == refusal::kNone && preconditions(Op::kFaultClear, g, none) == refusal::kNone);   // recoveries
  CHECK(preconditions(Op::kOutputSet, g, J("{\"output\":\"cfet\",\"state\":\"off\"}")) == refusal::kNone);
  CHECK(preconditions(Op::kOutputSet, g, J("{\"output\":\"cfet\",\"state\":\"on\"}")) == refusal::kCriticalAlarm);
  CHECK(preconditions(Op::kFirmwareUpdate, g, none) == refusal::kCriticalAlarm);
  // a battery with no serial number
  g = f; g.identity.serial.clear(); g.typed_serial = "NOSERIAL";
  CHECK(preconditions(Op::kSetClock, g, none) == refusal::kNone);
  g.typed_serial = "H221014C32213244";
  CHECK(preconditions(Op::kSetClock, g, none) == refusal::kSerialUnreadable);
  g.typed_serial = "H221014C32213244";
  CHECK(preconditions(Op::kWriteBarcode, g, J("{\"serial\":\"H221014C32213244\"}")) == refusal::kNone);
  CHECK(preconditions(Op::kWriteBarcode, g, J("{\"serial\":\"H221014C32213245\"}")) == refusal::kSerialMismatch);
  CHECK(preconditions(Op::kPcbSetup, g, J("{\"serial\":\"H221014C32213244\"}")) == refusal::kNone);
  CHECK(preconditions(Op::kPreflight, f, none) == refusal::kNone);
}

static void test_the_token() {
  Identity id;
  Armed out;
  const char* kTok = "tok1tok1tok1tok1tok1tok1tok1tok1";
  {
    Guard guard;
    CHECK(std::string(guard.can_arm(0, 0)) == refusal::kInvalidParams && std::string(guard.can_arm(11, 0)) == refusal::kInvalidParams && std::string(guard.can_arm(3, 0)) == refusal::kNone);
    // the right one works once, with what was read when it was armed
    guard.arm(3, "set_clock", "SERIAL00001", id, "before", kTok, 1000);
    CHECK(std::string(guard.execute(3, "set_clock", "SERIAL00001", kTok, 2000, out)) == refusal::kNone && out.token == kTok && out.before == "before");
    CHECK(std::string(guard.execute(3, "set_clock", "SERIAL00001", kTok, 2100, out)) == refusal::kNotArmed);
  }
  {
    Guard guard;   // a wrong token withdraws the arm: the right one no longer works
    guard.arm(3, "restart", "SERIAL00001", id, "", kTok, 1000);
    CHECK(std::string(guard.execute(3, "restart", "SERIAL00001", "wrong", 2000, out)) == refusal::kBadToken);
    CHECK(std::string(guard.execute(3, "restart", "SERIAL00001", kTok, 2100, out)) == refusal::kNotArmed);
  }
  {
    Guard guard;   // another operation, or another battery, is refused even with the right token
    guard.arm(3, "set_clock", "SERIAL00001", id, "", kTok, 1000);
    CHECK(std::string(guard.execute(3, "restart", "SERIAL00001", kTok, 1100, out)) == refusal::kInvalidParams);
    guard.arm(3, "restart", "SERIAL00001", id, "", kTok, 2000);
    CHECK(std::string(guard.execute(3, "restart", "OTHER000001", kTok, 2100, out)) == refusal::kInvalidParams);
  }
  {
    Guard guard;   // the arm of another port does not serve this one
    guard.arm(4, "restart", "SERIAL00001", id, "", kTok, 5000);
    CHECK(std::string(guard.execute(3, "restart", "SERIAL00001", kTok, 5100, out)) == refusal::kNotArmed);
    CHECK(std::string(guard.execute(4, "restart", "SERIAL00001", kTok, 5200, out)) == refusal::kNone);
  }
  {
    Guard guard;   // an arm expires, and a newer arm replaces the older
    guard.arm(5, "restart", "SERIAL00001", id, "", "tok5", 10'000);
    CHECK(std::string(guard.execute(5, "restart", "SERIAL00001", "tok5", 10'000 + Guard::kArmWindowMs + 1, out)) == refusal::kExpired);
    guard.arm(6, "restart", "SERIAL00001", id, "", "old", 20'000);
    guard.arm(6, "restart", "SERIAL00001", id, "", "new", 20'100);
    CHECK(std::string(guard.execute(6, "restart", "SERIAL00001", "old", 20'200, out)) == refusal::kBadToken);
    guard.arm(6, "restart", "SERIAL00001", id, "", "new", 30'000);
    CHECK(std::string(guard.execute(6, "restart", "SERIAL00001", "new", 30'100, out)) == refusal::kNone);
  }
  {
    Guard locking;   // three failures lock everything for a while
    for (int i = 0; i < 3; ++i) { locking.arm(1, "restart", "S", id, "", "t", 1000 + i); locking.execute(1, "restart", "S", "bad", 1100 + i, out); }
    CHECK(locking.locked(2000) && std::string(locking.can_arm(2, 2000)) == refusal::kLocked);
    locking.arm(1, "restart", "S", id, "", "t", 3000);
    CHECK(std::string(locking.execute(1, "restart", "S", "t", 3100, out)) == refusal::kLocked);
    CHECK(!locking.locked(1100 + Guard::kLockMs + 5000) && std::string(locking.can_arm(2, 1100 + Guard::kLockMs + 5000)) == refusal::kNone);
    Guard spread;   // failures far apart in time do not add up
    spread.arm(1, "restart", "S", id, "", "t", 0); spread.execute(1, "restart", "S", "bad", 100, out);
    spread.arm(1, "restart", "S", id, "", "t", 0); spread.execute(1, "restart", "S", "bad", 100 + Guard::kFailureWindowMs + 10, out);
    spread.arm(1, "restart", "S", id, "", "t", 0); spread.execute(1, "restart", "S", "bad", 200 + Guard::kFailureWindowMs + 10, out);
    CHECK(!spread.locked(300 + Guard::kFailureWindowMs));
  }
  {
    Guard busy;   // one operation at a time per port, and a pause after a write
    busy.begin(2);
    CHECK(std::string(busy.can_arm(2, 0)) == refusal::kBusy && std::string(busy.can_arm(3, 0)) == refusal::kNone);
    busy.arm(2, "restart", "S", id, "", "t", 0);
    CHECK(std::string(busy.execute(2, "restart", "S", "t", 10, out)) == refusal::kBusy);
    busy.finish(2, true, 100'000);
    CHECK(std::string(busy.can_arm(2, 100'500)) == refusal::kCooldown && std::string(busy.can_arm(2, 100'000 + Guard::kCooldownMs + 10)) == refusal::kNone);
    busy.begin(2);
    busy.finish(2, false, 500'000);
    CHECK(!busy.busy(2));
  }
}

// ---- the operations against the bench ---------------------------------------------------------------------------------------------------------------

static Plan plan_of(Op op, const std::string& params, const bench::Battery& b) {
  Plan plan; std::string why;
  Request r = request(op, params, identity_of(b));
  const bool ok = build_plan(r, plan, why);
  if (!ok) std::printf("no plan for %d: %s\n", static_cast<int>(op), why.c_str());
  return plan;
}

static bool wrote_line(const bench::Battery& b, const std::string& line) { for (const std::string& w : b.writes) if (w == line) return true; return false; }

static void test_reads_and_the_clock() {
  bench::Battery b;
  RunResult r;
  Plan pre = preflight_plan();
  run_plan(pre, b, r);
  CHECK(r.outcome == "ok" && r.identity.valid && r.identity.model == "US3000C" && r.identity.serial == b.serial && r.identity.soft_version == "V3.0");
  CHECK(r.writes_done == 0 && !r.wrote && b.writes.empty());                                     // reading writes nothing
  CHECK(r.before.find("Total Coul") != std::string::npos && r.before.find("## info") != std::string::npos);
  CHECK(b.debug_mode);
  // the clock, read back
  Plan clock = plan_of(Op::kSetClock, "{}", b);
  run_plan(clock, b, r);
  CHECK(r.outcome == "ok" && r.writes_done == 1 && wrote_line(b, "time 26 10 19 14 05 09") && b.clock == "26-10-19 14:05:09");
  CHECK(r.after.find("26-10-19 14:05:09") != std::string::npos && r.before.find("25-03-09") != std::string::npos);   // the before and the after both kept
  // a console that takes the line and keeps another value: failed, with the real state
  bench::Battery odd;
  odd.wrong_clock_format = true;
  Plan clock2 = plan_of(Op::kSetClock, "{}", odd);
  run_plan(clock2, odd, r);
  CHECK(r.outcome == "failed" && r.code.rfind("verify:clock_differs", 0) == 0 && r.writes_done == 1);
}

static void test_a_battery_that_does_not_answer() {
  bench::Battery silent;
  silent.silent = true;
  RunResult r;
  Plan pre = preflight_plan();
  run_plan(pre, silent, r);
  CHECK(r.outcome == "retry" && r.code == "timeout" && !r.wrote);                                // nothing was written: asking again is safe
  Plan clock = plan_of(Op::kSetClock, "{}", silent);
  run_plan(clock, silent, r);
  CHECK(r.outcome == "retry");
  // a wrong service password stops at the log-in with nothing written
  bench::Battery b;
  run_plan(clock, b, r, nullptr, nullptr, "another-dummy-pw");
  CHECK(r.outcome == "failed" && r.code == "step_failed:login" && r.writes_done == 0 && b.writes.empty());
  // no password set
  Runner none(clock, "");
  none.start(0);
  CHECK(none.finished() && none.result().outcome == "rejected" && none.result().code == refusal::kNoPassword);
}

static void test_the_password_is_never_shown() {
  bench::Battery b;
  Plan clock = plan_of(Op::kSetClock, "{}", b);
  Runner runner(clock, bench::kDummyPassword);
  bench::drive(runner, b);
  const RunResult& r = runner.result();
  CHECK(r.transcript.find("login ***") != std::string::npos);
  CHECK(r.transcript.find(bench::kDummyPassword) == std::string::npos);
  for (const std::string& line : b.received_lines) CHECK(line.find(bench::kDummyPassword) == std::string::npos);   // the bench masks it; the point is the battery really got it:
  CHECK(b.debug_mode);
  // redact() works on its own, and on a login line whatever the argument
  CHECK(redact("a dummy-service-pw b", "dummy-service-pw") == "a *** b");
  CHECK(redact("login hunter2\r\nmore", "") == "login ***\r\nmore");
  CHECK(redact("nothing", "pw") == "nothing" && redact("login ***", "") == "login ***");
  for (const Step& s : clock.steps) CHECK(s.send.find(bench::kDummyPassword) == std::string::npos);
  // a plan never carries the password: only its placeholder
  bool placeholder = false;
  for (const Step& s : clock.steps) if (s.send == "login {pw}") placeholder = true;
  CHECK(placeholder);
}

static void test_soc_operations() {
  bench::Battery b;
  RunResult r;
  b.total_coul_mah = 12000;
  run_plan(plan_of(Op::kSocCapacity, "{}", b), b, r);
  CHECK(r.outcome == "ok" && wrote_line(b, "totalcoul 74") && b.total_coul_mah == 74000);
  CHECK(r.before.find("12000") != std::string::npos && r.after.find("74000") != std::string::npos);
  // an older firmware without the command: nothing changes, and the result says why
  bench::Battery old;
  old.refuse_totalcoul = true;
  run_plan(plan_of(Op::kSocCapacity, "{}", old), old, r);
  CHECK(r.outcome == "failed" && r.code == "step_failed:write" && old.writes.empty());
  // the MOSFET test
  bench::Battery m;
  run_plan(plan_of(Op::kMosfailTest, "{}", m), m, r);
  CHECK(r.outcome == "ok" && m.outputs["mostest"] == "ON");
  // the maker's procedure after a board replacement
  bench::Battery pcb;
  pcb.serial.clear();
  run_plan(plan_of(Op::kPcbSetup, "{\"serial\":\"H221014C32213244\"}", pcb), pcb, r);
  CHECK(r.outcome == "ok" && pcb.serial == "H221014C32213244" && pcb.total_coul_mah == 74000 && pcb.outputs["mostest"] == "ON" && pcb.clock == "26-10-19 14:05:09");
  CHECK(wrote_line(pcb, "totalcoul 74") && wrote_line(pcb, "ctrl mostest on") && wrote_line(pcb, "tbar H221014C32213244"));
  // the reset of the learned state: erase, restart, log in again, write the capacity and the serial number back
  bench::Battery rs;
  rs.total_coul_mah = 33000;
  run_plan(plan_of(Op::kSocReset, "{}", rs), rs, r);
  CHECK(r.outcome == "ok" && rs.resets == 1 && rs.serial == "PPTBH02200409223" && rs.total_coul_mah == 74000);
  CHECK(wrote_line(rs, "eepe 0") && wrote_line(rs, "trst") && wrote_line(rs, "tbar PPTBH02200409223") && wrote_line(rs, "totalcoul 74"));
  CHECK(r.writes_done == 5 && r.steps_done == r.steps_total);
  // the order matters: erase, restart, then the writes that need the administrator mode again
  std::size_t eepe = 0, trst = 0, tbar = 0;
  for (std::size_t i = 0; i < rs.writes.size(); ++i) { if (rs.writes[i] == "eepe 0") eepe = i; if (rs.writes[i] == "trst") trst = i; if (rs.writes[i].rfind("tbar", 0) == 0) tbar = i; }
  CHECK(eepe < trst && trst < tbar);
  // the serial number cannot be written back: the sequence stops midway and says so
  bench::Battery half;
  half.fail_tbar = true;
  run_plan(plan_of(Op::kSocReset, "{}", half), half, r);
  CHECK(r.outcome == "partial" && r.code == "step_failed:barcode" && half.serial.empty() && r.writes_done >= 4);
  // an atomic sequence cannot be cancelled once it has begun to write
  Plan plan = plan_of(Op::kSocReset, "{}", rs);
  Runner late(plan, bench::kDummyPassword);
  late.start(0);
  CHECK(late.can_cancel());                                                                      // not begun
}

static void test_cancel_rules() {
  bench::Battery b;
  Plan clear = plan_of(Op::kClearStats, "{}", b);
  Runner runner(clear, bench::kDummyPassword);
  runner.start(0);
  runner.cancel();                                                                               // before anything: honoured at once
  runner.next_tx(1);
  CHECK(runner.finished() && runner.result().code == "cancelled" && runner.result().outcome == "failed" && !runner.result().wrote);
  Plan reset = plan_of(Op::kEepromErase, "{}", b);
  Runner r2(reset, bench::kDummyPassword);
  r2.start(0);
  std::uint64_t now = 0;
  while (!r2.finished() && !r2.committed() && now < 60'000) {
    const std::vector<std::uint8_t> tx = r2.next_tx(now);
    if (!tx.empty()) b.feed(tx);
    const std::vector<std::uint8_t> rx = b.take();
    if (!rx.empty()) r2.on_rx(rx.data(), rx.size(), now);
    now += 20;
  }
  CHECK(r2.committed() && !r2.can_cancel());
  r2.cancel();
  while (!r2.finished() && now < 120'000) {
    const std::vector<std::uint8_t> tx = r2.next_tx(now);
    if (!tx.empty()) b.feed(tx);
    const std::vector<std::uint8_t> rx = b.take();
    if (!rx.empty()) r2.on_rx(rx.data(), rx.size(), now);
    now += 20;
  }
  CHECK(r2.result().outcome == "ok" && r2.result().code.empty());                                // the cancel was ignored: the sequence ran to its end
}

static void test_the_rest_of_the_writes() {
  RunResult r;
  bench::Battery b;
  run_plan(plan_of(Op::kClearStats, "{}", b), b, r);
  CHECK(r.outcome == "ok" && wrote_line(b, "statc") && b.stat_cycles == 0);
  run_plan(plan_of(Op::kClearLog, "{}", b), b, r);
  CHECK(r.outcome == "ok" && wrote_line(b, "logc"));
  run_plan(plan_of(Op::kClearData, "{\"which\":\"history\"}", b), b, r);
  CHECK(r.outcome == "ok" && wrote_line(b, "datac history"));
  // restart: the prompt comes back to the user mode and the run ends there
  bench::Battery rb;
  run_plan(plan_of(Op::kRestart, "{}", rb), rb, r);
  CHECK(r.outcome == "ok" && rb.resets == 1 && !rb.debug_mode);
  // shutdown: the battery goes silent, which is the expected answer
  bench::Battery sd;
  run_plan(plan_of(Op::kShutdown, "{}", sd), sd, r);
  CHECK(r.outcome == "ok" && sd.off && wrote_line(sd, "shut"));
  // outputs, read back
  bench::Battery o;
  run_plan(plan_of(Op::kOutputSet, "{\"output\":\"cfet\",\"state\":\"off\"}", o), o, r);
  CHECK(r.outcome == "ok" && o.outputs["cfet"] == "OFF" && wrote_line(o, "ctrl cfet off"));
  run_plan(plan_of(Op::kOutputSet, "{\"output\":\"buzz\",\"state\":\"on\"}", o), o, r);
  CHECK(r.outcome == "ok" && o.outputs["buzz"] == "ON");
  run_plan(plan_of(Op::kOutputSet, "{\"output\":\"heat\",\"state\":\"on\"}", o), o, r);
  CHECK(r.outcome == "ok" && o.outputs["heat"] == "ON");
  run_plan(plan_of(Op::kFaultClear, "{}", o), o, r);
  CHECK(r.outcome == "ok" && wrote_line(o, "ctrl afuse off"));
  // limits: every line, then the save, then the read-back that shows the values
  bench::Battery l;
  const std::string good_temp = "{\"group\":\"temperature\",\"values\":{\"cot\":60000,\"cotr\":55000,\"cht\":50000,\"chtr\":45000,\"clt\":5000,\"cltr\":8000,\"cut\":-5000,\"cutr\":-2000}}";
  run_plan(plan_of(Op::kLimitsSet, good_temp, l), l, r);
  CHECK(r.outcome == "ok" && l.config.size() == 8 && l.config["bot"] == "60000" && l.config["but"] == "-5000" && wrote_line(l, "config save"));
  run_plan(plan_of(Op::kLimitsDefault, "{}", l), l, r);
  CHECK(r.outcome == "ok" && l.config.empty());
  // protections
  bench::Battery p;
  run_plan(plan_of(Op::kProtectionsSet, "{\"items\":{\"bov\":\"en\",\"doc\":\"dis\"},\"accept_risk\":true}", p), p, r);
  CHECK(r.outcome == "ok" && p.prot["bov"] == "en" && p.prot["doc"] == "dis");
  run_plan(plan_of(Op::kProtectionsDefault, "{}", p), p, r);
  CHECK(r.outcome == "ok" && p.prot.empty());
  // the serial number
  bench::Battery s;
  s.serial.clear();
  run_plan(plan_of(Op::kWriteBarcode, "{\"serial\":\"H221014C32213244\"}", s), s, r);
  CHECK(r.outcome == "ok" && s.serial == "H221014C32213244");
  bench::Battery sf;
  sf.fail_tbar = true;
  run_plan(plan_of(Op::kWriteBarcode, "{\"serial\":\"H221014C32213244\"}", sf), sf, r);
  CHECK(r.outcome == "failed" && r.code == "step_failed:write" && sf.serial == "PPTBH02200409223");
  // erase with the serial written back, and without
  bench::Battery e;
  run_plan(plan_of(Op::kEepromErase, "{}", e), e, r);
  CHECK(r.outcome == "ok" && e.serial == "PPTBH02200409223" && e.resets == 1);
  bench::Battery e2;
  run_plan(plan_of(Op::kEepromErase, "{\"restore_barcode\":false}", e2), e2, r);
  CHECK(r.outcome == "ok" && e2.serial.empty());
  // the expert console: what is sent is the checked line; a reply the battery refuses is shown, not hidden
  bench::Battery x;
  run_plan(plan_of(Op::kExpertCommand, "{\"line\":\"ctrl dfet off\"}", x), x, r);
  CHECK(r.outcome == "ok" && x.outputs["dfet"] == "OFF");
  run_plan(plan_of(Op::kExpertCommand, "{\"line\":\"nonsense 1\"}", x), x, r);
  CHECK(r.outcome == "ok" && r.transcript.find("Invalid command") != std::string::npos);
}

// ---- the firmware ------------------------------------------------------------------------------------------------------------------------------------

static Plan firmware_plan(const bench::Battery& b, const std::vector<std::uint8_t>& image, bool clear_eeprom = false) {
  const std::string params = "{\"image_size\":" + std::to_string(image.size()) + ",\"image_crc32\":" + std::to_string(crc32(image.data(), image.size())) + ",\"image_version\":\"2.4\"" + (clear_eeprom ? ",\"clear_eeprom\":true" : "") + "}";
  return plan_of(Op::kFirmwareUpdate, params, b);
}

static void test_firmware() {
  RunResult r;
  const std::vector<std::uint8_t> image = bench::make_image("US2000C", 2, 4);
  bench::Battery b;                                                                               // an older-chip US3000C
  Plan plan = firmware_plan(b, image);
  CHECK(!plan.steps.empty() && plan.uses_image && plan.atomic && plan.image_size == image.size());
  CHECK(check_firmware_request(plan, image, identity_of(b)).empty() || check_firmware_request(plan, image, identity_of(b)) == refusal::kNone);
  run_plan(plan, b, r, &image);
  CHECK(r.outcome == "ok");
  CHECK(r.packets_total == xmodem::packet_count(image.size()) && r.packets_sent == r.packets_total);
  CHECK(b.packets_accepted == static_cast<int>(r.packets_total) && b.received_image.size() >= image.size());
  CHECK(std::equal(image.begin(), image.end(), b.received_image.begin()));                       // what arrived is what was sent
  for (std::size_t i = image.size(); i < b.received_image.size(); ++i) CHECK(b.received_image[i] == xmodem::kPad);
  CHECK(b.soft == "V2.4" && b.serial == "PPTBH02200409223");                                      // the serial number was written back after the update
  CHECK(wrote_line(b, "updata") && wrote_line(b, "tbar PPTBH02200409223") && !wrote_line(b, "eepe 0"));
  // with the erase of the external memory afterwards
  bench::Battery b2;
  run_plan(firmware_plan(b2, image, true), b2, r, &image);
  CHECK(r.outcome == "ok" && wrote_line(b2, "eepe 0") && b2.serial == "PPTBH02200409223");
  // the receiver asks for some packets again
  bench::Battery nak;
  nak.nak_first_packets = 3;
  run_plan(plan, nak, r, &image);
  CHECK(r.outcome == "ok" && nak.packets_rejected == 3);
  // too many NAKs: it stops, and says the sequence is partial (the battery stays in its bootloader)
  bench::Battery stubborn;
  stubborn.nak_first_packets = 100;
  run_plan(plan, stubborn, r, &image);
  CHECK(r.outcome == "partial" && r.code == "firmware:resends_used_up" && stubborn.soft == "V3.0");
  // the bootloader cancels
  bench::Battery cancels;
  cancels.cancel_at_packet = 5;
  run_plan(plan, cancels, r, &image);
  CHECK(r.outcome == "partial" && r.code == "firmware:receiver_cancelled");
  // it never asks for the transfer: nothing was sent, and still it is partial because the battery went into its bootloader
  bench::Battery quiet;
  quiet.never_ask_crc = true;
  run_plan(plan, quiet, r, &image);
  CHECK(r.outcome == "partial" && r.code == "firmware:no_crc_request" && quiet.packets_accepted == 0);
  bench::Battery sum;
  sum.ask_checksum_mode = true;
  run_plan(plan, sum, r, &image);
  CHECK(r.outcome == "partial" && r.code == "firmware:checksum_mode_requested" && sum.packets_accepted == 0);
  // the battery reports an error at the end
  bench::Battery bad;
  bad.bad_end = true;
  run_plan(plan, bad, r, &image);
  CHECK(r.outcome == "partial" && r.code == "firmware:battery_reported_error");
  // an atomic sequence: from the first line on, no cancel
  Runner runner(plan, bench::kDummyPassword, &image);
  bench::Battery c;
  bench::drive(runner, c);
  CHECK(runner.committed() && !runner.can_cancel());

  // the gate: nothing is sent when the image does not fit the battery or is not the one the plan was made for
  const Identity id = identity_of(b);
  CHECK(check_firmware_request(plan, image, id) == refusal::kNone);
  std::vector<std::uint8_t> other = image; other[5000] ^= 1;
  CHECK(check_firmware_request(plan, other, id) == refusal::kImageMissing);
  std::vector<std::uint8_t> shorter(image.begin(), image.end() - 10);
  CHECK(check_firmware_request(plan, shorter, id) == refusal::kImageMissing);
  const std::vector<std::uint8_t> newer_image = bench::make_image("US2000C_NF4", 2, 4);
  Plan wrong = firmware_plan(b, newer_image);
  CHECK(check_firmware_request(wrong, newer_image, id) == refusal::kImageMismatch);              // the newer chip's image for an older chip
  Runner refused(wrong, bench::kDummyPassword, nullptr);
  refused.start(0);
  CHECK(refused.finished() && refused.result().outcome == "rejected" && refused.result().code == refusal::kImageMissing);   // no image staged: nothing is sent
  Identity unknown = id; unknown.board = "NF4.E2";
  CHECK(why_not(Op::kFirmwareUpdate, "{\"image_size\":200000,\"image_crc32\":1,\"image_version\":\"2.4\"}", unknown) == refusal::kChipUnknown);
  Identity noserial = id; noserial.serial.clear();
  CHECK(why_not(Op::kFirmwareUpdate, "{\"image_size\":200000,\"image_crc32\":1,\"image_version\":\"2.4\"}", noserial) == refusal::kSerialUnreadable);
  Identity us2kbpl = id; us2kbpl.model = "US2KBPL";
  CHECK(why_not(Op::kFirmwareUpdate, "{\"image_size\":200000,\"image_crc32\":1,\"image_version\":\"2.4\"}", us2kbpl) == refusal::kUnsupportedModel);
  CHECK(why_not(Op::kFirmwareUpdate, "{\"image_size\":10,\"image_crc32\":1,\"image_version\":\"2.4\"}", id) == refusal::kInvalidParams);
  CHECK(why_not(Op::kFirmwareUpdate, "{\"image_size\":200000,\"image_version\":\"2.4\"}", id) == refusal::kInvalidParams);
  // a block number that wraps: an image of more than 255 packets goes through
  const std::vector<std::uint8_t> big = bench::make_image("US2000C", 2, 4, 300 * 1024);
  bench::Battery bigbat;
  run_plan(firmware_plan(bigbat, big), bigbat, r, &big);
  CHECK(r.outcome == "ok" && bigbat.packets_accepted == 300);
}

static void test_the_catalog() {
  for (const OpInfo& i : catalog()) {
    CHECK(std::string(i.name).size() > 3);
    if (i.risk == Risk::kIrreversible && i.op != Op::kPreflight) CHECK(i.atomic || i.op == Op::kPcbSetup);   // what cannot be undone is never cut in half
    if (i.atomic) CHECK(i.writes && i.needs_idle);
  }
  CHECK(op_info(Op::kFirmwareUpdate)->atomic && op_info(Op::kEepromErase)->atomic && op_info(Op::kWriteBarcode)->atomic && op_info(Op::kSocReset)->atomic);
  CHECK(!op_info(Op::kPreflight)->writes);
  CHECK(std::string(risk_name(Risk::kIrreversible)) == "irreversible");
  CHECK(!models().empty() && model_profile("US3000C")->official_ah == 74 && model_profile("US2000C")->official_ah == 50 && model_profile("UP5000")->official_ah == 100 && model_profile("nope") == nullptr);
  // every group of limits names each key once and its orders only name its keys
  for (const LimitGroup& g : limit_groups()) {
    for (const Order& o : g.orders) {
      bool low = false, high = false;
      for (const Limit& k : g.keys) { if (std::string(k.key) == o.low) low = true; if (std::string(k.key) == o.high) high = true; }
      CHECK(low && high);
    }
  }
  // a plan for every operation can be built for a good battery, and no plan carries a line the expert console would refuse to take as a block-listed one except the ones made for them
  bench::Battery b;
  const std::string image_params = "{\"image_size\":200000,\"image_crc32\":1,\"image_version\":\"2.4\"}";
  struct Case { Op op; const char* params; };
  const Case cases[] = {{Op::kSetClock, "{}"}, {Op::kClearStats, "{}"}, {Op::kClearLog, "{}"}, {Op::kClearData, "{\"which\":\"event\"}"}, {Op::kRestart, "{}"}, {Op::kShutdown, "{}"}, {Op::kSocCapacity, "{}"}, {Op::kMosfailTest, "{}"},
                        {Op::kSocReset, "{}"}, {Op::kPcbSetup, "{\"serial\":\"H221014C32213244\"}"}, {Op::kLimitsDefault, "{}"}, {Op::kProtectionsDefault, "{}"}, {Op::kOutputSet, "{\"output\":\"cfet\",\"state\":\"on\"}"},
                        {Op::kFaultClear, "{}"}, {Op::kWriteBarcode, "{\"serial\":\"H221014C32213244\"}"}, {Op::kEepromErase, "{}"}, {Op::kExpertCommand, "{\"line\":\"pwr\"}"}};
  for (const Case& c : cases) {
    Plan p; std::string why;
    CHECK(build_plan(request(c.op, c.params, identity_of(b)), p, why) && !p.canonical.empty() && !p.summary.empty() && !p.steps.empty());
    for (const Step& s : p.steps) CHECK(s.send.find('\r') == std::string::npos && s.send.find('\n') == std::string::npos && s.send.find(';') == std::string::npos);
  }
  (void)image_params;
  Plan p; std::string why;
  CHECK(!build_plan(request(Op::kPreflight, "{}", identity_of(b)), p, why) && why == refusal::kUnknownOp);
}

#include "battery_more_tests.inc"
#include "battery_catalog_test.inc"

int main() {
  test_xmodem();
  test_identity_and_chip();
  test_image_checks();
  test_values_are_bounded();
  test_expert_line();
  test_preconditions();
  test_the_token();
  test_reads_and_the_clock();
  test_a_battery_that_does_not_answer();
  test_the_password_is_never_shown();
  test_soc_operations();
  test_cancel_rules();
  test_the_rest_of_the_writes();
  test_firmware();
  test_the_catalog();
  test_the_shared_table();
  test_the_staged_image();
  test_the_setting();
  test_the_read_only_console_stays_read_only();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
