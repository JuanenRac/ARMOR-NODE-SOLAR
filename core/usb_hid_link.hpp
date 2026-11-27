// ARMOR-NODE-SOLAR - the byte line of an inverter's USB port: a HID device that carries the same serial protocol in reports of 8 bytes.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The inverters of the Voltronic family with a USB socket usually hold a USB-HID to serial adapter (the Cypress one answers as 0665:5161, the pair the Network UPS Tools project documents). A question goes
// out as output reports of 8 bytes, the last one filled with zeros; the answer comes back in input reports of 8 bytes, with zeros after its CR. This file only cuts and joins those reports: it knows
// nothing of the USB stack (main/usb_port.cpp does), so all of it is tested on a computer. Nothing here has been connected to an inverter.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

namespace armor::usbhid {

constexpr std::size_t kReportSize = 8;
constexpr std::size_t kBufferLimit = 512;   // what is held while nobody reads: the oldest bytes go first

// What the equipment on the port says it is, for the diagnosis.
struct DeviceInfo {
  bool present = false;   // something is plugged in
  bool hid = false;       // and it is a HID device that was taken
  std::uint16_t vid = 0, pid = 0;
  std::uint8_t device_class = 0, interface_class = 0, interface_subclass = 0, interface_protocol = 0;
};

struct Counters {
  std::uint32_t connects = 0, disconnects = 0, reports_out = 0, reports_in = 0, send_failures = 0, receive_errors = 0, bytes_out = 0, bytes_in = 0, dropped = 0;
};

// "" (any device) or four hexadecimal digits.
inline bool parse_id(const std::string& text, std::uint16_t& out) {
  out = 0;
  if (text.empty()) return true;
  if (text.size() != 4) return false;
  unsigned value = 0;
  for (const char c : text) {
    unsigned digit = 0;
    if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
    else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a') + 10;
    else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A') + 10;
    else return false;
    value = value * 16 + digit;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

// Whether a device is the one that was asked for: 0 takes any.
inline bool wanted(std::uint16_t want_vid, std::uint16_t want_pid, std::uint16_t vid, std::uint16_t pid) {
  return (want_vid == 0 || want_vid == vid) && (want_pid == 0 || want_pid == pid);
}

class Link {
 public:
  Counters counters;

  void connected(const DeviceInfo& info) {
    info_ = info;
    in_.clear();
    ++counters.connects;
  }
  // Something is plugged in that has not been taken (yet): only for the diagnosis. Nothing changes once a HID device was taken.
  void seen(const DeviceInfo& info) { if (!ready()) info_ = info; }
  void disconnected() {
    if (info_.present) ++counters.disconnects;
    info_ = DeviceInfo();
    in_.clear();   // half an answer of a device that is gone is never joined to the next one
  }
  const DeviceInfo& info() const { return info_; }
  bool ready() const { return info_.present && info_.hid; }

  // Cuts `length` bytes into reports and hands each to `send(const std::uint8_t*, std::size_t)`, which says whether it went. False without a device, or at the first report that did not go.
  using Send = std::function<bool(const std::uint8_t*, std::size_t)>;
  bool transmit(const std::uint8_t* data, std::size_t length, const Send& send) {
    if (!ready()) return false;
    in_.clear();   // a question starts a new exchange
    for (std::size_t from = 0; from < length; from += kReportSize) {
      std::uint8_t report[kReportSize] = {};
      const std::size_t n = std::min(kReportSize, length - from);
      std::copy(data + from, data + from + n, report);
      if (!send(static_cast<const std::uint8_t*>(report), kReportSize)) { ++counters.send_failures; return false; }
      ++counters.reports_out;
      counters.bytes_out += static_cast<std::uint32_t>(n);
    }
    return true;
  }

  // One input report. What follows the CR in it is filling and is dropped, and so is a report of zeros alone (an idle adapter). A zero before the CR stays: a CRC byte may be zero.
  void on_report(const std::uint8_t* data, std::size_t length) {
    if (!ready() || data == nullptr || length == 0) return;
    ++counters.reports_in;
    if (std::all_of(data, data + length, [](std::uint8_t b) { return b == 0; })) return;
    const std::uint8_t* end = std::find(data, data + length, static_cast<std::uint8_t>('\r'));
    const std::size_t n = end == data + length ? length : static_cast<std::size_t>(end - data) + 1;
    in_.append(reinterpret_cast<const char*>(data), n);
    counters.bytes_in += static_cast<std::uint32_t>(n);
    if (in_.size() > kBufferLimit) {
      counters.dropped += static_cast<std::uint32_t>(in_.size() - kBufferLimit);
      in_.erase(0, in_.size() - kBufferLimit);
    }
  }
  void on_receive_error() { ++counters.receive_errors; }

  // Takes up to `capacity` bytes of what has arrived.
  std::size_t read(std::uint8_t* out, std::size_t capacity) {
    const std::size_t n = std::min(capacity, in_.size());
    std::copy(in_.begin(), in_.begin() + static_cast<std::ptrdiff_t>(n), out);
    in_.erase(0, n);
    return n;
  }
  std::size_t waiting() const { return in_.size(); }
  void discard() { in_.clear(); }

  // "0665:5161", or "" when nothing is plugged in.
  std::string id_text() const {
    if (!info_.present) return std::string();
    char text[16];
    std::snprintf(text, sizeof text, "%04x:%04x", static_cast<unsigned>(info_.vid), static_cast<unsigned>(info_.pid));
    return text;
  }

  // The lines the owner copies when the inverter does not answer.
  std::string diagnosis() const {
    char text[320];
    std::snprintf(text, sizeof text,
                  "USB device=%s vid:pid=%s class=%02x interface=%02x/%02x/%02x hid=%s\n"
                  "USB connects=%u disconnects=%u reports_out=%u reports_in=%u bytes_out=%u bytes_in=%u send_failures=%u receive_errors=%u dropped=%u\n",
                  info_.present ? "yes" : "no", info_.present ? id_text().c_str() : "-", static_cast<unsigned>(info_.device_class), static_cast<unsigned>(info_.interface_class),
                  static_cast<unsigned>(info_.interface_subclass), static_cast<unsigned>(info_.interface_protocol), info_.hid ? "yes" : "no", static_cast<unsigned>(counters.connects),
                  static_cast<unsigned>(counters.disconnects), static_cast<unsigned>(counters.reports_out), static_cast<unsigned>(counters.reports_in), static_cast<unsigned>(counters.bytes_out),
                  static_cast<unsigned>(counters.bytes_in), static_cast<unsigned>(counters.send_failures), static_cast<unsigned>(counters.receive_errors), static_cast<unsigned>(counters.dropped));
    return text;
  }

 private:
  DeviceInfo info_;
  std::string in_;
};

}  // namespace armor::usbhid
