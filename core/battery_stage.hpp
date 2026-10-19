// ARMOR-SOLAR - the place where a firmware image waits on the node until it is sent to a battery: it arrives in small pieces (an MQTT message is small), each checked, and the whole is accepted
// only when its size and CRC-32 are what the sender said. It touches no hardware and decides nothing about whether an image fits a battery (battery_info.hpp does).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "battery_info.hpp"

namespace armor::solar::battery {

// Strict base64 (the standard alphabet, padding required, no whitespace). False on anything else.
inline bool base64_decode(std::string_view text, std::vector<std::uint8_t>& out) {
  out.clear();
  if (text.empty() || text.size() % 4 != 0) return false;
  const auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  for (std::size_t i = 0; i < text.size(); i += 4) {
    const bool last = i + 4 == text.size();
    int v[4];
    int pad = 0;
    for (int k = 0; k < 4; ++k) {
      const char c = text[i + static_cast<std::size_t>(k)];
      if (c == '=') {
        if (!last || k < 2) return false;
        ++pad;
        v[k] = 0;
      } else {
        if (pad > 0) return false;   // data after padding
        v[k] = value(c);
        if (v[k] < 0) return false;
      }
    }
    const std::uint32_t n = (static_cast<std::uint32_t>(v[0]) << 18) | (static_cast<std::uint32_t>(v[1]) << 12) | (static_cast<std::uint32_t>(v[2]) << 6) | static_cast<std::uint32_t>(v[3]);
    out.push_back(static_cast<std::uint8_t>(n >> 16));
    if (pad < 2) out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
    if (pad < 1) out.push_back(static_cast<std::uint8_t>(n & 0xFF));
  }
  return true;
}

class ImageStage {
 public:
  static constexpr std::size_t kMaxChunkBytes = 256;
  static constexpr std::size_t kMaxChunks = 2048;
  static constexpr std::uint64_t kKeepMs = 30 * 60'000;

  bool ready() const { return ready_; }
  std::size_t received() const { return buffer_ ? buffer_->size() : 0; }
  std::size_t size() const { return size_; }
  std::uint32_t crc32() const { return crc_; }
  const std::string& id() const { return id_; }
  std::shared_ptr<const std::vector<std::uint8_t>> image() const { return ready_ ? buffer_ : nullptr; }

  void clear() { buffer_.reset(); ready_ = false; id_.clear(); size_ = 0; next_ = 0; total_ = 0; }

  // Drops an image that waited too long.
  void expire(std::uint64_t now_ms) { if (buffer_ && now_ms - touched_ms_ > kKeepMs) clear(); }

  // One piece. Piece 0 starts a new image (and drops any other). "" when accepted, else why not: stage_invalid, stage_order, stage_size, stage_data, stage_crc.
  std::string add(const std::string& id, std::size_t index, std::size_t total, std::size_t size, std::uint32_t crc, std::string_view data_b64, std::uint64_t now_ms) {
    if (id.size() < 8 || id.size() > 32 || total == 0 || total > kMaxChunks || size < kImageMinBytes || size > kImageMaxBytes || index >= total) return "stage_invalid";
    if (total * kMaxChunkBytes < size || (total - 1) * kMaxChunkBytes >= size) return "stage_invalid";
    if (index == 0) {
      clear();
      id_ = id; size_ = size; crc_ = crc; total_ = total; next_ = 0;
      buffer_ = std::make_shared<std::vector<std::uint8_t>>();
      buffer_->reserve(size);
    } else if (!buffer_ || id != id_ || size != size_ || crc != crc_ || total != total_) {
      return "stage_order";
    }
    if (index != next_) return "stage_order";
    std::vector<std::uint8_t> bytes;
    if (!base64_decode(data_b64, bytes) || bytes.empty() || bytes.size() > kMaxChunkBytes) return "stage_data";
    const bool last = index + 1 == total;
    if (!last && bytes.size() != kMaxChunkBytes) return "stage_data";
    if (buffer_->size() + bytes.size() > size_) return "stage_size";
    buffer_->insert(buffer_->end(), bytes.begin(), bytes.end());
    ++next_;
    touched_ms_ = now_ms;
    if (last) {
      if (buffer_->size() != size_) { clear(); return "stage_size"; }
      if (battery::crc32(buffer_->data(), buffer_->size()) != crc_) { clear(); return "stage_crc"; }
      ready_ = true;
    }
    return "";
  }

 private:
  std::shared_ptr<std::vector<std::uint8_t>> buffer_;
  std::string id_;
  std::size_t size_ = 0, next_ = 0, total_ = 0;
  std::uint32_t crc_ = 0;
  bool ready_ = false;
  std::uint64_t touched_ms_ = 0;
};

}  // namespace armor::solar::battery
