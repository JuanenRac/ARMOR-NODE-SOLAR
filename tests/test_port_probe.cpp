// ARMOR-NODE-SOLAR - host tests of the probe of a port (core/port_probe.hpp): it can only send reading questions, it tells an echo from an answer and from silence, it keeps to the
// speeds a port can do, it can be stopped, it ends in time, and it writes down every step. The line and the clock are stand-ins; no equipment has been probed.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/mux_group.hpp"
#include "../core/port_probe.hpp"

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
using namespace armor::solar::probe;

static Bytes bytes(const std::string& text) { return Bytes(text.begin(), text.end()); }
static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
static Bytes with_crc_cr(const std::string& text) {
  Bytes out = bytes(text);
  const auto crc = voltronic::wire_crc(out.data(), out.size());
  out.push_back(crc[0]);
  out.push_back(crc[1]);
  out.push_back('\r');
  return out;
}
static Bytes modbus(std::initializer_list<int> head) {
  Bytes out;
  for (const int b : head) out.push_back(static_cast<std::uint8_t>(b));
  const std::uint16_t crc = drivers::crc16_modbus(out.data(), out.size());
  out.push_back(static_cast<std::uint8_t>(crc));
  out.push_back(static_cast<std::uint8_t>(crc >> 8));
  return out;
}

static std::uint64_t g_now = 0;

// A line that records every write and answers as told; time passes only in it.
struct FakeLine {
  enum class Mode { kSilent, kEcho, kAnswerPlainQpi, kEchoThenAnswer, kNoise, kStream };
  Mode mode = Mode::kSilent;
  int baud = 0;
  bool inverted = false;
  Format format = Format::k8N1;
  std::vector<Bytes> writes;
  std::vector<int> bauds;
  Bytes pending;
  bool any_inverted = false, any_other_format = false;

  bool configure(int b, bool inv, Format f) {
    baud = b; inverted = inv; format = f;
    bauds.push_back(b);
    any_inverted = any_inverted || inv;
    any_other_format = any_other_format || f != Format::k8N1;
    return true;
  }
  void discard() { pending.clear(); }
  bool write(const std::uint8_t* data, std::size_t length) {
    writes.emplace_back(data, data + length);
    g_now += length * 10'000 / static_cast<std::uint64_t>(baud) + 1;
    const Bytes sent = writes.back();
    const bool right = baud == 2400 && !inverted && format == Format::k8N1 && sent == bytes("QPI\r");
    const Bytes answer = bytes("(PI30\r");
    if (mode == Mode::kEcho) pending = sent;
    if (mode == Mode::kAnswerPlainQpi && right) pending = answer;
    if (mode == Mode::kEchoThenAnswer && right) { pending = sent; pending.insert(pending.end(), answer.begin(), answer.end()); }
    if (mode == Mode::kNoise && baud == 9600) pending = {0x00, 0xF8, 0x80};
    return true;
  }
  std::size_t read(std::uint8_t* out, std::size_t capacity, unsigned wait_ms) {
    if (mode == Mode::kStream) { g_now += 5; std::fill(out, out + capacity, 0x55); return capacity; }
    if (pending.empty()) { g_now += wait_ms; return 0; }
    g_now += 5;
    const std::size_t n = std::min(capacity, pending.size());
    std::copy(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(n), out);
    pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(n));
    return n;
  }
  std::uint32_t framing_errors() { return 0; }
};

static Clock fake_clock() {
  Clock c;
  c.uptime_ms = [] { return g_now; };
  c.stamp = [] { return "+" + std::to_string(g_now) + "ms"; };
  return c;
}

static std::string run(Probe& p, FakeLine& line) {
  std::string record = p.header();
  for (int guard = 0; !p.finished() && guard < 5000; ++guard) record += p.step(line);
  record += p.summary();
  return record;
}

