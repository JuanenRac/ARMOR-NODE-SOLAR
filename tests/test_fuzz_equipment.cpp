// ARMOR-NODE-SOLAR - host tests with a fixed seed: the readers of what the equipment answers (the ANT-BMS frames of both protocols, the Pylontech console text and its RS-485 frames,
// the Voltronic replies and their fields) must never crash and never accept a damaged frame, whatever arrives on the serial line.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "pylontech_real.hpp"
#include "pylontech_us2000_real.hpp"
#include "../core/ant_bms.hpp"
#include "../core/pylontech.hpp"
#include "../core/voltronic.hpp"
#include "ant_frames.hpp"

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

// the same numbers on every run, so a failure can be repeated
static std::uint32_t rng_state = 0x1B873593u;
static std::uint32_t next_random() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}
static std::uint32_t below(std::uint32_t limit) { return limit == 0 ? 0 : next_random() % limit; }

static std::vector<std::uint8_t> noise_bytes(std::size_t length) {
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<std::uint8_t>(next_random() & 0xff));
  return out;
}

static std::string noise_text(std::size_t length) {
  std::string out;
  for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<char>(next_random() & 0xff));
  return out;
}

// a valid text with a few characters changed, dropped or added
static std::string mutate(std::string text) {
  const std::uint32_t edits = 1 + below(4);
  for (std::uint32_t i = 0; i < edits && !text.empty(); ++i) {
    const std::size_t at = below(static_cast<std::uint32_t>(text.size()));
    switch (below(3)) {
      case 0: text[at] = static_cast<char>(next_random() & 0xff); break;
      case 1: text.erase(at, 1); break;
      default: text.insert(at, 1, static_cast<char>(next_random() & 0xff)); break;
    }
  }
  return text;
}

using namespace armor::solar;

static const std::vector<const std::vector<std::uint8_t>*>& old_frames() {
  static const std::vector<const std::vector<std::uint8_t>*> frames = {&kOld8S, &kOld2021, &kOld2019};
  return frames;
}
static const std::vector<const std::vector<std::uint8_t>*>& new_frames() {
  static const std::vector<const std::vector<std::uint8_t>*> frames = {&kNew2021, &kNew16S};
  return frames;
}

// the check sum of the old protocol and the CRC of the new one, written again after a field was changed on purpose
static void fix_old(std::vector<std::uint8_t>& frame) {
  unsigned sum = 0;
  for (std::size_t i = 4; i < ant::kOldFrameSize - 2; ++i) sum += frame[i];
  frame[138] = static_cast<std::uint8_t>((sum >> 8) & 0xFF);
  frame[139] = static_cast<std::uint8_t>(sum & 0xFF);
}
static void fix_new(std::vector<std::uint8_t>& frame) {
  const std::uint16_t crc = ant::crc16(frame.data() + 1, frame.size() - 5);
  frame[frame.size() - 4] = static_cast<std::uint8_t>(crc & 0xFF);
  frame[frame.size() - 3] = static_cast<std::uint8_t>(crc >> 8);
}

static bool consistent(const ant::Reading& reading) {
  return reading.cells >= 1 && reading.cells <= ant::kMaxCells && static_cast<int>(reading.module.cells_v.size()) == reading.cells;
}

static void ant_known_frames() {
  for (const auto* frame : old_frames()) {
    ant::Reading reading;
    CHECK(frame->size() == ant::kOldFrameSize && ant::decode_old(frame->data(), frame->size(), reading) && consistent(reading));
  }
  for (const auto* frame : new_frames()) {
    ant::Reading reading;
    CHECK(ant::decode_new(frame->data(), frame->size(), reading) && consistent(reading));
  }
}

