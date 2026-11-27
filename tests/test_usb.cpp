// ARMOR-NODE-SOLAR - host tests of the USB-HID line of an inverter (core/usb_hid_link.hpp) and of its setting.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The adapter here is a double: it joins the output reports into the question and cuts its answer into reports of 8 bytes. No USB device was used.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/poller.hpp"
#include "../core/solar_config.hpp"
#include "../core/usb_hid_link.hpp"

using namespace armor;

static int g_failures = 0;
#define CHECK(condition) do { if (!(condition)) { ++g_failures; std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); } } while (false)

using Bytes = std::vector<std::uint8_t>;

static usbhid::DeviceInfo cypress() {
  usbhid::DeviceInfo info;
  info.present = true;
  info.hid = true;
  info.vid = 0x0665;
  info.pid = 0x5161;
  info.interface_class = 3;
  return info;
}

// A frame of the protocol: '(' + text + CRC + CR.
static Bytes reply(const std::string& text) {
  Bytes frame{'('};
  frame.insert(frame.end(), text.begin(), text.end());
  const auto crc = solar::voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]);
  frame.push_back(crc[1]);
  frame.push_back('\r');
  return frame;
}

// The adapter cuts an answer into reports of 8, the last filled with zeros.
static void deliver(usbhid::Link& link, const Bytes& frame) {
  for (std::size_t from = 0; from < frame.size(); from += 8) {
    std::uint8_t report[8] = {};
    for (std::size_t i = 0; i < 8 && from + i < frame.size(); ++i) report[i] = frame[from + i];
    link.on_report(report, 8);
  }
}

static Bytes take(usbhid::Link& link) {
  Bytes out(1024);
  out.resize(link.read(out.data(), out.size()));
  return out;
}

static void fragmentation() {
  usbhid::Link link;
  link.connected(cypress());
  Bytes question;
  CHECK(solar::voltronic::build_command("QPIGS2", question));   // 9 bytes: two reports
  std::vector<Bytes> sent;
  CHECK(link.transmit(question.data(), question.size(), [&sent](const std::uint8_t* r, std::size_t n) { sent.emplace_back(r, r + n); return true; }));
  CHECK(sent.size() == 2);
  CHECK(sent[0].size() == 8 && sent[1].size() == 8);
  CHECK(Bytes(sent[0].begin(), sent[0].end()) == Bytes(question.begin(), question.begin() + 8));
  CHECK(sent[1][0] == '\r');
  for (std::size_t i = 1; i < 8; ++i) CHECK(sent[1][i] == 0);
  CHECK(link.counters.reports_out == 2 && link.counters.bytes_out == 9);
  // a report that does not go stops the question
  int tries = 0;
  CHECK(!link.transmit(question.data(), question.size(), [&tries](const std::uint8_t*, std::size_t) { ++tries; return false; }));
  CHECK(tries == 1 && link.counters.send_failures == 1);
}

