// ARMOR-SOLAR - a simulated Pylontech battery console for the tests: a stand-in that answers the way the real console's logs show (the echo of the line, "@", the text, "Command completed
// successfully", "$$", the prompt), including the two modes (`pylon>` and, after `login`, `pylon_debug>`), "Invalid command or fail to excute.", "Unknown command" and the bootloader's
// XMODEM-1K handshake. NOTHING in the tests has been tried on a real battery: this is the bench the operations are checked against until the owner tries them on a spare one.
// The service password used here is a dummy value made up for the tests.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../core/battery_info.hpp"
#include "../core/battery_runner.hpp"
#include "../core/xmodem.hpp"

namespace bench {

constexpr const char* kDummyPassword = "dummy-service-pw";

inline std::vector<std::uint8_t> make_image(const std::string& board, int major, int minor, std::size_t size = 200'000) {
  std::vector<std::uint8_t> image(size, 0x5A);
  for (std::size_t i = 0; i < size; ++i) image[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xFF);
  image[0x400] = static_cast<std::uint8_t>(minor);
  image[0x401] = static_cast<std::uint8_t>(major);
  for (std::size_t i = 0; i < 11; ++i) image[0x418 + i] = i < board.size() ? static_cast<std::uint8_t>(board[i]) : 0;
  return image;
}

class Battery {
 public:
  // identity
  std::string model = "US3000C", board = "", serial = "PPTBH02200409223", soft = "V3.0", spec = "48V/74AH";
  int cells = 15;
  // state
  bool debug_mode = false, off = false, in_bootloader = false;
  long total_coul_mah = 74000;
  std::map<std::string, std::string> outputs = {{"cfet", "ON"}, {"dfet", "ON"}, {"buzz", "OFF"}, {"heat", "OFF"}, {"mostest", "OFF"}};
  std::map<std::string, std::string> config;           // wire name -> value
  std::map<std::string, std::string> prot;
  std::string clock = "25-03-09 19:01:42";
  int stat_cycles = 1344, resets = 0;
  std::vector<std::string> received_lines;             // every line the console got (with the password replaced), for the tests
  std::vector<std::string> writes;                     // the lines that changed something
  // faults to inject
  bool silent = false;                                 // never answers
  bool refuse_totalcoul = false;                       // answers "Unknown command" (an older firmware)
  bool wrong_clock_format = false;                     // takes `time` but keeps another value
  int nak_first_packets = 0;                           // answer NAK to the first N packets
  int cancel_at_packet = 0;                            // answer CAN CAN at this packet number (1-based)
  bool never_ask_crc = false;                          // the bootloader does not send 'C'
  bool ask_checksum_mode = false;                      // the bootloader sends NAK instead of 'C'
  bool fail_tbar = false;                              // `tbar` answers 'Test result:fail'
  bool bad_end = false;                                // after the transfer says "Updata error last time"
  std::string password = kDummyPassword;
  std::string image_board, image_version;
  std::vector<std::uint8_t> received_image;
  int packets_accepted = 0, packets_rejected = 0;

  void feed(const std::uint8_t* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
      if (off || silent) return;
      if (in_bootloader) { boot_byte(data[i]); continue; }
      const char c = static_cast<char>(data[i]);
      if (c == '\n') continue;
      if (c == '\r') { const std::string typed = line_; line_.clear(); handle(typed); }
      else line_ += c;
    }
  }
  void feed(const std::vector<std::uint8_t>& v) { feed(v.data(), v.size()); }

  // What the battery says, taken once.
  std::vector<std::uint8_t> take() { std::vector<std::uint8_t> out(out_.begin(), out_.end()); out_.clear(); return out; }

  // The bootloader asks for CRC mode now and then.
  void tick(std::uint64_t now_ms) {
    if (!in_bootloader || now_ms - last_c_ms_ < 1000) return;
    last_c_ms_ = now_ms;
    if (never_ask_crc) return;
    out_ += ask_checksum_mode ? static_cast<char>(armor::solar::xmodem::kNak) : 'C';
  }

 private:
  static std::string crlf(const std::string& s) { std::string o; for (char c : s) { if (c == '\n') o += "\r\n"; else o += c; } return o; }
  std::string prompt() const { return debug_mode ? "pylon_debug>" : "pylon>"; }
  void reply(const std::string& echo, const std::string& body, bool success = true) {
    std::string t = echo + "\n@\n" + body;
    if (success) t += "Command completed successfully\n";
    t += "$$\n" + prompt();
    out_ += crlf(t);
  }
  void invalid(const std::string& echo, const std::string& usage = "Usage:\n") { out_ += crlf(echo + "\n@\nInvalid command or fail to excute.\n" + usage + "$$\n" + prompt()); }
  void unknown(const std::string& echo, const std::string& name) { out_ += crlf(echo + "\n@\nUnknown command '" + name + "' - try 'help'\n$$\n" + prompt()); }

