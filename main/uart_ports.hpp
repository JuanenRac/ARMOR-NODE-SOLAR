// ARMOR-NODE-SOLAR - the node's serial ports: three hardware UARTs and seven emulated ones, behind one small interface.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace armor::ports {

struct Options {
  std::size_t index = 0;   // the port's number, 0 to 9: 0 to 2 hardware (UART1, UART2, UART0), 3 to 9 emulated
  int baud = 2400;
  int rx = -1, tx = -1;
  int de = -1;             // driver-enable (and receiver-enable) of an RS485 transceiver: high while the node sends
  bool invert = false;     // the signals arrive upside down (a hardware UART flips them)
};

// One open port. 8 data bits, no parity, one stop bit (the probe of a port tries others for a while).
class Port {
 public:
  virtual ~Port() = default;
  // Waits up to `wait_ms` for bytes and returns how many came (0 when none did).
  virtual std::size_t read(std::uint8_t* out, std::size_t capacity, unsigned wait_ms) = 0;
  // Sends the bytes and returns when they are on the line. False when the port could not send them.
  virtual bool write(const std::uint8_t* data, std::size_t length) = 0;
  virtual bool soft() const = 0;
  // How many edges or bytes the port had to drop because nobody read them in time (a busy node).
  virtual std::uint32_t overruns() const = 0;
  virtual std::uint32_t framing_errors() const = 0;
  // Changes the speed and the polarity of the line. An emulated port takes only its slow speeds and never inverted. False when the port cannot.
  virtual bool set_line(int baud, bool invert) { (void)baud; (void)invert; return false; }
  // Even parity or two stop bits instead of 8N1 (a hardware UART only). False when the port cannot.
  virtual bool set_format(bool even_parity, bool two_stop_bits) { return !even_parity && !two_stop_bits; }
  // Drops whatever the port holds unread.
  virtual void discard() {}
};

// Opens a port, or returns nullptr with the reason in `error` ("busy", "driver", "pins", "baud").
std::unique_ptr<Port> open(const Options& options, std::string& error);

}  // namespace armor::ports
