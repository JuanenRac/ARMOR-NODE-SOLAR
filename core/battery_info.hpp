// ARMOR-SOLAR - what a Pylontech battery says about itself (the console's `info` text), which chip it has, and whether a firmware image fits it.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The identity comes from `info`: "Device name", "Board" (present only on the batteries with the newer chip: NF4.E2 or NF4.E3), "Soft  version", "Barcode" (the serial number), "Specification"
// ("48V/74AH"). Which chip a battery has decides which firmware image it may receive, and the wrong image destroys it. The rules, from the maker's own update tool and notes:
//   - US2000C, US3000C and UP5000: the 8th and 9th characters of the serial number are E2 or C3 on the newer chip (the battery's Board is then NF4.E2); anything else is the older chip.
//   - US5000: the 8th and 9th characters are C5 on the newer chip (Board NF4.E3).
//   - The serial number and the Board that the battery reports must agree; when they do not, or the serial cannot be read, nothing is decided and nothing is sent.
// An image carries its own identity: two bytes at offset 0x400 (the first is the minor version, the next the major version) and the name of the board it is for, 11 bytes at offset 0x418,
// padded with zeros. The image that fits is chosen from the model, the Board and, for the US5000 with Board NF4.E3, the image version (below 2.0: E32000C_NF4; 2.0 and up: US2000C_NF4):
//     US5000, older chip -> "US5000"      US5000, NF4.E3 -> "E32000C_NF4" or "US2000C_NF4"      US2000C/US3000C/UP5000, NF4.E2 -> "US2000C_NF4"      those, older chip -> "US2000C"
// Nothing here has run against a real battery.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace armor::solar::battery {

struct Identity {
  bool valid = false;            // a Device name was read
  std::string model;             // Device name: US3000C ...
  std::string board;             // "Board" (NF4.E2 ...), "" on the batteries that do not print it
  std::string board_version;     // "Board version" (V10R04)
  std::string soft_version;      // "Soft  version" (V3.0)
  std::string main_soft_version; // "Main Soft version" (B68.8.0.0)
  std::string serial;            // "Barcode", trimmed; "" when it is blank
  std::string specification;     // 48V/74AH
  int capacity_ah = -1;          // the AH of the specification, -1 when it cannot be read
  int cells = -1;
};

namespace detail {
inline std::string trim(std::string_view text) {
  std::size_t b = 0, e = text.size();
  while (b < e && (text[b] == ' ' || text[b] == '\t' || text[b] == '\r' || text[b] == '\n')) ++b;
  while (e > b && (text[e - 1] == ' ' || text[e - 1] == '\t' || text[e - 1] == '\r' || text[e - 1] == '\n')) --e;
  return std::string(text.substr(b, e - b));
}
// The value of the line "Key : value" (first match; the key's inner runs of spaces count as one), true when the line exists.
inline bool line_value(const std::string& text, std::string_view key, std::string& value) {
  std::size_t pos = 0;
  while (pos <= text.size()) {
    std::size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    const std::size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name;
      bool gap = false;
      for (char c : trim(line.substr(0, colon))) {
        if (c == ' ' || c == '\t') { gap = true; continue; }
        if (gap && !name.empty()) name += ' ';
        gap = false;
        name += c;
      }
      if (name == key) { value = trim(line.substr(colon + 1)); return true; }
    }
    if (end >= text.size()) break;
    pos = end + 1;
  }
  return false;
}
}  // namespace detail

// Reads the text `info` printed. `valid` is false when there is no Device name.
inline Identity parse_info(const std::string& text) {
  Identity id;
  std::string v;
  if (detail::line_value(text, "Device name", v) && !v.empty()) { id.valid = true; id.model = v; }
  if (detail::line_value(text, "Board", v)) id.board = v;
  if (detail::line_value(text, "Board version", v)) id.board_version = v;
  if (detail::line_value(text, "Soft version", v)) id.soft_version = v;
  if (detail::line_value(text, "Main Soft version", v)) id.main_soft_version = v;
  if (detail::line_value(text, "Barcode", v)) id.serial = v;
  if (detail::line_value(text, "Specification", v)) {
    id.specification = v;
    const std::size_t slash = v.find('/');
    if (slash != std::string::npos) {
      int n = 0;
      std::size_t i = slash + 1;
      for (; i < v.size() && v[i] >= '0' && v[i] <= '9' && i < slash + 5; ++i) n = n * 10 + (v[i] - '0');
      if (i > slash + 1 && i < v.size() && (v[i] == 'A' || v[i] == 'a')) id.capacity_ah = n;
    }
  }
  if (detail::line_value(text, "Cell Number", v)) {
    int n = 0;
    for (char c : v) { if (c < '0' || c > '9') { n = -1; break; } n = n * 10 + (c - '0'); }
    id.cells = n;
  }
  return id;
}

