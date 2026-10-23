// ARMOR-NODE-SOLAR - XMODEM-1K with a 16-bit CRC, the way the bootloader of a Pylontech battery receives a firmware image after the console's `updata` command.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The receiver (the battery's bootloader) asks for CRC mode with the character 'C'; the sender answers with packets of 1024 bytes: STX, the block number (1, 2, ... wrapping past 255),
// its complement, the 1024 data bytes (the last block padded with 0x1A), and the CRC-16/XMODEM of the data (polynomial 0x1021, start 0), high byte first. The receiver answers ACK (0x06) to
// accept a packet and NAK (0x15) to ask for it again; the sender repeats a packet at most `kMaxResends` times (twenty, as the maker's update tool does), waits two seconds for each answer,
// and ends with EOT (0x04), which the receiver may NAK once before it ACKs. Two CAN (0x18) from the receiver stop the transfer.
//
// This file touches no hardware and no battery: the firmware sends what `next_tx` returns and gives `on_rx` what arrives. Once a packet has gone out the transfer is never abandoned
// by this code on its own account except when the receiver gives up, the resends are used up, or the receiver stays silent: the caller must not cancel it in between.
// Nothing here has run against a real battery.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace armor::solar::xmodem {

constexpr std::uint8_t kSoh = 0x01, kStx = 0x02, kEot = 0x04, kAck = 0x06, kNak = 0x15, kCan = 0x18, kPad = 0x1A, kCrcRequest = 'C';
constexpr std::size_t kBlock = 1024;
constexpr int kMaxResends = 20;
constexpr std::uint64_t kStartWaitMs = 5'000, kPacketWaitMs = 2'000, kFinishWaitMs = 5'000;

inline std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
  std::uint16_t crc = 0;
  for (std::size_t i = 0; i < size; ++i) {
    crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(data[i]) << 8));
    for (int bit = 0; bit < 8; ++bit) crc = static_cast<std::uint16_t>((crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1));
  }
  return crc;
}

// One packet for block `number` (the first is 1) of an image: STX, number, complement, 1024 bytes, CRC.
inline std::vector<std::uint8_t> packet(const std::vector<std::uint8_t>& image, std::size_t index) {
  std::vector<std::uint8_t> out;
  out.reserve(kBlock + 5);
  const std::uint8_t number = static_cast<std::uint8_t>((index + 1) & 0xFF);
  out.push_back(kStx);
  out.push_back(number);
  out.push_back(static_cast<std::uint8_t>(~number));
  const std::size_t begin = index * kBlock;
  for (std::size_t i = 0; i < kBlock; ++i) out.push_back(begin + i < image.size() ? image[begin + i] : kPad);
  const std::uint16_t crc = crc16(out.data() + 3, kBlock);
  out.push_back(static_cast<std::uint8_t>(crc >> 8));
  out.push_back(static_cast<std::uint8_t>(crc & 0xFF));
  return out;
}

inline std::size_t packet_count(std::size_t image_size) { return (image_size + kBlock - 1) / kBlock; }

// The sender. Built over an image that outlives it.
class Sender {
 public:
  enum class State { kWaitStart, kSending, kWaitEot, kDone, kFailed };

  explicit Sender(const std::vector<std::uint8_t>& image) : image_(image), total_(packet_count(image.size())) {}

  State state() const { return state_; }
  bool done() const { return state_ == State::kDone; }
  bool failed() const { return state_ == State::kFailed; }
  const std::string& error() const { return error_; }
  std::size_t sent() const { return next_ < total_ ? next_ : total_; }   // packets the receiver has accepted
  std::size_t total() const { return total_; }
  int resends() const { return resends_total_; }
  // True from the moment the first packet is on the wire: from then on the transfer must run to its end.
  bool committed() const { return first_sent_; }

  // Call before the first use: from now the receiver has `kStartWaitMs` to ask for CRC mode.
  void begin(std::uint64_t now_ms) { state_ = State::kWaitStart; mark_ms_ = now_ms; if (total_ == 0) fail("empty_image"); }

  // What to send now (may be empty). Packets are sent once per answer.
  std::vector<std::uint8_t> next_tx(std::uint64_t now_ms) {
    if (state_ == State::kFailed || state_ == State::kDone) return {};
    if (state_ == State::kWaitStart) {
      if (now_ms - mark_ms_ > kStartWaitMs) fail("no_crc_request");
      return {};
    }
    if (!pending_) {
      if (now_ms - mark_ms_ > (state_ == State::kWaitEot ? kPacketWaitMs : kPacketWaitMs)) return resend(now_ms);
      return {};
    }
    pending_ = false;
    mark_ms_ = now_ms;
    if (state_ == State::kWaitEot) return {kEot};
    first_sent_ = true;
    return packet(image_, next_);
  }

  void on_rx(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    for (std::size_t i = 0; i < length; ++i) {
      if (state_ == State::kDone || state_ == State::kFailed) return;
      const std::uint8_t c = data[i];
      if (c == kCan) { if (++cancels_ >= 2) { fail("receiver_cancelled"); return; } continue; }
      cancels_ = 0;
      switch (state_) {
        case State::kWaitStart:
          if (c == kCrcRequest) { state_ = State::kSending; pending_ = true; mark_ms_ = now_ms; }
          else if (c == kNak) fail("checksum_mode_requested");   // the receiver wants the old checksum: this sender only does CRC, and nothing has been sent yet
          break;
        case State::kSending:
          if (pending_) break;
          if (c == kAck) { resend_count_ = 0; ++next_; if (next_ >= total_) { state_ = State::kWaitEot; } pending_ = true; mark_ms_ = now_ms; }
          else if (c == kNak) bump(now_ms);
          // 'C' (a late request) and anything else are ignored
          break;
        case State::kWaitEot:
          if (pending_) break;
          if (c == kAck) { state_ = State::kDone; }
          else if (c == kNak) bump(now_ms);
          break;
        default: break;
      }
    }
  }

 private:
  void bump(std::uint64_t now_ms) {
    if (++resend_count_ > kMaxResends) { fail("resends_used_up"); return; }
    ++resends_total_;
    pending_ = true;
    mark_ms_ = now_ms;
  }
  std::vector<std::uint8_t> resend(std::uint64_t now_ms) {
    if (++resend_count_ > kMaxResends) { fail("receiver_silent"); return {}; }
    ++resends_total_;
    pending_ = true;
    mark_ms_ = now_ms;
    return next_tx(now_ms);
  }
  void fail(const char* why) { state_ = State::kFailed; error_ = why; }

  const std::vector<std::uint8_t>& image_;
  std::size_t total_ = 0, next_ = 0;
  State state_ = State::kWaitStart;
  bool pending_ = false, first_sent_ = false;
  int resend_count_ = 0, resends_total_ = 0, cancels_ = 0;
  std::uint64_t mark_ms_ = 0;
  std::string error_;
};

}  // namespace armor::solar::xmodem
