// ARMOR-NODE-SOLAR - the interface of a protocol driver: what to ask, how to read the answer, what it adds up to.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A driver touches no hardware and no clock: the poller gives it the bytes and tells it when a question was not answered, and sends the bytes it asks for. Only reading
// requests exist in any driver. The families spoken here are binary (Modbus RTU, DALY, JBD, JK) or a passive text stream (VE.Direct); the ASCII families of Voltronic stay in
// core/voltronic*.hpp. A driver holds one frame at most, so it costs a few hundred bytes of RAM.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pylontech.hpp"
#include "voltronic.hpp"

namespace armor::solar::drivers {

// What a finished cycle holds: an inverter (or solar charger) fills `status`, a battery fills `modules`.
struct Out {
  voltronic::Status status;
  char mode = 0;                       // the letters of voltronic::mode_name; 0: unknown
  std::vector<std::string> warnings;
  bool warnings_known = false;         // false: the equipment was not asked for them, or did not answer
  std::vector<pylontech::Module> modules;
};

enum class Rx {
  kMore,   // the frame is not whole yet
  kNext,   // the answer was good: `next` holds the following question
  kDone,   // the cycle is over: out() holds the reading
  kBad,    // the cycle failed: error() says why
};

class Driver {
 public:
  virtual ~Driver() = default;
  virtual bool inverter() const = 0;                       // which of the two messages the readings become
  virtual std::vector<std::uint8_t> begin() = 0;           // starts a cycle: the first question, or nothing for a stream that talks on its own
  virtual Rx feed(std::uint8_t byte, std::vector<std::uint8_t>& next) = 0;
  // The question was not answered in time: an optional one is skipped (kNext or kDone), an essential one gives the cycle up (kBad).
  virtual Rx timeout(std::vector<std::uint8_t>& next) = 0;
  const Out& out() const { return out_; }
  const char* error() const { return error_; }             // "", "crc", "nak", "format", "timeout"
  const char* command() const { return command_; }         // what is being asked, for the panel
  // Equipment of a family with several register maps: ask first which one it has, and read nothing from one that is not known here.
  virtual void set_layout_guard(bool) {}
  virtual bool layout_unsupported() const { return false; }

 protected:
  Rx bad(const char* why) { error_ = why; return Rx::kBad; }
  Out out_;
  const char* error_ = "";
  const char* command_ = "";
};

namespace detail {
inline std::uint16_t be16(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0] << 8) | p[1]); }
inline std::int16_t be16s(const std::uint8_t* p) { return static_cast<std::int16_t>(be16(p)); }
inline std::uint32_t be32(const std::uint8_t* p) { return (static_cast<std::uint32_t>(be16(p)) << 16) | be16(p + 2); }
inline double bounded(double v, double low, double high) { return v < low ? low : (v > high ? high : v); }

// The extremes of a module worked out from its cells and sensors.
inline void finish_module(pylontech::Module& m) {
  m.number = 1;
  m.present = true;
  if (!m.cells_v.empty()) {
    m.cell_low_v = m.cell_high_v = m.cells_v.front();
    for (const double v : m.cells_v) { if (v < m.cell_low_v) m.cell_low_v = v; if (v > m.cell_high_v) m.cell_high_v = v; }
  }
  if (!m.temperatures_c.empty()) {
    m.temperature_low_c = m.temperature_high_c = m.temperatures_c.front();
    for (const double t : m.temperatures_c) { if (t < m.temperature_low_c) m.temperature_low_c = t; if (t > m.temperature_high_c) m.temperature_high_c = t; }
    m.temperature_c = m.temperatures_c.front();
  } else {
    m.temperature_low_c = m.temperature_high_c = m.temperature_c;
  }
  if (m.base_state.empty()) m.base_state = m.current_a > 0.5 ? "Charge" : m.current_a < -0.5 ? "Dischg" : "Idle";
}
}  // namespace detail

}  // namespace armor::solar::drivers
