// ARMOR-NODE-SOLAR - host tests of the capture of the exchanges with the inverters and batteries (core/frame_capture.hpp), of the poller that feeds it and of its settings: the ring is bounded, off by
// default, refuses anything that looks like a sign-in, and never sees what is not the polling. No real device was used. Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/frame_capture.hpp"
#include "../core/poller.hpp"
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

using namespace armor;

static std::vector<std::uint8_t> bytes_of(const std::string& text) { return std::vector<std::uint8_t>(text.begin(), text.end()); }
static std::string hex_of(const std::vector<std::uint8_t>& bytes) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  for (std::uint8_t b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
  return out;
}

static void test_ring() {
  framecap::Capture capture;
  const std::vector<std::uint8_t> request = {'Q', 'P', 'I', 'G', 'S', 0xB7, 0xA9, '\r'};
  // off by default: nothing is kept and nothing is allocated
  CHECK(!capture.enabled() && capture.capacity() == 0 && capture.size() == 0 && capture.text().empty());
  CHECK(!capture.push(1, true, request.data(), request.size()) && capture.size() == 0);
  // on: bounded to what was asked
  capture.configure(true, 2);
  CHECK(capture.enabled() && capture.capacity() == static_cast<std::size_t>(framecap::kMinFrames));
  capture.configure(true, 5000);
  CHECK(capture.capacity() == static_cast<std::size_t>(framecap::kMaxFrames));
  capture.configure(true, 8);
  CHECK(capture.capacity() == 8);
  // refuses a port that does not exist, empty bytes and no bytes at all
  CHECK(!capture.push(0, true, request.data(), request.size()) && !capture.push(17, true, request.data(), request.size()));
  CHECK(!capture.push(1, true, request.data(), 0) && !capture.push(1, true, nullptr, 4) && capture.size() == 0);
  // a request, then an answer in three pieces: the pieces of one answer make one entry
  CHECK(capture.push(2, true, request.data(), request.size()));
  const std::vector<std::uint8_t> a = bytes_of("(232.0 50.0"), b = bytes_of(" 230.0 50.0"), c = bytes_of(" 0161\r");
  CHECK(capture.push(2, false, a.data(), a.size()) && capture.push(2, false, b.data(), b.size()) && capture.push(2, false, c.data(), c.size()));
  CHECK(capture.size() == 2);
  // the answer of another port, or the next request, starts a new entry
  CHECK(capture.push(3, false, a.data(), a.size()) && capture.size() == 3);
  CHECK(capture.push(2, true, request.data(), request.size()) && capture.size() == 4);
  const std::string text = capture.text();
  CHECK(text.compare(0, 11, "FRAME p2 tx") == 0);
  CHECK(text.find("FRAME p2 rx 28323332") != std::string::npos);                 // "(232"
  std::size_t lines = 0;
  for (char ch : text) if (ch == '\n') ++lines;
  CHECK(lines == 4);
  // the joined answer holds the three pieces in order: "(232.0 50.0 230.0 50.0 0161\r"
  CHECK(text.find("FRAME p2 rx " + hex_of(bytes_of("(232.0 50.0 230.0 50.0 0161\r")) + "\n") != std::string::npos);
  // an answer longer than an entry spills into the next one
  framecap::Capture big;
  big.configure(true, 8);
  const std::vector<std::uint8_t> long_answer(framecap::kEntryBytes + 40, 'x');
  CHECK(big.push(1, false, long_answer.data(), long_answer.size()) && big.size() == 2);
  // the oldest goes when the ring is full
  capture.clear();
  CHECK(capture.size() == 0 && capture.capacity() == 8 && capture.dropped() == 0);
  for (unsigned i = 0; i < 11; ++i) { const std::vector<std::uint8_t> q = bytes_of(std::string("Q") + static_cast<char>('A' + i)); capture.push(1, true, q.data(), q.size()); }
  CHECK(capture.size() == 8 && capture.dropped() == 3);
  CHECK(capture.text().compare(0, 17, "FRAME p1 tx 5144\n") == 0);                                         // QD is now the oldest
  CHECK(capture.text().find("5141\n") == std::string::npos && capture.text().find("5143\n") == std::string::npos);   // QA and QC went
  CHECK(capture.text().find("514b\n") != std::string::npos);                                               // QK is there
  // turning it off frees it
  capture.configure(false, 8);
  CHECK(!capture.enabled() && capture.capacity() == 0 && !capture.push(1, true, request.data(), request.size()));
}