static void ant_noise() {
  for (int round = 0; round < 3000; ++round) {
    const std::vector<std::uint8_t> bytes = noise_bytes(below(260));
    ant::Reading reading;
    if (ant::decode_old(bytes.data(), bytes.size(), reading)) CHECK(consistent(reading));
    if (ant::decode_new(bytes.data(), bytes.size(), reading)) CHECK(consistent(reading));
  }
  // noise with the right head and size and a true check sum: only the fields can reject it
  for (int round = 0; round < 1500; ++round) {
    std::vector<std::uint8_t> old_noise = noise_bytes(ant::kOldFrameSize);
    old_noise[0] = 0xAA; old_noise[1] = 0x55; old_noise[2] = 0xAA; old_noise[3] = 0xFF;
    fix_old(old_noise);
    ant::Reading reading;
    const bool accepted = ant::decode_old(old_noise.data(), old_noise.size(), reading);
    CHECK(accepted == (old_noise[123] >= 1 && old_noise[123] <= ant::kMaxCells));
    if (accepted) CHECK(consistent(reading));

    const std::size_t body = 10 + below(150);
    std::vector<std::uint8_t> new_noise = noise_bytes(6 + body + 4);
    new_noise[0] = 0x7E; new_noise[1] = 0xA1; new_noise[2] = 0x11;
    new_noise[5] = static_cast<std::uint8_t>(body);
    new_noise[new_noise.size() - 2] = 0xAA; new_noise[new_noise.size() - 1] = 0x55;
    fix_new(new_noise);
    if (ant::decode_new(new_noise.data(), new_noise.size(), reading)) CHECK(consistent(reading));
  }
}

static void ant_damaged_frames() {
  // one changed byte inside what the sum or the CRC covers is always seen
  for (const auto* frame : old_frames()) {
    for (std::size_t at = 4; at < ant::kOldFrameSize - 2; ++at) {
      std::vector<std::uint8_t> bad = *frame;
      bad[at] = static_cast<std::uint8_t>(bad[at] + 1 + below(255));
      ant::Reading reading;
      CHECK(!ant::decode_old(bad.data(), bad.size(), reading));
    }
  }
  for (const auto* frame : new_frames()) {
    for (std::size_t at = 1; at + 4 < frame->size(); ++at) {
      std::vector<std::uint8_t> bad = *frame;
      bad[at] = static_cast<std::uint8_t>(bad[at] + 1 + below(255));
      ant::Reading reading;
      CHECK(!ant::decode_new(bad.data(), bad.size(), reading));
    }
  }
  // a frame cut short or with bytes added is not a frame
  for (const auto* frame : old_frames()) {
    for (std::size_t cut = 0; cut < frame->size(); ++cut) {
      ant::Reading reading;
      CHECK(!ant::decode_old(frame->data(), cut, reading));
    }
  }
  for (const auto* frame : new_frames()) {
    for (std::size_t cut = 0; cut < frame->size(); ++cut) {
      ant::Reading reading;
      CHECK(!ant::decode_new(frame->data(), cut, reading));
    }
    std::vector<std::uint8_t> longer = *frame;
    longer.push_back(0x00);
    ant::Reading reading;
    CHECK(!ant::decode_new(longer.data(), longer.size(), reading));
  }
}

static void ant_changed_fields() {
  // the cell count and the sensor count changed on purpose, with the sum or the CRC written again: the reader must keep inside the frame
  for (int round = 0; round < 1500; ++round) {
    std::vector<std::uint8_t> old_frame = *old_frames()[below(3)];
    old_frame[123] = static_cast<std::uint8_t>(below(256));
    for (int edits = static_cast<int>(below(4)); edits > 0; --edits) old_frame[4 + below(120)] = static_cast<std::uint8_t>(below(256));
    fix_old(old_frame);
    ant::Reading reading;
    if (ant::decode_old(old_frame.data(), old_frame.size(), reading)) CHECK(consistent(reading));

    std::vector<std::uint8_t> new_frame = *new_frames()[below(2)];
    new_frame[8] = static_cast<std::uint8_t>(below(20));
    new_frame[9] = static_cast<std::uint8_t>(below(50));
    for (int edits = static_cast<int>(below(4)); edits > 0; --edits) new_frame[6 + below(static_cast<std::uint32_t>(new_frame.size() - 12))] = static_cast<std::uint8_t>(below(256));
    fix_new(new_frame);
    if (ant::decode_new(new_frame.data(), new_frame.size(), reading)) CHECK(consistent(reading));
  }
}