static void joining_and_crc() {
  usbhid::Link link;
  link.connected(cypress());
  const std::uint8_t idle[8] = {};
  link.on_report(idle, 8);
  CHECK(link.waiting() == 0);
  for (const char* text : {"B", "L", "PI30", "230.0 50.0 230.0 50.0 0300 0250 010 360 48.0 000 100 0031 00.0 000.0 00.00 00000"}) {
    const Bytes frame = reply(text);
    deliver(link, frame);
    link.on_report(idle, 8);
    const Bytes got = take(link);
    CHECK(got == frame);   // the filling after the CR is gone
    std::string read;
    CHECK(solar::voltronic::parse_reply(got.data(), got.size(), read) && read == text);
  }
  // a zero before the CR stays (a CRC byte may be zero)
  const std::uint8_t with_zero[8] = {'(', 'A', 0, 'B', '\r', 'x', 'y', 0};
  link.on_report(with_zero, 8);
  CHECK(take(link) == Bytes({'(', 'A', 0, 'B', '\r'}));
  // a damaged byte is still refused by the CRC
  Bytes damaged = reply("PI30");
  damaged[2] ^= 1;
  deliver(link, damaged);
  const Bytes got = take(link);
  std::string read;
  CHECK(!solar::voltronic::parse_reply(got.data(), got.size(), read));
  // read in small pieces, and the buffer never grows past its limit
  deliver(link, reply("PI30"));
  std::uint8_t three[3];
  CHECK(link.read(three, 3) == 3 && three[0] == '(');
  link.discard();
  const std::uint8_t full[8] = {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'};
  for (int i = 0; i < 100; ++i) link.on_report(full, 8);
  CHECK(link.waiting() == usbhid::kBufferLimit);
  CHECK(link.counters.dropped == 800 - usbhid::kBufferLimit);
}

static void disconnection() {
  usbhid::Link link;
  std::uint8_t one[1] = {'Q'};
  const auto yes = [](const std::uint8_t*, std::size_t) { return true; };
  CHECK(!link.transmit(one, 1, yes));   // nothing plugged in
  CHECK(link.id_text().empty());
  link.connected(cypress());
  CHECK(link.ready() && link.id_text() == "0665:5161");
  const std::uint8_t half[8] = {'(', '2', '3', '0', '.', '0', ' ', '5'};
  link.on_report(half, 8);
  CHECK(link.waiting() == 8);
  link.disconnected();
  CHECK(!link.ready() && link.waiting() == 0 && link.counters.disconnects == 1);
  link.on_report(half, 8);   // a late report of the device that is gone
  CHECK(link.waiting() == 0);
  link.connected(cypress());
  CHECK(link.ready() && link.waiting() == 0 && link.counters.connects == 2);
  usbhid::DeviceInfo serial;   // something that is not HID: seen, never used
  serial.present = true;
  serial.vid = 0x1a86;
  serial.pid = 0x7523;
  serial.interface_class = 0xFF;
  link.connected(serial);
  CHECK(!link.ready() && !link.transmit(one, 1, yes));
  CHECK(link.diagnosis().find("1a86:7523") != std::string::npos);
  CHECK(link.diagnosis().find("hid=no") != std::string::npos);
}

static void ids() {
  std::uint16_t id = 1;
  CHECK(usbhid::parse_id("", id) && id == 0);
  CHECK(usbhid::parse_id("0665", id) && id == 0x0665);
  CHECK(usbhid::parse_id("A1f0", id) && id == 0xA1F0);
  CHECK(!usbhid::parse_id("665", id));
  CHECK(!usbhid::parse_id("06g5", id));
  CHECK(usbhid::wanted(0, 0, 1, 2) && usbhid::wanted(1, 0, 1, 2) && !usbhid::wanted(1, 3, 1, 2) && !usbhid::wanted(2, 0, 1, 2));
}

// The poller over the line: the same questions as on a serial port, answered through reports; then silence (the time limit) and the device gone.
static void polling() {
  solar::Poller poller(config::Kind::kVoltronic, "solar-test", "inverter-usb", 5, 0, "pi30");
  usbhid::Link link;
  link.connected(cypress());
  std::uint64_t now = 1000;
  Bytes question;
  const auto send = [&question](const std::uint8_t* r, std::size_t n) { question.insert(question.end(), r, r + n); return true; };
  bool published = false;
  std::string topic, payload;
  for (int turn = 0; turn < 200 && !published; ++turn) {
    now += 100;
    const Bytes request = poller.next_tx(now);
    if (!request.empty()) {
      question.clear();
      CHECK(link.transmit(request.data(), request.size(), send));
      std::string text(question.begin(), question.end());
      if (text.rfind("QPIGS", 0) == 0) deliver(link, reply("230.0 50.0 230.0 50.0 0300 0250 010 360 52.40 012 081 0041 10.9 180.0 52.40 00000 00010110 00 00 01960 010"));
      else if (text.rfind("QMOD", 0) == 0) deliver(link, reply("L"));
      else if (text.rfind("QPIWS", 0) == 0) deliver(link, reply("00000000000000000000000000000000"));
      else deliver(link, reply("NAK"));
    }
    const Bytes got = take(link);
    if (!got.empty()) poller.on_rx(got.data(), got.size(), now);
    published = poller.take_message(now, topic, payload);
  }
  CHECK(published);
  CHECK(topic == "armor/solar/solar-test/inverter-usb/state");
  CHECK(payload.find("\"kind\":\"inverter\"") != std::string::npos);
  // unplugged: nothing answers, the poller counts time limits and no reading comes
  link.disconnected();
  const auto before = poller.stats().timeouts;
  for (int turn = 0; turn < 200; ++turn) {
    now += 100;
    const Bytes request = poller.next_tx(now);
    if (!request.empty()) CHECK(!link.transmit(request.data(), request.size(), send));
  }
  CHECK(poller.stats().timeouts > before);
}

static void setting() {
  const config::Settings base = config::default_settings("a1b2c3");
  CHECK(!base.usb.enabled);   // off until the owner turns it on
  config::Settings out;
  config::Problems problems;
  CHECK(config::load(R"({"ports":[{"enabled":true,"kind":"voltronic","name":"inv"}],"usb_host":{"enabled":true,"port":1,"vid":"0665","pid":"5161"}})", base, out, problems));
  CHECK(out.usb.enabled && out.usb.port == 1 && out.usb.vid == "0665" && out.usb.pid == "5161");
  config::Settings again;
  CHECK(config::load(config::to_json(out, true), base, again, problems));
  CHECK(again.usb.enabled && again.usb.vid == "0665");
  // the port of the USB needs no pins
  CHECK(config::load(R"({"ports":[{"enabled":true,"kind":"voltronic","name":"inv","rx":-1,"tx":-1}],"usb_host":{"enabled":true,"port":1}})", base, out, problems));
  const auto refused = [&base](const char* document, const char* path) {
    config::Settings s;
    config::Problems p;
    if (config::load(document, base, s, p)) return false;
    for (const config::Problem& problem : p) if (problem.path == path) return true;
    return false;
  };
  CHECK(refused(R"({"usb_host":{"enabled":true,"port":1}})", "usb_host.port"));   // the port is off
  CHECK(refused(R"({"ports":[{"enabled":true,"kind":"pylontech","name":"b"}],"usb_host":{"enabled":true,"port":1}})", "usb_host.port"));
  CHECK(refused(R"({"usb_host":{"port":11}})", "usb_host.port"));
  CHECK(refused(R"({"usb_host":{"vid":"66"}})", "usb_host.vid"));
  CHECK(refused(R"({"usb_host":{"pid":"zzzz"}})", "usb_host.pid"));
}

int main() {
  fragmentation();
  joining_and_crc();
  disconnection();
  ids();
  polling();
  setting();
  std::printf(g_failures == 0 ? "usb: all checks passed\n" : "usb: %d checks failed\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