  std::vector<std::string> words(const std::string& line) const {
    std::vector<std::string> w;
    std::string cur;
    for (char c : line) { if (c == ' ') { if (!cur.empty()) { w.push_back(cur); cur.clear(); } } else cur += c; }
    if (!cur.empty()) w.push_back(cur);
    return w;
  }
  std::string info_text() const {
    std::string t = "Device address      : 1\nManufacturer        : Pylon\nDevice name         : " + model + "\nBoard version       : V10R04\n";
    if (!board.empty()) t += "Board               : " + board + "\n";
    t += "Main Soft version   : B68.8.0.0\nSoft  version       : " + soft + "\nBoot  version       : V2.1\nComm version        : V2.0\nRelease Date        : 24-07-25\nBarcode             : " + serial + "\n";
    t += "Specification       : " + spec + "\nCell Number         : " + std::to_string(cells) + "\nMax Dischg Curr     : -90000mA\nMax Charge Curr     : 90000mA\nEPONPort rate       : 1200\nConsole Port rate   : 115200\n";
    return t;
  }

  void handle(const std::string& line) {
    const std::vector<std::string> w = words(line);
    std::string shown = line;
    if (!w.empty() && w[0] == "login") shown = "login ***";
    received_lines.push_back(shown);
    if (w.empty()) { out_ += crlf("\n" + prompt()); return; }
    const std::string& c = w[0];
    const std::string echo = line;   // a real console echoes what it was typed, password included: the code under test must redact it
    if (c == "login") {
      if (!debug_mode) {
        if (w.size() == 2 && w[1] == password) { debug_mode = true; reply(echo, ""); }
        else invalid(echo, "Usage:\nLogin <PW> mode - login <PW>\n");
      } else { debug_mode = false; out_ += crlf(echo + "\n@\nQuit current mode\nInvalid command or fail to excute.\nUsage:\nLogin <PW> mode - login <PW>\n$$\n" + prompt()); }
      return;
    }
    if (c == "logout") { if (debug_mode) { debug_mode = false; out_ += crlf(echo + "\n@\n$$\n" + prompt()); } else unknown(echo, "logout"); return; }
    if (c == "info") { reply(echo, info_text()); return; }
    if (c == "stat") { reply(echo, "Device address           1\nData Items      :     1689\nCYCLE Times     :     " + std::to_string(stat_cycles) + "\nPwr Percent     :       89\nPwr Coulomb     : 237095460\n"); return; }
    if (c == "time" && w.size() == 1) { reply(echo, "Time            : " + clock + "\n"); return; }
    if (c == "totalcoul" && refuse_totalcoul) { unknown(echo, "totalcoul"); return; }
    if (c == "totalcoul" && w.size() == 1) { reply(echo, " Total Coul: " + std::to_string(total_coul_mah) + " mAH\n"); return; }
    if (c == "setlowcfg") { reply(echo, "ShutSoc      = 3 %\nShutSocTime  = 600 s\n"); return; }
    if (c == "config" && w.size() == 1) { std::string t; for (auto& kv : config) t += kv.first + " : " + kv.second + "\n"; reply(echo, t); return; }
    if (c == "prot" && w.size() == 1) { std::string t; for (auto& kv : prot) t += kv.first + " : " + kv.second + "\n"; reply(echo, t); return; }
    if (c == "ctrl" && w.size() == 2) {
      auto it = outputs.find(w[1]);
      if (it == outputs.end()) { invalid(echo); return; }
      std::string label = w[1];
      reply(echo, label + "   : " + it->second + "   ");
      return;
    }
    if (!debug_mode) { unknown(echo, c); return; }
    // administrator commands
    if (c == "time" && w.size() == 7) {
      writes.push_back(line);
      if (!wrong_clock_format) clock = w[1] + "-" + w[2] + "-" + w[3] + " " + w[4] + ":" + w[5] + ":" + w[6];
      reply(echo, "");
      return;
    }
    if (c == "totalcoul" && w.size() == 2) { writes.push_back(line); total_coul_mah = std::stol(w[1]) * 1000; reply(echo, "Test result:pass\n"); return; }
    if (c == "tbar" && w.size() == 2 && fail_tbar) { reply(echo, "Test result:fail CODE err\n"); return; }
    if (c == "tbar" && w.size() == 2) { writes.push_back(line); serial = w[1]; reply(echo, "Test result:pass\n"); return; }
    if (c == "tbar") { reply(echo, "Test result:fail CODE err\n"); return; }
    if (c == "eepe" && w.size() == 2 && w[1] == "0") { writes.push_back(line); serial.clear(); total_coul_mah = 100000; reply(echo, ""); return; }
    if (c == "eepe") { invalid(echo, "Usage:\nEEPROM erase - eepe [chip]\n"); return; }
    if (c == "trst") { writes.push_back(line); debug_mode = false; ++resets; out_ += crlf(echo + "\n@pylon>"); return; }
    if (c == "shut") { writes.push_back(line); off = true; return; }
    if (c == "statc") { writes.push_back(line); stat_cycles = 0; reply(echo, ""); return; }
    if (c == "logc") { writes.push_back(line); reply(echo, ""); return; }
    if (c == "datac" && w.size() == 2 && (w[1] == "event" || w[1] == "history")) { writes.push_back(line); reply(echo, ""); return; }
    if (c == "ctrl" && w.size() == 3 && (w[2] == "on" || w[2] == "off")) {
      if (!outputs.count(w[1]) && w[1] != "afuse") { invalid(echo); return; }
      writes.push_back(line);
      if (w[1] != "afuse") outputs[w[1]] = w[2] == "on" ? "ON" : "OFF";
      reply(echo, "");
      return;
    }
    if (c == "config" && w.size() == 3) { writes.push_back(line); config[w[1]] = w[2]; reply(echo, ""); return; }
    if (c == "config" && w.size() == 2 && (w[1] == "save" || w[1] == "default")) { writes.push_back(line); if (w[1] == "default") config.clear(); reply(echo, ""); return; }
    if (c == "prot" && w.size() == 3 && (w[2] == "en" || w[2] == "dis")) { writes.push_back(line); prot[w[1]] = w[2]; reply(echo, ""); return; }
    if (c == "prot" && w.size() == 2 && w[1] == "default") { writes.push_back(line); prot.clear(); reply(echo, ""); return; }
    if (c == "updata") {
      writes.push_back(line);
      out_ += crlf(echo + "\n@\nCurrent Soft version\n : B68.8\nRestart the BMS system!\n\nBootLoader soft version: V2.0\n\nProgram start!\n\n");
      in_bootloader = true; last_c_ms_ = 0; expected_block_ = 1; eot_nak_sent_ = false; packet_number_seen_ = 0; received_image.clear(); buffer_.clear();
      return;
    }
    invalid(echo);
  }