static void ant_framer() {
  // a stream of noise with whole frames in it: every frame that comes out is whole, and the framer never grows past its limit
  ant::Framer framer;
  std::vector<std::uint8_t> frame;
  ant::Protocol protocol = ant::Protocol::kOld;
  int whole = 0;
  for (int round = 0; round < 400; ++round) {
    for (std::uint8_t byte : noise_bytes(below(40))) {
      if (framer.feed(byte, frame, protocol)) CHECK(protocol != ant::Protocol::kOld ? frame.size() <= ant::kNewMaxFrameSize : frame.size() == ant::kOldFrameSize);
    }
    const std::vector<std::uint8_t>& real = (round % 2 == 0) ? *old_frames()[below(3)] : *new_frames()[below(2)];
    for (std::uint8_t byte : real) {
      if (framer.feed(byte, frame, protocol)) {
        ++whole;
        CHECK(protocol != ant::Protocol::kOld || frame.size() == ant::kOldFrameSize);
      }
    }
    framer.clear();
  }
  CHECK(whole >= 300);
}

static void pylontech_text() {
  const std::vector<std::string> samples = {kRealBat, kRealSoh, kRealStat, kRealDataEvent, kUs2000Pwr, kUs2000Bat, kUs2000Data, kUs2000GetPwr, kUs2000Info, kUs2000Soh};
  for (const std::string& sample : samples) {
    for (int round = 0; round < 150; ++round) {
      const std::string text = (round % 5 == 0) ? noise_text(below(300)) : mutate(sample);
      std::vector<pylontech::Module> modules;
      std::vector<double> cells;
      pylontech::Module module;
      pylontech::Event event;
      pylontech::PowerSnapshot power;
      pylontech::DataList list;
      (void)pylontech::parse_pwr(text, modules);
      (void)pylontech::parse_bat(text, cells);
      (void)pylontech::parse_info(text, module);
      (void)pylontech::parse_event(text, event);
      (void)pylontech::parse_soh(text, module);
      (void)pylontech::parse_getpwr(text, power);
      (void)pylontech::parse_datalist(text, list);
      CHECK(true);
    }
  }
  // the whole real texts still read after all that
  std::vector<pylontech::Module> modules;
  CHECK(pylontech::parse_pwr(kUs2000Pwr, modules) > 0 && !modules.empty());
}

static void pylontech_frames() {
  for (int round = 0; round < 1000; ++round) {
    std::string info;
    for (std::uint32_t i = below(30); i > 0; --i) info.push_back("0123456789ABCDEF"[below(16)]);
    const unsigned version = below(256), address = below(256), cid1 = below(256), cid2 = below(256);
    const std::string text = pylontech::build_frame(version, address, cid1, cid2, info);
    pylontech::Frame frame;
    CHECK(pylontech::parse_frame(text, frame) && frame.version == version && frame.address == address && frame.cid1 == cid1 && frame.cid2 == cid2 && frame.info == info);
    // one changed character between the head and the end is seen by the checksum
    std::string bad = text;
    const std::size_t at = 1 + below(static_cast<std::uint32_t>(bad.size() - 2));
    bad[at] = static_cast<char>(bad[at] == 'A' ? 'B' : 'A');
    CHECK(!pylontech::parse_frame(bad, frame));
    // anything made of the same text must not crash
    (void)pylontech::parse_frame(mutate(text), frame);
    (void)pylontech::parse_frame(noise_text(below(80)), frame);
  }
  for (std::size_t cut = 0; cut < 20; ++cut) {
    pylontech::Frame frame;
    CHECK(!pylontech::parse_frame(pylontech::build_frame(2, 1, 0x46, 0x42, "01").substr(0, cut), frame));
  }
}