// "Total Coul: 50000 mAH" (the answer to `totalcoul`): the milliampere-hours, or -1.
inline long parse_total_coul_mah(const std::string& text) {
  std::string v;
  if (!detail::line_value(text, "Total Coul", v)) return -1;
  long n = 0;
  std::size_t i = 0;
  for (; i < v.size() && v[i] >= '0' && v[i] <= '9' && i < 9; ++i) n = n * 10 + (v[i] - '0');
  return i == 0 ? -1 : n;
}

// A serial number as the maker's tool takes it: letters and digits (and - _), 10 to 24 characters.
inline bool valid_serial(std::string_view serial) {
  if (serial.size() < 10 || serial.size() > 24) return false;
  for (char c : serial) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) return false;
  return true;
}

enum class Chip { kOlder, kNewer, kUnknown };

// The chip of a battery from its model, its Board and its serial number. kUnknown whenever the three do not tell one story.
inline Chip chip_of(const Identity& id) {
  if (!id.valid || !valid_serial(id.serial)) return Chip::kUnknown;
  const std::string marker = id.serial.substr(7, 2);
  const bool c_family = id.model == "US2000C" || id.model == "US3000C" || id.model == "UP5000";
  if (c_family) {
    const bool marker_new = marker == "E2" || marker == "C3";
    if (id.board == "NF4.E2") return marker_new ? Chip::kNewer : Chip::kUnknown;
    if (id.board.empty() || id.board == "ST1") return marker_new ? Chip::kUnknown : Chip::kOlder;
    return Chip::kUnknown;
  }
  if (id.model == "US5000") {
    const bool marker_new = marker == "C5";
    if (id.board == "NF4.E3") return marker_new ? Chip::kNewer : Chip::kUnknown;
    if (id.board.empty()) return marker_new ? Chip::kUnknown : Chip::kOlder;
    return Chip::kUnknown;
  }
  return Chip::kUnknown;
}

// ---- the firmware image ------------------------------------------------------------------------------------------------------------------

constexpr std::size_t kImageVersionOffset = 0x400, kImageBoardOffset = 0x418, kImageBoardSize = 11;
constexpr std::size_t kImageMinBytes = 64 * 1024, kImageMaxBytes = 512 * 1024;

struct ImageVerdict {
  bool ok = false;
  std::string code;       // "" when ok; else image_size, image_header, unsupported_model, chip_unknown, board_mismatch
  std::string board;      // the board name found in the image
  int major = -1, minor = -1;
};

inline ImageVerdict check_image(const std::vector<std::uint8_t>& image, const Identity& id) {
  ImageVerdict v;
  if (image.size() < kImageMinBytes || image.size() > kImageMaxBytes) { v.code = "image_size"; return v; }
  v.minor = image[kImageVersionOffset];
  v.major = image[kImageVersionOffset + 1];
  for (std::size_t i = 0; i < kImageBoardSize; ++i) {
    const std::uint8_t c = image[kImageBoardOffset + i];
    if (c == 0) break;
    if (c < 0x21 || c > 0x7E) { v.code = "image_header"; return v; }
    v.board += static_cast<char>(c);
  }
  if (v.board.empty()) { v.code = "image_header"; return v; }
  const bool supported = id.model == "US2000C" || id.model == "US3000C" || id.model == "UP5000" || id.model == "US5000";
  if (!supported) { v.code = "unsupported_model"; return v; }
  const Chip chip = chip_of(id);
  if (chip == Chip::kUnknown) { v.code = "chip_unknown"; return v; }
  std::string expected;
  if (id.model == "US5000") {
    if (chip == Chip::kOlder) expected = "US5000";
    else expected = v.major < 2 ? "E32000C_NF4" : "US2000C_NF4";
  } else {
    expected = chip == Chip::kNewer ? "US2000C_NF4" : "US2000C";
  }
  if (v.board != expected) { v.code = "board_mismatch"; return v; }
  v.ok = true;
  return v;
}

// The CRC-32 of the image, for the staged copy to be compared with what the server sent.
inline std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) {
  crc = ~crc;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

}  // namespace armor::solar::battery