static void test_only_reading_questions() {
  CHECK(kQueries.size() == 37);
  for (const Query& q : kQueries) {
    const Bytes frame = build(q);
    CHECK(!frame.empty());
    CHECK(read_only(frame));
    CHECK(frame.size() <= 64);   // what an emulated port can send in one piece
    if (!read_only(frame)) std::printf("  refused: %s\n", q.name);
  }
  // setting and control orders of every family are refused
  for (const char* text : {"POP02", "PCP00", "PBT00", "MUCHGC030", "PSDV48.0", "PF", "PE", "QPOP02", "QPI9"}) {
    CHECK(!read_only(with_crc_cr(text)));
    CHECK(!read_only(bytes(std::string(text) + "\r")));
  }
  CHECK(!read_only(with_crc_cr("^S007POP0,1")));
  CHECK(!read_only(bytes("^S007POP\r")));
  CHECK(!read_only(modbus({1, 0x06, 0, 0, 0, 1})));       // write one register
  CHECK(!read_only(modbus({1, 0x05, 0, 0, 0xFF, 0})));    // write a coil
  CHECK(!read_only(modbus({1, 0x10, 0, 0, 0, 1})));       // write registers
  CHECK(!read_only(modbus({1, 0x0F, 0, 0, 0, 1})));
  CHECK(!read_only(modbus({1, 0x16, 0, 0, 0, 1})));
  CHECK(!read_only(modbus({0, 0x03, 0, 0, 0, 1})));       // broadcast
  CHECK(!read_only(modbus({11, 0x03, 0, 0, 0, 1})));
  Bytes daly = drivers::DalyDriver::request(0xD9);        // switches the discharge MOSFET
  CHECK(!read_only(daly));
  CHECK(!read_only(drivers::DalyDriver::request(0x00)));
  CHECK(!read_only(Bytes{0xDD, 0x5A, 0xE1, 0x02, 0x00, 0x02, 0xFF, 0x1B, 0x77}));   // JBD: write the MOSFETs
  CHECK(!read_only(Bytes{0xDD, 0x5A, 0x03, 0x00, 0xFF, 0xFD, 0x77}));
  Bytes jk = drivers::JkDriver::request();
  jk[8] = 0x02;                                           // JK: write
  CHECK(!read_only(jk));
  Bytes ant_write = ant::request(ant::Protocol::kNew);
  ant_write[2] = 0x51;
  CHECK(!read_only(ant_write));
  CHECK(!read_only(bytes(":8EDED0000\n")));               // VE.Direct set
  CHECK(!read_only(bytes(":6B5\n")));                     // VE.Direct restart
  for (const char* c : {"trst\r", "login debug\r", "setbaud\r", "pwr 1\r", "ctrl\r", "shut\r"}) CHECK(!read_only(bytes(c)));
  CHECK(!read_only(Bytes{}));
}

static void test_a_silent_port_is_swept_whole_and_in_time() {
  g_now = 0;
  Setup setup;
  setup.port = 5;
  Probe p(setup, fake_clock());
  FakeLine line;
  const std::string record = run(p, line);
  CHECK(p.progress().state == State::kDone);            // inside the time limit
  CHECK(p.progress().step == p.progress().total);
  CHECK(g_now < Probe::kActiveLimitMs);
  CHECK(std::string(p.progress().verdict()) == "silent");
  CHECK(p.progress().combinations == 32);               // 8 speeds x 2 polarities in 8N1, then 8 speeds in 8E1 and 8N2
  CHECK(has(record, "NOT ONE BYTE in 32 combinations"));
  CHECK(has(record, "received: nothing"));
  CHECK(has(record, "#" + std::to_string(p.progress().total) + " "));
  CHECK(line.any_inverted && line.any_other_format);
  CHECK(!line.writes.empty() && line.writes.size() == p.progress().sent);
  // every byte that went out is one of the reading questions
  std::vector<Bytes> catalogue;
  for (const Query& q : kQueries) catalogue.push_back(build(q));
  for (const Bytes& w : line.writes) {
    CHECK(read_only(w));
    CHECK(std::find(catalogue.begin(), catalogue.end(), w) != catalogue.end());
  }
  Log log;
  log.append(record);
  CHECK(!log.truncated());                              // the whole record of a silent port is kept
}

