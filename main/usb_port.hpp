// ARMOR-NODE-SOLAR - an inverter on the chip's native USB, as host: one more way for a port to reach its equipment (core/usb_hid_link.hpp).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "uart_ports.hpp"

namespace armor::usbhost {

// Starts the USB host (once) and gives the port that talks to the HID device on it. `vid`/`pid` 0 take any HID device. nullptr with the reason in `error` ("driver"; "pins" on a board without it).
// From this call on the native USB socket is no longer a console or a way to flash, until a restart.
std::unique_ptr<ports::Port> open(std::uint16_t vid, std::uint16_t pid, std::string& error);
bool connected();             // a HID device was taken and is still there
std::string device_id();      // "0665:5161" or ""
std::string diagnosis();      // the lines of core/usb_hid_link.hpp

}  // namespace armor::usbhost