static std::vector<std::uint8_t> voltronic_reply(const std::string& text) {
  std::vector<std::uint8_t> frame;
  frame.push_back('(');
  frame.insert(frame.end(), text.begin(), text.end());
  const std::array<std::uint8_t, 2> crc = voltronic::wire_crc(frame.data(), frame.size());
  frame.push_back(crc[0]);
  frame.push_back(crc[1]);
  frame.push_back('\r');
  return frame;
}

static void voltronic_replies() {
  const std::vector<std::string> samples = {
      "000.0 00.0 230.0 50.0 0000 0000 000 400 54.00 000 100 0030 0000 000.0 00.00 00000 00010000 00 00 00000 010",
      "B", "000.0 00.0 000.0 000", "230.0 50.0 230.0 50.0 1000 1000 0 49.0 54.0 1 60 0 0 0 0 0 0 0 0 0",
      "00000000000000000000000000000000", "VOLTRONIC", "00000000", "12345678901234"};
  int rejected = 0, rounds = 0;
  for (int round = 0; round < 2000; ++round) {
    const std::string text = samples[below(static_cast<std::uint32_t>(samples.size()))];
    const std::vector<std::uint8_t> frame = voltronic_reply(text);
    std::string back;
    CHECK(voltronic::parse_reply(frame.data(), frame.size(), back) && back == text);
    std::vector<std::uint8_t> bad = frame;
    const std::size_t at = 1 + below(static_cast<std::uint32_t>(bad.size() - 2));
    bad[at] = static_cast<std::uint8_t>(bad[at] + 1 + below(255));
    ++rounds;
    if (!voltronic::parse_reply(bad.data(), bad.size(), back)) ++rejected;
    const std::vector<std::uint8_t> junk = noise_bytes(below(120));
    (void)voltronic::parse_reply(junk.data(), junk.size(), back);
    (void)voltronic::parse_reply_revo(junk.data(), junk.size(), back);
    (void)voltronic::parse_reply_revo(bad.data(), bad.size(), back);
  }
  CHECK(rejected * 100 >= rounds * 98);   // a damaged reply is refused (the CRC bytes that dodge the line breaks may let one in by chance, never many)
}

static void voltronic_fields() {
  const std::vector<std::string> samples = {
      "000.0 00.0 230.0 50.0 0000 0000 000 400 54.00 000 100 0030 0000 000.0 00.00 00000 00010000 00 00 00000 010",
      "230.0 50.0 230.0 50.0 1000 1000 0 49.0 54.0 1 60 0 0 0 0 0 0 0 0 0", "B", "00000000000000000000000000000000", "VOLTRONIC", "12345678901234"};
  for (int round = 0; round < 2500; ++round) {
    const std::string text = (round % 4 == 0) ? noise_text(below(160)) : mutate(samples[below(static_cast<std::uint32_t>(samples.size()))]);
    voltronic::Status status;
    voltronic::Pv2 pv2;
    voltronic::ParallelUnit unit;
    voltronic::Ratings ratings;
    char mode = 0;
    std::string word;
    std::vector<std::string> active;
    double energy = 0;
    (void)voltronic::parse_qpigs(text, status);
    (void)voltronic::parse_qpigs_revo(text, status);
    (void)voltronic::parse_qmod(text, mode);
    (void)voltronic::parse_qpigs2(text, pv2);
    (void)voltronic::parse_qpgs(text, unit);
    (void)voltronic::parse_qpiws(text, active);
    (void)voltronic::parse_qpiri(text, ratings);
    (void)voltronic::parse_qid(text, word);
    (void)voltronic::parse_qvfw(text, word);
    (void)voltronic::parse_qet(text, energy);
    CHECK(true);
  }
}

int main() {
  ant_known_frames();
  ant_noise();
  ant_damaged_frames();
  ant_changed_fields();
  ant_framer();
  pylontech_text();
  pylontech_frames();
  voltronic_replies();
  voltronic_fields();
  std::printf("fuzz_equipment: %d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
