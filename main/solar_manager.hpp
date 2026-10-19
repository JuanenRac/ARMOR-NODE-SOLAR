// ARMOR-SOLAR - the ports at work: one task per enabled port opens it, asks its equipment (core/poller.hpp) and hands every whole reading to the broker link.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/battery_ops.hpp"
#include "core/battery_runner.hpp"
#include "core/solar_config.hpp"

namespace armor::manager {

// A job on a Pylontech battery's console (core/battery_ops.hpp), run by the port's own task: the port's readings wait while it runs. The plan is the checked one; the password is the node's
// own setting. `on_done` is called by the port's task when the job ends, `on_progress` now and then while it runs.
struct BatteryJob {
  solar::battery::Plan plan;
  std::string secret;
  std::shared_ptr<const std::vector<std::uint8_t>> image;   // a firmware update's image, staged beforehand
  std::atomic<bool> cancel{false};          // asked to stop (honoured between two steps, and never in the middle of a sequence that must not be cut)
  std::atomic<bool> committed{false};       // the port's task says: from now on it cannot be cancelled
  std::function<void(const solar::battery::RunResult&)> on_done;
  std::function<void(const solar::battery::RunResult&)> on_progress;
};

// What the port knows about its battery from its last reading, for the preconditions of a write.
struct BatteryPortFacts {
  bool pylontech = false;
  bool present = false;
  bool critical_alarm = false;
  long current_ma = 0;
};
BatteryPortFacts battery_port_facts(std::size_t index);
// Hands a job to the port (0 to 9). Returns "" when it was accepted, or why not: no_port, not_pylontech, busy.
std::string start_battery_job(std::size_t index, std::shared_ptr<BatteryJob> job);
// Asks the job running on the port to stop. False when there is none or it can no longer be cancelled.
bool cancel_battery_job(std::size_t index);

using Publish = std::function<void(const std::string& topic, const std::string& payload)>;

// Starts a task for every enabled port (in the mux profile, one for every group that has an enabled port: the ports of a group take turns on its UART). `publish` gets each reading (topic, JSON).
void start(const config::Settings& settings, Publish publish);

struct PortStatus {
  int number = 0;             // 1 to 10
  bool enabled = false;
  bool soft = false;
  std::string kind, name;
  int baud = 0, rx = -1, tx = -1, de = -1, poll_s = 0;
  int group = -1;             // the mux profile: the group of ports this one belongs to (0 to 2: A, B, C), or -1 when the port has pins of its own
  int channel = 0;            // ... and its channel on the group's multiplexer
  bool invert = false;        // the port's line is flipped
  std::string state;          // disabled, starting, error, waiting, reporting, silent, garbled, listening
  std::string error;          // why the port did not open, or the last error of an exchange
  std::string detail;   // what the equipment said besides the readings (an ANT-BMS: the protocol and its MOSFET codes)
  std::uint32_t bytes_rx = 0, bytes_tx = 0, replies_ok = 0, replies_bad = 0, timeouts = 0, readings = 0, overruns = 0, framing_errors = 0;
  std::string last_payload;   // the last whole reading (JSON), or ""
};
PortStatus status(std::size_t index);

// Reading the settings of an ANT-BMS (its model, its version, its protection and balancing values): READ ONLY, one operation at a time per port, run by the port's own task.
struct BmsSettingsStatus {
  std::string state = "idle";   // idle, running, done, error
  std::string error;             // when state is error: silent
  std::size_t step = 0, total = 0;
  std::string result;            // when state is done: the JSON of ant::SettingsReader::result_json()
};
// Starts it on port `index` (0 to 9). Returns "" when it started, or why not: no_port, not_ant, protocol_unknown (the BMS has not answered yet), old_protocol (this BMS
// has no settings request here) or busy.
std::string start_bms_settings(std::size_t index);
BmsSettingsStatus bms_settings(std::size_t index);

// Asking a Pylontech battery's console one question that only reads (core/console_probe.hpp), from the panel: what the battery answers is shown as it came. One at a time per port,
// run by the port's own task, which pauses its readings meanwhile. Returns "" when it started, or why not: no_port, not_pylontech, not_allowed, busy.
struct ConsoleStatus {
  std::string state = "idle";   // idle, running, done, error
  std::string command;          // what was asked, as it went on the wire
  std::string text;             // what the battery answered (what came, when it did not finish)
  std::string error;            // when state is error: timeout
  bool truncated = false;
};
std::string start_console(std::size_t index, const std::string& command);
ConsoleStatus console_status(std::size_t index);

// The last bytes a port received, as hexadecimal lines with the printable characters beside them.
std::string raw_dump(std::size_t index);

// The last reading of every port that has one, as a JSON array of the messages the node publishes.
std::string readings_json();

}  // namespace armor::manager