static void test_refuses_sign_in() {
  framecap::Capture capture;
  capture.configure(true, 16);
  const std::vector<std::uint8_t> login = bytes_of("login Pylon\r"), upper = bytes_of("LOGIN x"), pass = bytes_of("my Password is"), fine = bytes_of("pwr\r");
  CHECK(framecap::looks_secret(login.data(), login.size()) && framecap::looks_secret(upper.data(), upper.size()) && framecap::looks_secret(pass.data(), pass.size()));
  CHECK(!framecap::looks_secret(fine.data(), fine.size()) && !framecap::looks_secret(nullptr, 0));
  CHECK(!capture.push(1, true, login.data(), login.size()) && !capture.push(1, false, pass.data(), pass.size()));
  CHECK(capture.size() == 0 && capture.refused() == 2 && capture.text().empty());
  CHECK(capture.push(1, true, fine.data(), fine.size()) && capture.size() == 1);
  capture.clear();
  CHECK(capture.refused() == 0);
}

// The poller feeds the capture with the polling of a port and with nothing else: a full Voltronic cycle gives requests and answers, and no sign-in is ever among them.
static std::vector<std::uint8_t> voltronic_reply(const std::string& text) {
  std::vector<std::uint8_t> frame{'('};
  frame.insert(frame.end(), text.begin(), text.end());
  const auto crc = solar::voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]); frame.push_back(crc[1]); frame.push_back('\r');
  return frame;
}

static void test_poller_feeds_capture() {
  framecap::Capture capture;
  capture.configure(true, 64);
  solar::Poller poller(config::Kind::kVoltronic, "solar-1", "axpert-1", 5, 0, "pi30");
  poller.set_capture([&](bool tx, const std::uint8_t* data, std::size_t length) { capture.push(4, tx, data, length); });
  const std::vector<std::uint8_t> answer = voltronic_reply("232.0 50.0 230.0 50.0 0161 0119 003 460 57.50 012 100 0069 0014 103.8 57.45 00000 00110110 00 00 00856");
  bool asked = false;
  for (std::uint64_t now = 0; now < 4000; now += 10) {
    const std::vector<std::uint8_t> out = poller.next_tx(now);
    if (!out.empty() && std::string(out.begin(), out.end()).compare(0, 5, "QPIGS") == 0 && !asked) {
      asked = true;
      poller.on_rx(answer.data(), 20, now + 20);              // an answer in two pieces
      poller.on_rx(answer.data() + 20, answer.size() - 20, now + 30);
    }
  }
  CHECK(asked && capture.size() >= 2);
  const std::string text = capture.text();
  CHECK(text.find("FRAME p4 tx 5150494753") != std::string::npos);   // "QPIGS" and its check bytes
  CHECK(text.find("FRAME p4 rx 28323332") != std::string::npos);   // "(232"
  CHECK(capture.refused() == 0);
  // the answer is whole in one entry: its first 20 bytes and the rest are joined
  CHECK(text.find("FRAME p4 rx " + hex_of(answer) + "\n") != std::string::npos);
  // a poller with no sink costs nothing and still works
  solar::Poller plain(config::Kind::kVoltronic, "solar-1", "axpert-2", 5, 0, "pi30");
  CHECK(!plain.next_tx(0).empty());
  // a poller whose sink is taken away stops feeding it
  const std::size_t before = capture.size();
  poller.set_capture(nullptr);
  poller.next_tx(9000);
  poller.on_rx(answer.data(), answer.size(), 9010);
  CHECK(capture.size() == before);
}

static void test_settings() {
  config::Settings base = config::default_settings("a1b2c3");
  base.sta.enabled = true;   // a node with no way onto a network is not a valid document
  base.sta.ssid = "casa";
  base.sta.password = "una-clave-larga";
  CHECK(!base.capture_enabled && base.capture_frames == framecap::kDefaultFrames);
  // the values load (this comes BEFORE the refusals: a refusal test is worthless if the same document with good values does not load)
  config::Settings out;
  config::Problems problems;
  CHECK(config::load(R"({"capture":{"enabled":true,"frames":100}})", base, out, problems) && problems.empty());
  CHECK(out.capture_enabled && out.capture_frames == 100);
  // the stored form reads back the same
  const std::string stored = config::to_json(out, true);
  CHECK(stored.find("\"capture\":{\"enabled\":true,\"frames\":100}") != std::string::npos);
  config::Settings back;
  problems.clear();
  CHECK(config::load(stored, base, back, problems) && back.capture_enabled && back.capture_frames == 100 && config::to_json(back, true) == stored);
  CHECK(config::to_json(base, true).find("\"capture\":{\"enabled\":false,\"frames\":48}") != std::string::npos);
  const auto refuses = [&](const char* text, const char* path) {
    config::Problems p;
    config::Settings o;
    if (config::load(text, base, o, p)) return false;
    for (const config::Problem& item : p) if (item.path == path) return true;
    return false;
  };
  CHECK(refuses(R"({"capture":{"frames":7}})", "capture.frames"));
  CHECK(refuses(R"({"capture":{"frames":129}})", "capture.frames"));
  CHECK(refuses(R"({"capture":{"frames":"many"}})", "capture.frames"));
  CHECK(refuses(R"({"capture":{"enabled":1}})", "capture.enabled"));
}

int main() {
  test_ring();
  test_refuses_sign_in();
  test_poller_feeds_capture();
  test_settings();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