  // XMODEM-1K receiver
  void boot_byte(std::uint8_t b) {
    using namespace armor::solar::xmodem;
    if (buffer_.empty()) {
      if (b == kEot) {
        if (!eot_nak_sent_) { eot_nak_sent_ = true; out_ += static_cast<char>(kNak); }
        else finish_transfer();
        return;
      }
      if (b != kStx) return;
    }
    buffer_.push_back(b);
    if (buffer_.size() < kBlock + 5) return;
    const std::vector<std::uint8_t> p = buffer_;
    buffer_.clear();
    ++packet_number_seen_;
    if (cancel_at_packet > 0 && packet_number_seen_ == cancel_at_packet) { out_ += static_cast<char>(kCan); out_ += static_cast<char>(kCan); return; }
    const bool header_ok = p[1] == expected_block_ && p[2] == static_cast<std::uint8_t>(~expected_block_);
    const std::uint16_t crc = static_cast<std::uint16_t>((p[kBlock + 3] << 8) | p[kBlock + 4]);
    const bool crc_ok = crc == crc16(p.data() + 3, kBlock);
    if (nak_first_packets > 0) { --nak_first_packets; ++packets_rejected; out_ += static_cast<char>(kNak); return; }
    if (!header_ok || !crc_ok) { ++packets_rejected; out_ += static_cast<char>(kNak); return; }
    received_image.insert(received_image.end(), p.begin() + 3, p.begin() + 3 + kBlock);
    expected_block_ = static_cast<std::uint8_t>(expected_block_ + 1);
    ++packets_accepted;
    out_ += static_cast<char>(kAck);
  }
  void finish_transfer() {
    using namespace armor::solar::xmodem;
    out_ += static_cast<char>(kAck);
    in_bootloader = false;
    if (bad_end) { out_ += crlf("Updata error last time\nPlease updata again\n"); return; }
    out_ += crlf("Updata OK!\nSystem will be started by BMS after 1 sec!\n\npylon>");
    debug_mode = false;
    serial.clear();   // the update leaves the serial number to be written again
    // the new version, from the image header
    if (received_image.size() > 0x401) soft = "V" + std::to_string(received_image[0x401]) + "." + std::to_string(received_image[0x400]);
    eot_nak_sent_ = false; packet_number_seen_ = 0;
  }

  std::string line_, out_;
  std::vector<std::uint8_t> buffer_;
  bool eot_nak_sent_ = false;
  std::uint8_t expected_block_ = 1;
  int packet_number_seen_ = 0;
  std::uint64_t last_c_ms_ = 0;
};

// Drives a runner against the bench in virtual time. Returns the milliseconds that passed (a bound of 20 minutes ends a stuck run).
inline std::uint64_t drive(armor::solar::battery::Runner& runner, Battery& battery, std::uint64_t start_ms = 1'000) {
  std::uint64_t now = start_ms;
  runner.start(now);
  while (!runner.finished() && now - start_ms < 20 * 60'000) {
    const std::vector<std::uint8_t> tx = runner.next_tx(now);
    if (!tx.empty()) battery.feed(tx);
    battery.tick(now);
    const std::vector<std::uint8_t> rx = battery.take();
    if (!rx.empty()) runner.on_rx(rx.data(), rx.size(), now);
    now += 20;
  }
  return now - start_ms;
}

}  // namespace bench
