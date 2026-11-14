// ARMOR-NODE-SOLAR - the capture of the exchanges with the inverters and batteries: the last requests a port sent and the answers it heard, kept in a small ring in memory and handed to the installer as a
// text file. Pure: tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Off by default (capture.enabled). While it is off nothing is kept and no memory is held. When it is on the ring holds the last `frames` entries (8 to 128, 48 by default); the oldest one goes when a new one
// comes, and a counter says how many went. An entry is the bytes one port sent (a request) or heard (consecutive pieces of one answer are joined, up to 256 bytes an entry).
//
// What goes in: only the traffic of the polling of the port (the requests of the poller and what comes back). The console of a battery, the maintenance jobs and the writes to a battery run on other paths
// that never reach the capture; as a second guard the capture itself refuses any piece of bytes that has the word "login" or "passw" in it (in any case), counts it, and keeps nothing of it.
// What comes out: one line per entry, "FRAME p<port> <tx|rx> <hexadecimal>", oldest first.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace armor::framecap {

constexpr std::size_t kEntryBytes = 256;
constexpr int kMinFrames = 8;
constexpr int kMaxFrames = 128;
constexpr int kDefaultFrames = 48;
constexpr unsigned kMaxPort = 16;

inline int clamp_frames(int frames) { return frames < kMinFrames ? kMinFrames : (frames > kMaxFrames ? kMaxFrames : frames); }

// Does this piece of bytes look like part of a sign-in or of a password? (Letters of any case.)
inline bool looks_secret(const std::uint8_t* bytes, std::size_t length) {
  static const char* const words[] = {"login", "passw"};
  for (const char* word : words) {
    std::size_t size = 0;
    while (word[size] != 0) ++size;
    for (std::size_t at = 0; at + size <= length; ++at) {
      std::size_t n = 0;
      while (n < size) {
        char c = static_cast<char>(bytes[at + n]);
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != word[n]) break;
        ++n;
      }
      if (n == size) return true;
    }
  }
  return false;
}

class Capture {
 public:
  // Turns the capture on or off and sets how many entries it keeps. Turning it off, or changing the size, forgets what was kept.
  void configure(bool enabled, int frames) {
    const std::size_t wanted = enabled ? static_cast<std::size_t>(clamp_frames(frames)) : 0;
    if (wanted == slots_.size()) return;
    slots_.clear();
    slots_.shrink_to_fit();
    slots_.resize(wanted);
    next_ = 0;
    count_ = 0;
    dropped_ = 0;
    refused_ = 0;
  }
  bool enabled() const { return !slots_.empty(); }
  std::size_t capacity() const { return slots_.size(); }
  std::size_t size() const { return count_; }
  std::uint32_t dropped() const { return dropped_; }
  std::uint32_t refused() const { return refused_; }

  // Keeps the bytes one port (1 to 16) sent (`tx`) or heard. Returns false when the capture is off, the port is not valid, there are no bytes, or they look like a sign-in.
  bool push(unsigned port, bool tx, const std::uint8_t* bytes, std::size_t length) {
    if (slots_.empty() || port < 1 || port > kMaxPort || bytes == nullptr || length == 0) return false;
    if (looks_secret(bytes, length)) { ++refused_; return false; }
    std::size_t done = 0;
    if (!tx && count_ > 0) {   // the pieces of one answer go on the same entry
      Slot& last = slots_[(next_ + slots_.size() - 1) % slots_.size()];
      if (!last.tx && last.port == port && last.bytes.size() < kEntryBytes) {
        const std::size_t room = kEntryBytes - last.bytes.size();
        const std::size_t take = length < room ? length : room;
        last.bytes.append(reinterpret_cast<const char*>(bytes), take);
        done = take;
      }
    }
    while (done < length) {
      const std::size_t take = (length - done) < kEntryBytes ? (length - done) : kEntryBytes;
      Slot& slot = slots_[next_];
      slot.port = static_cast<std::uint8_t>(port);
      slot.tx = tx;
      slot.bytes.assign(reinterpret_cast<const char*>(bytes) + done, take);
      next_ = (next_ + 1) % slots_.size();
      if (count_ < slots_.size()) ++count_; else ++dropped_;
      done += take;
    }
    return true;
  }

  void clear() { next_ = 0; count_ = 0; dropped_ = 0; refused_ = 0; }

  // The kept entries, oldest first, one "FRAME p<port> <tx|rx> <hex>" line each.
  std::string text() const {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    const std::size_t start = count_ < slots_.size() ? 0 : next_;
    for (std::size_t n = 0; n < count_; ++n) {
      const Slot& slot = slots_[(start + n) % slots_.size()];
      out += "FRAME p";
      out += std::to_string(static_cast<unsigned>(slot.port));
      out += slot.tx ? " tx " : " rx ";
      for (char c : slot.bytes) {
        const std::uint8_t b = static_cast<std::uint8_t>(c);
        out += digits[b >> 4];
        out += digits[b & 15];
      }
      out += '\n';
    }
    return out;
  }

 private:
  struct Slot {
    std::uint8_t port = 0;
    bool tx = false;
    std::string bytes;
  };
  std::vector<Slot> slots_;
  std::size_t next_ = 0, count_ = 0;
  std::uint32_t dropped_ = 0, refused_ = 0;
};

}  // namespace armor::framecap
