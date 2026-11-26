// ARMOR-NODE-SOLAR - keeps the data of chosen tasks in the external memory (PSRAM). Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The sdkconfig sends every request of up to 4096 bytes to the internal memory first. The readings of a battery stack are hundreds of small strings and vectors (tens of kilobytes for each
// port with eight batteries), so three or four ports used up the internal memory. A task listed here gets its C++ allocations (new, std::string, std::vector) from the external memory,
// and from the internal one only when there is no external memory. malloc, the drivers, lwIP and FreeRTOS are not touched.
//
// ONLY for data that no interrupt reads: a port task calls keep_data_in_psram() AFTER its serial port is open (the emulated port's interrupts read the port object).
#pragma once

namespace armor::memory {

// From now on this task's C++ data goes to the external memory. For tasks that never end.
void keep_data_in_psram();

// The same for one block of code (a request of the web server that builds a long answer).
class ExternalScope {
 public:
  ExternalScope();
  ~ExternalScope();
  ExternalScope(const ExternalScope&) = delete;
  ExternalScope& operator=(const ExternalScope&) = delete;

 private:
  bool added_ = false;
};

}  // namespace armor::memory