static void test_echo_answer_and_noise_are_told_apart() {
  {
    g_now = 0;
    Probe p(Setup{}, fake_clock());
    FakeLine line;
    line.mode = FakeLine::Mode::kEcho;
    const std::string record = run(p, line);
    CHECK(std::string(p.progress().verdict()) == "echo");
    CHECK(p.progress().echoes == p.progress().sent && p.progress().answers == 0 && p.progress().unknown == 0);
    CHECK(has(record, "ECHO of what was sent"));
    CHECK(has(record, "only an ECHO"));
  }
  for (const auto mode : {FakeLine::Mode::kAnswerPlainQpi, FakeLine::Mode::kEchoThenAnswer}) {
    g_now = 0;
    Probe p(Setup{}, fake_clock());
    FakeLine line;
    line.mode = mode;
    const std::string record = run(p, line);
    CHECK(std::string(p.progress().verdict()) == "answered");
    CHECK(p.progress().answers == 1 && p.progress().found.size() == 1);
    if (!p.progress().found.empty()) {
      const Found& f = p.progress().found.front();
      CHECK(f.baud == 2400 && f.format == Format::k8N1 && !f.inverted);
      CHECK(std::string(f.query) == "Voltronic QPI without CRC (PI00/PI16)");
      CHECK(std::string(f.protocol) == "Voltronic");
    }
    CHECK(has(record, "ANSWERED Voltronic QPI without CRC (PI00/PI16) at 2400 8N1 normal"));
    CHECK(has(record, "|(PI30.|") || mode == FakeLine::Mode::kEchoThenAnswer);
    CHECK((p.progress().echoes == 1) == (mode == FakeLine::Mode::kEchoThenAnswer));
  }
  {
    g_now = 0;
    Probe p(Setup{}, fake_clock());
    FakeLine line;
    line.mode = FakeLine::Mode::kNoise;
    const std::string record = run(p, line);
    CHECK(std::string(p.progress().verdict()) == "bytes");
    CHECK(p.progress().rx_bytes > 0 && !p.progress().noise.empty() && p.progress().noise.front().baud == 9600);
    CHECK(has(record, "00 F8 80"));
  }
}

static void test_what_came_is_recognised() {
  const auto name = [](const Bytes& b) { return std::string(recognise(b.data(), b.size())); };
  CHECK(name(with_crc_cr("(PI30")) == "Voltronic");
  CHECK(name(modbus({5, 0x03, 2, 0x12, 0x34})) == "Modbus");
  CHECK(name(modbus({1, 0x84, 2})) == "Modbus");
  CHECK(name(bytes("PID\t0xA05F\r\nV\t13200\r\n")) == "VE.Direct text");
  CHECK(name(bytes(":78DED00D204FF\n")) == "VE.Direct HEX");
  CHECK(name(bytes("pwr\r\n@\r\npylon>")) == "Pylontech console");
  CHECK(name(Bytes{0x00, 0xFF, 0x80, 0x12, 0x00}) == "");
  CHECK(judge(bytes("QPI\r"), bytes("QPI\rQPI\r")).result == Result::kEcho);
  CHECK(judge(bytes("QPIGS\r"), bytes("QPIG")).result == Result::kEcho);
  CHECK(judge(bytes("QPI\r"), Bytes{}).result == Result::kNothing);
}

static void test_the_speeds_of_each_port() {
  {   // an emulated port: only 8N1, normal polarity, 19200 at most, and the record says what was skipped and why
    g_now = 0;
    Setup setup;
    setup.port = 9;
    setup.emulated = true;
    setup.max_baud = 19200;
    setup.inverter = true;
    setup.fast = true;   // asking for more does not make the port faster
    Probe p(setup, fake_clock());
    FakeLine line;
    const std::string record = run(p, line);
    CHECK(p.progress().state == State::kDone);
    CHECK(*std::max_element(line.bauds.begin(), line.bauds.end()) == 19200);
    CHECK(!line.any_inverted && !line.any_other_format);
    CHECK(p.progress().combinations == 5);
    CHECK(has(record, "speeds tried: 1200, 2400, 4800, 9600, 19200\n"));
    CHECK(has(record, "speeds skipped: 38400, 57600, 115200 (this port reaches 19200 baud at most)"));
  }
  {   // an inverter on a UART: 19200 at most unless the faster ones are asked for
    g_now = 0;
    Setup setup;
    setup.inverter = true;
    Probe p(setup, fake_clock());
    FakeLine line;
    const std::string record = run(p, line);
    CHECK(*std::max_element(line.bauds.begin(), line.bauds.end()) == 19200);
    CHECK(has(record, "speeds skipped: 38400, 57600, 115200 (an inverter port, and the faster speeds were not asked for)"));
    setup.fast = true;
    Probe fast(setup, fake_clock());
    FakeLine second;
    run(fast, second);
    CHECK(*std::max_element(second.bauds.begin(), second.bauds.end()) == 115200);
  }
  {   // the port's own speed and polarity go first
    g_now = 0;
    Setup setup;
    setup.baud = 9600;
    setup.invert = true;
    Probe p(setup, fake_clock());
    FakeLine line;
    p.step(line);
    CHECK(line.bauds.front() == 9600 && line.inverted);
  }
  {   // no TX pin: only listening
    g_now = 0;
    Setup setup;
    setup.can_transmit = false;
    Probe p(setup, fake_clock());
    FakeLine line;
    run(p, line);
    CHECK(p.progress().state == State::kDone && line.writes.empty() && p.progress().sent == 0);
  }
}

static void test_stop_and_time_limit() {
  {
    g_now = 0;
    Probe p(Setup{}, fake_clock());
    FakeLine line;
    std::atomic<bool> cancel{false};
    for (int i = 0; i < 30; ++i) p.step(line, &cancel);
    const std::size_t sent = line.writes.size();
    cancel = true;
    p.step(line, &cancel);
    p.step(line, &cancel);
    CHECK(p.progress().state == State::kStopped && p.progress().step == 30 && line.writes.size() == sent);
    CHECK(has(p.summary(), "stopped by hand: 30 of "));
  }
  {   // a line that never stops talking: every step is cut, and the whole probe ends at its limit
    g_now = 0;
    Probe p(Setup{}, fake_clock());
    FakeLine line;
    line.mode = FakeLine::Mode::kStream;
    Log log;
    for (int guard = 0; !p.finished() && guard < 5000; ++guard) log.append(p.step(line));
    log.append(p.summary(), true);
    CHECK(p.progress().state == State::kTimeLimit);
    CHECK(g_now <= Probe::kActiveLimitMs + 2 * Probe::kStepLimitMs);
    CHECK(log.truncated());                          // too much to keep it all: the summary is still there
    CHECK(has(log.text(), "SUMMARY"));
    CHECK(log.size() < 400 * 1024);
  }
}

static void test_the_record_is_read_in_pieces() {
  Log log(10'000);
  std::string all;
  for (int i = 0; i < 300; ++i) {
    const std::string line = "line " + std::to_string(i) + " of the record\n";
    log.append(line);
    all += line;
  }
  CHECK(all.size() > Log::kPiece && log.text() == all.substr(0, log.size()));
  std::string again;
  char piece[1000];
  for (std::size_t n; (n = log.read(again.size(), piece, sizeof piece)) > 0;) again.append(piece, n);
  CHECK(again == log.text());
  CHECK(log.read(log.size(), piece, sizeof piece) == 0);
}

// A port that is being probed gets no turns; the others of its multiplexed group go on.
struct IdleLine : MuxLine {
  std::vector<std::size_t> selected;
  void select(std::size_t channel) override { selected.push_back(channel); }
  bool configure(int, bool) override { return true; }
  void discard() override {}
  std::size_t read(std::uint8_t*, std::size_t, unsigned wait_ms) override { g_now += wait_ms; return 0; }
  bool write(const std::uint8_t*, std::size_t) override { return true; }
};

static void test_a_paused_member_leaves_the_others_reading() {
  g_now = 0;
  std::vector<MuxMember> members;
  for (std::size_t i = 0; i < 2; ++i) {
    MuxMember m;
    m.port = i;
    m.channel = i;
    m.poller = std::make_unique<Poller>(armor::config::Kind::kVoltronic, "node", "inverter-" + std::to_string(i), 2, 0, "pi30", 0);
    members.push_back(std::move(m));
  }
  MuxGroup group(std::move(members));
  group.set_paused(0, true);
  IdleLine line;
  MuxClock clock;
  clock.uptime_ms = [] { return g_now; };
  clock.wall_clock_is_set = [] { return false; };
  clock.wall_clock_ms = [] { return g_now; };
  for (int i = 0; i < 3000; ++i) group.step(line, clock, 20);
  CHECK(!line.selected.empty());
  CHECK(std::all_of(line.selected.begin(), line.selected.end(), [](std::size_t c) { return c == 1; }));
  group.set_paused(0, false);
  line.selected.clear();
  for (int i = 0; i < 3000; ++i) group.step(line, clock, 20);
  CHECK(std::find(line.selected.begin(), line.selected.end(), 0u) != line.selected.end());
}

int main() {
  test_only_reading_questions();
  test_a_silent_port_is_swept_whole_and_in_time();
  test_echo_answer_and_noise_are_told_apart();
  test_what_came_is_recognised();
  test_the_speeds_of_each_port();
  test_stop_and_time_limit();
  test_the_record_is_read_in_pieces();
  test_a_paused_member_leaves_the_others_reading();
  std::printf("port probe: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
