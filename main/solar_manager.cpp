// ARMOR-NODE-SOLAR - the ports at work.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One task per enabled port: it opens the port, asks its equipment whenever the poller says so, feeds the poller what arrives and hands every whole reading to
// the broker link. A port that cannot be opened says why (pins, speed, driver) and stays out of the way of the others. Nothing here has run on a board.
#include "solar_manager.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
extern "C" {
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}
#include "core/ant_settings.hpp"
#include "core/console_probe.hpp"
#include "core/mux_group.hpp"
#include "core/poller.hpp"
#include "core/port_leds.hpp"
#include "core/soft_uart.hpp"
#include "core/solar_system.hpp"
#include "mqtt_link.hpp"
#include "mux_board.hpp"
#include "port_leds_hw.hpp"
#include "psram_mem.hpp"
#include "psram_new.hpp"
#include "uart_ports.hpp"

namespace armor::manager {
namespace {
constexpr char kTag[] = "armor-ports";

struct Slot {
  std::mutex lock;
  PortStatus status;
  std::shared_ptr<const std::string> reading;   // the last whole reading (JSON)
  BatteryPortFacts facts;                       // of that reading, for the preconditions of a write
  std::string raw;
  std::atomic<std::uint64_t> raw_wanted_until_ms{0};   // the dump is only made while the panel is looking at it (see raw_dump)
  std::string node_id;
  config::PortConfig config;
  std::size_t index = 0;
  // the ANT-BMS settings operation: asked by the web server, run by the port's task
  std::atomic<bool> settings_requested{false};
  bool ant_new_protocol = false;
  BmsSettingsStatus settings;
  // the console question asked from the panel
  std::atomic<bool> console_requested{false};
  ConsoleStatus console;
  // a battery maintenance job (core/battery_ops.hpp), handed over by battery_maintenance.cpp and run by the port's task
  std::atomic<bool> battery_requested{false};
  std::shared_ptr<BatteryJob> battery_job;
  bool battery_running = false;
  // the probe of this port, and whether a task holds the port open
  std::atomic<bool> probe_requested{false};
  bool open = false;
};
std::array<Slot, config::kPortCount> g_slots;
Publish g_publish;

std::uint64_t uptime_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL; }

// ---- the summary of the whole system and the alerts of the node (core/solar_system.hpp) -----------------------------------------------------------
// Off until the settings turn on the message or the alerts: with both off nothing here takes a lock. Every reading that a port takes is noted; once a second one of the tasks decides the
// alerts again (a battery that stopped answering clears its own), lights the slow blink of the port LEDs and sends the message when it is due.
std::mutex g_summary_lock;
solar::SystemSummary g_summary;
std::string g_summary_node;
std::atomic<bool> g_summary_wanted{false};
std::atomic<std::uint64_t> g_summary_next_tick_ms{0};

void summary_note(const solar::Poller& poller, std::size_t port, std::uint64_t now_ms) {
  if (!g_summary_wanted.load()) return;
  std::lock_guard<std::mutex> guard(g_summary_lock);
  if (poller.is_inverter()) g_summary.note_inverter(port, poller.device(), poller.pv_power_w(), poller.status().out_w, now_ms);
  else if (poller.kind() != config::Kind::kRaw) g_summary.note_battery(port, poller.device(), poller.modules(), now_ms);
}

void summary_tick(std::uint64_t now_ms) {
  if (!g_summary_wanted.load() || now_ms < g_summary_next_tick_ms.load()) return;
  g_summary_next_tick_ms.store(now_ms + 1000);
  std::string payload, topic;
  std::array<bool, solar::PortLeds::kCount> lit{};
  {
    std::lock_guard<std::mutex> guard(g_summary_lock);
    const std::vector<solar::Alert> alerts = g_summary.evaluate(now_ms);
    for (const solar::Alert& alert : alerts) if (alert.port < lit.size()) lit[alert.port] = true;
    if (g_summary.due(now_ms)) {
      topic = solar::system_topic(g_summary_node);
      payload = g_summary.json(g_summary_node, mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : now_ms, now_ms, alerts);
      g_summary.mark_sent(now_ms);
    }
  }
  for (std::size_t port = 0; port < lit.size(); ++port) leds::alert(port, lit[port]);
  if (!topic.empty() && g_publish) g_publish(topic, payload);
}

// A port that hears bytes and reads none says so in the log, one port at a time: the health report carries the last error line to the server (core/port_diag.hpp).
std::mutex g_unreadable_lock;
solar::portdiag::LogSchedule g_unreadable;

void tell_unreadable(const PortStatus& s, std::uint64_t now_ms) {
  const std::size_t port = static_cast<std::size_t>(s.number - 1);
  {
    std::lock_guard<std::mutex> guard(g_unreadable_lock);
    if (s.readings > 0 && s.state == "reporting") g_unreadable.clear(port);
    if (s.cause.empty() || !g_unreadable.take(port, now_ms)) return;
  }
  ESP_LOGE(kTag, "%s", solar::portdiag::log_line(s.number, s.cause, s.baud, s.rx, s.tx, s.invert, s.bytes_rx, s.cause == "slave" ? std::string() : s.sample, s.slave_module).c_str());
}

// The capture of the exchanges of the polling: off until the settings turn it on. The flag is read first so that a node with the capture off takes no lock for it.
std::mutex g_capture_lock;
framecap::Capture g_capture;
std::atomic<bool> g_capture_on{false};
std::function<void(bool, const std::uint8_t*, std::size_t)> capture_sink(std::size_t port) {
  return [port](bool tx, const std::uint8_t* data, std::size_t length) {
    if (!g_capture_on.load()) return;
    std::lock_guard<std::mutex> guard(g_capture_lock);
    g_capture.push(static_cast<unsigned>(port), tx, data, length);
  };
}

// ---- the probe of a port (core/port_probe.hpp) ----------------------------------------------------------------------------------------------------

namespace probe = solar::probe;

// One probe at a time on the node: which port has it (-1: none) and what it has found so far.
std::atomic<int> g_probe_port{-1};
std::atomic<bool> g_probe_cancel{false};
struct ProbeShared {
  std::mutex lock;
  int port = -1;
  bool fast = false;
  probe::Progress progress;
  std::unique_ptr<probe::Log> log;   // made by the port's task, so in the external memory
  std::uint64_t started_ms = 0, ended_ms = 0;
} g_probe;

// The summary of the last probe, in the memory of the clock domain: it survives a restart of the node (not a power cut) and nothing is written to the flash.
struct ProbeKept {
  std::uint32_t magic, length, sum;
  char text[1200];
};
constexpr std::uint32_t kProbeKeptMagic = 0x50524F42;   // "PROB"
RTC_NOINIT_ATTR ProbeKept g_probe_kept;

std::uint32_t kept_sum(const char* text, std::size_t length) {
  std::uint32_t sum = 2166136261u;
  for (std::size_t i = 0; i < length; ++i) sum = (sum ^ static_cast<std::uint8_t>(text[i])) * 16777619u;
  return sum;
}
void keep_probe_summary(const std::string& text) {
  const std::size_t n = std::min(text.size(), sizeof g_probe_kept.text);
  std::memcpy(g_probe_kept.text, text.data(), n);
  g_probe_kept.length = static_cast<std::uint32_t>(n);
  g_probe_kept.sum = kept_sum(g_probe_kept.text, n);
  g_probe_kept.magic = kProbeKeptMagic;
}
// What the last start left (read once, before any probe of this start overwrites it).
const std::string& kept_before_restart() {
  static const std::string text = [] {
    const ProbeKept& k = g_probe_kept;
    if (k.magic != kProbeKeptMagic || k.length > sizeof k.text || kept_sum(k.text, k.length) != k.sum) return std::string();
    return std::string(k.text, k.length);
  }();
  return text;
}

std::string probe_stamp() {
  char out[48];
  if (mqtt_link::clock_is_set()) {
    const std::uint64_t ms = mqtt_link::wall_clock_ms();
    const std::time_t seconds = static_cast<std::time_t>(ms / 1000);
    std::tm parts{};
    gmtime_r(&seconds, &parts);
    char date[32] = {};
    std::strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", &parts);
    std::snprintf(out, sizeof out, "%s.%03uZ", date, static_cast<unsigned>(ms % 1000));
  } else {
    const std::uint64_t ms = uptime_ms();
    std::snprintf(out, sizeof out, "+%llu.%03us", static_cast<unsigned long long>(ms / 1000), static_cast<unsigned>(ms % 1000));
  }
  return out;
}

// A UART port or the line of a multiplexed group, as the probe wants them.
struct PortProbeLine {
  ports::Port& port;
  bool configure(int baud, bool invert, probe::Format f) { return port.set_line(baud, invert) && port.set_format(f == probe::Format::k8E1, f == probe::Format::k8N2); }
  void discard() { port.discard(); }
  std::size_t read(std::uint8_t* out, std::size_t capacity, unsigned wait_ms) { return port.read(out, capacity, wait_ms); }
  bool write(const std::uint8_t* data, std::size_t length) { return port.write(data, length); }
  std::uint32_t framing_errors() { return port.framing_errors(); }
};
struct MuxProbeLine {
  solar::MuxLine& line;
  bool configure(int baud, bool invert, probe::Format f) { return line.configure(baud, invert) && line.set_format(f == probe::Format::k8E1, f == probe::Format::k8N2); }
  void discard() { line.discard(); }
  std::size_t read(std::uint8_t* out, std::size_t capacity, unsigned wait_ms) { return line.read(out, capacity, wait_ms); }
  bool write(const std::uint8_t* data, std::size_t length) { return line.write(data, length); }
  std::uint32_t framing_errors() { return line.framing_errors(); }
};

// The port's task takes the probe that was asked: from here on the port says "probing".
std::unique_ptr<probe::Probe> probe_begin(Slot& slot, bool emulated, bool has_tx) {
  probe::Setup setup;
  setup.port = static_cast<int>(slot.index + 1);
  setup.emulated = emulated;
  setup.can_transmit = has_tx;
  setup.baud = config::effective_baud(slot.config);
  setup.invert = slot.config.invert && !emulated;
  setup.max_baud = emulated ? static_cast<int>(softuart::kMaxBaud) : 115200;
  setup.inverter = slot.config.kind == "voltronic" || slot.config.kind == "modbus" || slot.config.kind == "vedirect";
  setup.version = esp_app_get_description()->version;
  { std::lock_guard<std::mutex> guard(g_probe.lock); setup.fast = g_probe.fast; }
  probe::Clock clock;
  clock.uptime_ms = uptime_ms;
  clock.stamp = probe_stamp;
  auto prober = std::make_unique<probe::Probe>(std::move(setup), std::move(clock));
  auto log = std::make_unique<probe::Log>();
  log->append(prober->header());
  kept_before_restart();
  keep_probe_summary("port " + std::to_string(slot.index + 1) + ": a probe was running when the node restarted (started " + probe_stamp() + ")\n");
  ESP_LOGW(kTag, "port %u: probe started, %u steps; its readings wait", static_cast<unsigned>(slot.index + 1), static_cast<unsigned>(prober->progress().total));
  {
    std::lock_guard<std::mutex> guard(g_probe.lock);
    g_probe.log = std::move(log);
    g_probe.progress = prober->progress();
    g_probe.started_ms = uptime_ms();
    g_probe.ended_ms = 0;
  }
  std::lock_guard<std::mutex> guard(slot.lock);
  slot.status.state = "probing";
  slot.status.error.clear();
  return prober;
}

// One step (or none, when it was stopped). True when the probe is over: its summary is written and the node is free for another one.
template <class Line>
bool probe_step(Slot& slot, probe::Probe& prober, Line line) {
  if (g_probe_cancel.load()) prober.stop();
  const std::string entry = prober.step(line, &g_probe_cancel);
  const bool over = prober.finished();
  const std::string summary = over ? prober.summary() : std::string();
  {
    std::lock_guard<std::mutex> guard(g_probe.lock);
    if (g_probe.log) {
      g_probe.log->append(entry);
      if (over) g_probe.log->append(summary, true);
    }
    g_probe.progress = prober.progress();
    if (over) g_probe.ended_ms = uptime_ms();
  }
  if (!over) return false;
  keep_probe_summary(summary);
  const probe::Progress& p = prober.progress();
  ESP_LOGW(kTag, "port %u: probe %s: %s, %u of %u steps, %u combinations, %u bytes received, %u answers", static_cast<unsigned>(slot.index + 1), probe::state_name(p.state), p.verdict(),
           static_cast<unsigned>(p.step), static_cast<unsigned>(p.total), static_cast<unsigned>(p.combinations), static_cast<unsigned>(p.rx_bytes), static_cast<unsigned>(p.answers));
  for (const probe::Found& f : p.found)
    ESP_LOGW(kTag, "port %u: answered %s at %d %s %s", static_cast<unsigned>(slot.index + 1), f.query, f.baud, probe::format_name(f.format), f.inverted ? "inverted" : "normal");
  g_probe_port.store(-1);
  return true;
}

void fill(Slot& slot, const solar::Poller& poller, std::uint32_t overruns, std::uint32_t framing_errors, std::string payload) {
  const solar::PortStats& stats = poller.stats();
  std::shared_ptr<const std::string> whole;
  BatteryPortFacts facts;
  if (!payload.empty()) {
    whole = std::make_shared<const std::string>(std::move(payload));
    // the console answers for the module it is connected to (the first of the stack)
    const std::vector<solar::pylontech::Module>& modules = poller.modules();
    if (poller.kind() == config::Kind::kPylontech && !modules.empty()) {
      facts.present = modules.front().present;
      facts.current_ma = static_cast<long>(modules.front().current_a * 1000.0);
      facts.critical_alarm = solar::pylontech::summarise(modules).alarm;
    }
  }
  // The console of another battery of a stack gives no reading, but a firmware update has to be able to start on it.
  bool slave = false;
  if (poller.kind() == config::Kind::kPylontech && poller.slave_sheet().place > 0) {
    const solar::pylontech::SlaveSheet& sheet = poller.slave_sheet();
    slave = true;
    facts = BatteryPortFacts{};
    facts.present = sheet.current_known;
    facts.slave = true;
    facts.current_ma = sheet.current_ma;
    facts.critical_alarm = !sheet.quiet;
  }
  const std::uint64_t now = uptime_ms();
  const std::string cause = poller.unreadable_cause(now);
  const std::string sample = cause.empty() ? std::string() : poller.heard_sample();
  PortStatus said;
  {
    std::lock_guard<std::mutex> guard(slot.lock);
    PortStatus& s = slot.status;
    s.cause = cause;
    s.sample = sample;
    s.slave_module = poller.slave_module();
    s.state = poller.state(now);
    s.error = stats.last_error;
    s.detail = poller.detail();
    s.bytes_rx = stats.bytes_rx; s.bytes_tx = stats.bytes_tx; s.replies_ok = stats.replies_ok; s.replies_bad = stats.replies_bad; s.timeouts = stats.timeouts; s.readings = stats.readings;
    s.overruns = overruns;
    s.framing_errors = framing_errors;
    if (whole) slot.reading = std::move(whole);
    if (whole || slave) slot.facts = facts;
    slot.ant_new_protocol = poller.ant_new_protocol();
    said = s;
  }
  tell_unreadable(said, now);   // outside the slot's lock: writing the log may take its time
}

// Reads the settings of the BMS on this port: one request at a time, nothing written, the poller waits. The result is left in the slot for the web server to give.
template <class Line>   // a Port, or the MuxLine of a group
void run_bms_settings(Slot& slot, Line& port) {
  solar::ant::SettingsReader reader;
  std::uint8_t buffer[128];
  std::size_t reported = static_cast<std::size_t>(-1);
  while (!reader.done()) {
    const std::vector<std::uint8_t> request = reader.next_tx(uptime_ms());
    if (!request.empty() && !port.write(request.data(), request.size())) ESP_LOGW(kTag, "port %u: a settings request could not be sent", static_cast<unsigned>(slot.index + 1));
    const std::size_t n = port.read(buffer, sizeof buffer, 30);
    if (n > 0) reader.on_rx(buffer, n);
    if (reader.step() != reported) {
      reported = reader.step();
      std::lock_guard<std::mutex> guard(slot.lock);
      slot.settings.step = reported;
    }
  }
  std::lock_guard<std::mutex> guard(slot.lock);
  slot.settings.step = solar::ant::SettingsReader::total();
  if (reader.failed()) { slot.settings.state = "error"; slot.settings.error = reader.error(); }
  else { slot.settings.state = "done"; slot.settings.result = reader.result_json(); }
}

// Asks the console the question that was requested and waits for its answer; the port's readings wait meanwhile.
template <class Line>
void run_console(Slot& slot, Line& port) {
  solar::console::Probe probe;
  std::string command, why;
  { std::lock_guard<std::mutex> guard(slot.lock); command = slot.console.command; }
  probe.start(command, uptime_ms(), why);
  std::uint8_t buffer[256];
  while (probe.running()) {
    const std::vector<std::uint8_t> request = probe.next_tx(uptime_ms());
    if (!request.empty() && !port.write(request.data(), request.size())) ESP_LOGW(kTag, "port %u: the console question could not be sent", static_cast<unsigned>(slot.index + 1));
    const std::size_t n = port.read(buffer, sizeof buffer, 40);
    if (n > 0) probe.on_rx(buffer, n, uptime_ms());
    probe.tick(uptime_ms());
  }
  std::lock_guard<std::mutex> guard(slot.lock);
  slot.console.state = probe.state();
  slot.console.text = probe.text();
  slot.console.error = probe.error();
  slot.console.truncated = probe.truncated();
}

// Runs the battery maintenance job that was handed over, line by line, and tells how it went. The port's readings wait meanwhile (this is the port's own task). Nothing here
// has run on a board; the plan and the runner are tested on a computer against a simulated console.
template <class Line>
void run_battery(Slot& slot, Line& port) {
  std::shared_ptr<BatteryJob> job;
  { std::lock_guard<std::mutex> guard(slot.lock); job = slot.battery_job; }
  if (!job) return;
  const std::unique_ptr<solar::battery::Runner> runner = std::make_unique<solar::battery::Runner>(job->plan, job->secret, job->image ? job->image.get() : nullptr);
  runner->start(uptime_ms());
  std::uint8_t buffer[256];
  std::uint64_t last_progress_ms = uptime_ms();
  while (!runner->finished()) {
    if (job->cancel.load()) runner->cancel();
    job->committed = runner->committed();
    const std::vector<std::uint8_t> request = runner->next_tx(uptime_ms());
    if (!request.empty() && !port.write(request.data(), request.size())) ESP_LOGW(kTag, "port %u: a maintenance line could not be sent", static_cast<unsigned>(slot.index + 1));
    const std::size_t n = port.read(buffer, sizeof buffer, 20);
    if (n > 0) runner->on_rx(buffer, n, uptime_ms());
    const std::uint64_t now = uptime_ms();
    if (job->on_progress && now - last_progress_ms >= 2000) { last_progress_ms = now; job->on_progress(runner->result()); }
  }
  const solar::battery::RunResult result = runner->result();
  { std::lock_guard<std::mutex> guard(slot.lock); slot.battery_running = false; slot.battery_job.reset(); }
  if (job->on_done) job->on_done(result);
}

void port_task(void* argument) {
  Slot& slot = *static_cast<Slot*>(argument);
  const config::PortConfig& cfg = slot.config;
  config::Kind kind = config::Kind::kVoltronic;
  config::kind_from_text(cfg.kind, kind);
  ports::Options options;
  options.index = slot.index;
  options.baud = config::effective_baud(cfg);
  options.rx = cfg.rx; options.tx = cfg.tx; options.de = cfg.de; options.invert = cfg.invert;
  std::string error;
  std::unique_ptr<ports::Port> port = ports::open(options, error);
  if (!port) {
    ESP_LOGE(kTag, "port %u could not be opened (%s): it stays off", static_cast<unsigned>(slot.index + 1), error.c_str());
    std::lock_guard<std::mutex> guard(slot.lock);
    slot.status.state = "error";
    slot.status.error = error;
    vTaskDelete(nullptr);
    return;
  }
  // from here on what this task makes (the readings, their JSON) goes to the external memory; the port above, which its interrupts read, stays where it is
  memory::keep_data_in_psram();
  // on the heap: its raw log and its readings would eat the port task's small stack
  const std::unique_ptr<solar::Poller> poller_owner = std::make_unique<solar::Poller>(kind, slot.node_id, cfg.name, config::effective_poll_s(cfg), cfg.modules, cfg.dialect, cfg.cells_per_cycle);
  solar::Poller& poller = *poller_owner;
  poller.set_optional(cfg.pv2, cfg.parallel);
  poller.set_identity(true);
  poller.set_capture(capture_sink(slot.index + 1));
  std::uint8_t buffer[256];
  std::uint64_t last_fill_ms = 0, last_raw_total = 0;
  std::unique_ptr<probe::Probe> prober;
  { std::lock_guard<std::mutex> guard(slot.lock); slot.open = true; }
  for (;;) {
    if (slot.probe_requested.exchange(false)) {
      poller.abort(uptime_ms());
      prober = probe_begin(slot, port->soft(), cfg.tx >= 0);
    }
    if (prober) {
      if (probe_step(slot, *prober, PortProbeLine{*port})) {
        prober.reset();
        // the port goes back to its own settings
        port->set_format(false, false);
        port->set_line(options.baud, options.invert && !port->soft());
        port->discard();
      }
      summary_tick(uptime_ms());
      vTaskDelay(1);
      continue;
    }
    if (slot.settings_requested.exchange(false)) run_bms_settings(slot, *port);
    if (slot.console_requested.exchange(false)) run_console(slot, *port);
    if (slot.battery_requested.exchange(false)) run_battery(slot, *port);
    const std::uint64_t now = uptime_ms();
    const std::vector<std::uint8_t> request = poller.next_tx(now);
    if (!request.empty() && !port->write(request.data(), request.size())) ESP_LOGW(kTag, "port %u: the request could not be sent", static_cast<unsigned>(slot.index + 1));
    const std::size_t n = port->read(buffer, sizeof buffer, 40);
    if (n > 0) poller.on_rx(buffer, n, uptime_ms());
    std::string topic, payload;
    // The message carries the node's wall clock; while the clock is not set it carries the time since the node started, and the server stamps it with the time it receives it.
    const bool ready = poller.take_message(mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : uptime_ms(), topic, payload);
    if (ready && g_publish) g_publish(topic, payload);
    if (ready) summary_note(poller, slot.index, uptime_ms());
    summary_tick(uptime_ms());
    const std::uint64_t after = uptime_ms();
    if (ready || after - last_fill_ms >= 1000) {
      last_fill_ms = after;
      fill(slot, poller, port->overruns(), port->framing_errors(), ready ? std::move(payload) : std::string());
      if (after < slot.raw_wanted_until_ms.load() && poller.raw().total() != last_raw_total) {
        last_raw_total = poller.raw().total();
        const std::string dump = poller.diagnosis() + poller.raw().dump();
        std::lock_guard<std::mutex> guard(slot.lock);
        slot.raw = dump;
      }
    }
  }
}
// ---- the mux profile: one task per group of ports that share a UART ----------------------------------------------------------------------------------

struct GroupJob {
  std::size_t group = 0;
  config::MuxGroup pins;
  std::vector<std::size_t> ports;   // the enabled ports of the group, by index
};
std::array<GroupJob, config::kMuxGroups> g_groups;

void group_task(void* argument) {
  GroupJob& job = *static_cast<GroupJob*>(argument);
  const config::MuxGroup& pins = job.pins;
  int first_baud = 9600;
  if (!job.ports.empty()) first_baud = config::effective_baud(g_slots[job.ports.front()].config);
  std::string error;
  std::unique_ptr<solar::MuxLine> line = muxboard::open_group(job.group, pins, first_baud, error);
  if (!line) {
    ESP_LOGE(kTag, "group %c could not be opened (%s): its ports stay off", static_cast<char>('A' + job.group), error.c_str());
    for (std::size_t index : job.ports) { std::lock_guard<std::mutex> guard(g_slots[index].lock); g_slots[index].status.state = "error"; g_slots[index].status.error = error; }
    vTaskDelete(nullptr);
    return;
  }
  memory::keep_data_in_psram();   // as in port_task: after the line is open
  // on the heap: every port's raw log and readings would eat the task's small stack
  std::vector<solar::MuxMember> members;
  for (std::size_t index : job.ports) {
    Slot& slot = g_slots[index];
    const config::PortConfig& cfg = slot.config;
    config::Kind kind = config::Kind::kVoltronic;
    config::kind_from_text(cfg.kind, kind);
    solar::MuxMember member;
    member.port = index;
    member.channel = static_cast<std::size_t>(slot.status.channel);
    member.baud = config::effective_baud(cfg);
    member.invert = cfg.invert;
    member.poller = std::make_unique<solar::Poller>(kind, slot.node_id, cfg.name, config::effective_poll_s(cfg), cfg.modules, cfg.dialect, cfg.cells_per_cycle);
    member.poller->set_optional(cfg.pv2, cfg.parallel);
    member.poller->set_identity(true);
    member.poller->set_capture(capture_sink(index + 1));
    members.push_back(std::move(member));
  }
  const std::unique_ptr<solar::MuxGroup> group = std::make_unique<solar::MuxGroup>(std::move(members));
  solar::MuxClock clock;
  clock.uptime_ms = uptime_ms;
  clock.wall_clock_is_set = [] { return mqtt_link::clock_is_set(); };
  clock.wall_clock_ms = [] { return mqtt_link::wall_clock_ms(); };
  std::vector<std::uint64_t> last_fill(job.ports.size(), 0), last_raw_total(job.ports.size(), 0);
  std::uint32_t reported_failures = 0;
  std::unique_ptr<probe::Probe> prober;
  std::size_t probed = 0;   // the member whose port is being probed
  for (std::size_t index : job.ports) { std::lock_guard<std::mutex> guard(g_slots[index].lock); g_slots[index].open = true; }
  for (;;) {
    for (std::size_t k = 0; k < job.ports.size(); ++k) {
      Slot& slot = g_slots[job.ports[k]];
      if (!slot.probe_requested.exchange(false)) continue;
      probed = k;
      group->set_paused(k, true);   // the others of the group go on taking their turns between two steps of the probe
      prober = probe_begin(slot, false, pins.tx >= 0);
    }
    if (prober && (!group->has_line() || g_probe_cancel.load())) {
      Slot& slot = g_slots[job.ports[probed]];
      const bool free_line = !group->has_line();
      if (free_line) {
        group->poller(probed).abort(uptime_ms());
        line->select(group->members()[probed].channel);
      } else {
        prober->stop();   // stopped while another port has the line: nothing is sent
      }
      const bool over = probe_step(slot, *prober, MuxProbeLine{*line});
      if (free_line) { line->set_format(false, false); group->invalidate_line(); }
      if (over) { prober.reset(); group->set_paused(probed, false); }
    }
    // the reading of an ANT-BMS's settings asked in the panel is done between two turns, on that port's channel
    if (!group->has_line()) {
      for (std::size_t k = 0; k < job.ports.size(); ++k) {
        Slot& slot = g_slots[job.ports[k]];
        if (!slot.settings_requested.exchange(false)) continue;
        const solar::MuxMember& m = group->members()[k];
        line->select(m.channel);
        line->configure(m.baud, m.invert);
        line->discard();
        run_bms_settings(slot, *line);
        group->invalidate_line();
      }
      for (std::size_t k = 0; k < job.ports.size(); ++k) {
        Slot& slot = g_slots[job.ports[k]];
        if (!slot.console_requested.exchange(false)) continue;
        const solar::MuxMember& m = group->members()[k];
        line->select(m.channel);
        line->configure(m.baud, m.invert);
        line->discard();
        run_console(slot, *line);
        group->invalidate_line();
      }
      for (std::size_t k = 0; k < job.ports.size(); ++k) {
        Slot& slot = g_slots[job.ports[k]];
        if (!slot.battery_requested.exchange(false)) continue;
        const solar::MuxMember& m = group->members()[k];
        line->select(m.channel);
        line->configure(m.baud, m.invert);
        line->discard();
        run_battery(slot, *line);
        group->invalidate_line();
      }
    }
    solar::MuxTurn turn = group->step(*line, clock, 20);
    if (group->write_failures() != reported_failures) { reported_failures = group->write_failures(); ESP_LOGW(kTag, "group %c: a request could not be sent", static_cast<char>('A' + job.group)); }
    if (turn.reading && g_publish) g_publish(turn.topic, turn.payload);
    if (turn.reading) leds::good(turn.port);
    if (turn.ended && turn.failed) leds::bad(turn.port);
    if (turn.reading && turn.active) summary_note(*group->members()[turn.member].poller, turn.port, uptime_ms());
    summary_tick(uptime_ms());
    if (!turn.active) continue;
    Slot& slot = g_slots[turn.port];
    const solar::Poller& poller = *group->members()[turn.member].poller;
    const std::uint64_t after = uptime_ms();
    if (turn.reading || turn.ended || after - last_fill[turn.member] >= 1000) {
      last_fill[turn.member] = after;
      fill(slot, poller, 0, 0, turn.reading ? std::move(turn.payload) : std::string());
      if (after < slot.raw_wanted_until_ms.load() && poller.raw().total() != last_raw_total[turn.member]) {
        last_raw_total[turn.member] = poller.raw().total();
        const std::string dump = poller.diagnosis() + poller.raw().dump();
        std::lock_guard<std::mutex> guard(slot.lock);
        slot.raw = dump;
      }
    }
  }
}
}  // namespace

// The mux profile: the eight ports of the base board, their groups and the LEDs.
static void start_mux(const config::Settings& settings) {
  for (std::size_t g = 0; g < config::kMuxGroups; ++g) { g_groups[g].group = g; g_groups[g].pins = settings.mux[g]; g_groups[g].ports.clear(); }
  for (std::size_t i = 0; i < config::kPortCount; ++i) {
    Slot& slot = g_slots[i];
    const config::PortConfig& cfg = settings.ports[i];
    slot.index = i;
    slot.node_id = settings.node_id;
    slot.config = cfg;
    PortStatus& s = slot.status;
    s.number = static_cast<int>(i + 1);
    config::MuxSlot where;
    const bool in_profile = config::mux_slot_of(settings.mux, i, where);
    s.enabled = cfg.enabled && in_profile;
    s.soft = false;
    s.kind = cfg.kind; s.name = cfg.name;
    s.baud = config::effective_baud(cfg);
    s.invert = cfg.invert;
    s.poll_s = config::effective_poll_s(cfg);
    if (in_profile) { s.group = static_cast<int>(where.group); s.channel = static_cast<int>(where.channel); s.rx = settings.mux[where.group].rx; s.tx = settings.mux[where.group].tx; }
    s.state = s.enabled ? "starting" : "disabled";
    if (s.enabled) g_groups[where.group].ports.push_back(i);
  }
  leds::start(settings.leds, settings);
  for (std::size_t g = 0; g < config::kMuxGroups; ++g) {
    if (g_groups[g].ports.empty()) continue;
    xTaskCreate(group_task, (std::string("mux-") + static_cast<char>('A' + g)).c_str(), 8192, &g_groups[g], 5, nullptr);
  }
}

void start(const config::Settings& settings, Publish publish) {
  g_publish = std::move(publish);
  kept_before_restart();
  {
    std::lock_guard<std::mutex> guard(g_summary_lock);
    g_summary.configure(settings.summary);
    g_summary_node = settings.node_id;
  }
  g_summary_wanted.store(settings.summary.publish || settings.summary.alerts);
  set_capture(settings.capture_enabled, settings.capture_frames);
  if (config::profile_is_mux(settings.profile)) { start_mux(settings); return; }
  for (std::size_t i = 0; i < config::kPortCount; ++i) {
    Slot& slot = g_slots[i];
    const config::PortConfig& cfg = settings.ports[i];
    slot.index = i;
    slot.node_id = settings.node_id;
    slot.config = cfg;
    PortStatus& s = slot.status;
    s.number = static_cast<int>(i + 1);
    s.enabled = cfg.enabled;
    s.soft = config::is_soft_port(i);
    s.kind = cfg.kind; s.name = cfg.name;
    s.baud = config::effective_baud(cfg);
    s.rx = cfg.rx; s.tx = cfg.tx; s.de = cfg.de; s.poll_s = config::effective_poll_s(cfg);
    s.invert = cfg.invert;
    s.state = cfg.enabled ? "starting" : "disabled";
    if (!cfg.enabled) continue;
    xTaskCreate(port_task, ("port" + std::to_string(i + 1)).c_str(), 6144, &slot, 5, nullptr);
  }
}

std::string start_bms_settings(std::size_t index) {
  if (index >= config::kPortCount) return "no_port";
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  if (!slot.status.enabled || slot.status.kind != "ant") return "not_ant";
  if (slot.settings.state == "running" || g_probe_port.load() == static_cast<int>(index)) return "busy";
  if (!slot.ant_new_protocol) return slot.status.detail.empty() ? "protocol_unknown" : "old_protocol";
  slot.settings = BmsSettingsStatus();
  slot.settings.state = "running";
  slot.settings.total = solar::ant::SettingsReader::total();
  slot.settings_requested = true;
  return "";
}

std::string start_probe(std::size_t index, bool fast) {
  if (index >= config::kPortCount) return "no_port";
  const memory::ExternalScope in_psram;
  // the record of a probe is a few hundred kilobytes: only with external memory to spare
  if (memory::snapshot().psram_free < 1024u * 1024u) return "no_memory";
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  if (!slot.status.enabled || !slot.open) return "port_off";
  if (slot.console.state == "running" || slot.settings.state == "running" || slot.battery_running) return "busy";
  int nobody = -1;
  if (!g_probe_port.compare_exchange_strong(nobody, static_cast<int>(index))) return "busy";
  {
    std::lock_guard<std::mutex> shared(g_probe.lock);
    g_probe.port = static_cast<int>(index);
    g_probe.fast = fast;
    g_probe.progress = probe::Progress();
    g_probe.log.reset();
    g_probe.started_ms = uptime_ms();
    g_probe.ended_ms = 0;
  }
  g_probe_cancel = false;
  slot.probe_requested = true;
  return "";
}

bool stop_probe() {
  if (g_probe_port.load() < 0) return false;
  g_probe_cancel = true;
  return true;
}

ProbeStatus probe_status() {
  ProbeStatus out;
  std::lock_guard<std::mutex> guard(g_probe.lock);
  out.previous = kept_before_restart();
  if (g_probe.port < 0) return out;
  out.port = g_probe.port + 1;
  out.fast = g_probe.fast;
  out.progress = g_probe.progress;
  out.elapsed_ms = (g_probe.ended_ms != 0 ? g_probe.ended_ms : uptime_ms()) - g_probe.started_ms;
  if (g_probe.log) { out.log_size = g_probe.log->size(); out.truncated = g_probe.log->truncated(); }
  return out;
}

std::size_t probe_log_read(std::size_t from, char* out, std::size_t capacity) {
  std::lock_guard<std::mutex> guard(g_probe.lock);
  return g_probe.log ? g_probe.log->read(from, out, capacity) : 0;
}

std::string start_console(std::size_t index, const std::string& command) {
  if (index >= config::kPortCount) return "no_port";
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  if (!slot.status.enabled || slot.status.kind != "pylontech") return "not_pylontech";
  std::string canonical;
  if (!solar::console::allowed(command, canonical)) return "not_allowed";
  if (slot.console.state == "running" || g_probe_port.load() == static_cast<int>(index)) return "busy";
  slot.console = ConsoleStatus();
  slot.console.state = "running";
  slot.console.command = canonical;
  slot.console_requested = true;
  return "";
}

ConsoleStatus console_status(std::size_t index) {
  if (index >= config::kPortCount) return {};
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  return slot.console;
}

BmsSettingsStatus bms_settings(std::size_t index) {
  if (index >= config::kPortCount) return {};
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  return slot.settings;
}

PortStatus status(std::size_t index) {
  if (index >= config::kPortCount) return {};
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  return slot.status;
}

BatteryPortFacts battery_port_facts(std::size_t index) {
  BatteryPortFacts facts;
  if (index >= config::kPortCount) return facts;
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  facts = slot.facts;
  facts.pylontech = slot.status.enabled && slot.status.kind == "pylontech";
  return facts;
}

std::string start_battery_job(std::size_t index, std::shared_ptr<BatteryJob> job) {
  if (index >= config::kPortCount) return "no_port";
  if (!job) return "no_port";
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  if (!slot.status.enabled || slot.status.kind != "pylontech") return "not_pylontech";
  if (slot.battery_running || slot.battery_job || g_probe_port.load() == static_cast<int>(index)) return "busy";
  slot.battery_job = std::move(job);
  slot.battery_running = true;
  slot.battery_requested = true;
  return "";
}

bool cancel_battery_job(std::size_t index) {
  if (index >= config::kPortCount) return false;
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  if (!slot.battery_job || slot.battery_job->committed.load()) return false;
  slot.battery_job->cancel = true;
  return true;
}

void set_capture(bool enabled, int frames) {
  std::lock_guard<std::mutex> guard(g_capture_lock);
  g_capture.configure(enabled, frames);
  g_capture_on.store(g_capture.enabled());
}

std::string capture_text() { std::lock_guard<std::mutex> guard(g_capture_lock); return g_capture.text(); }

void capture_clear() { std::lock_guard<std::mutex> guard(g_capture_lock); g_capture.clear(); }

std::string capture_json() {
  json::Writer w;
  std::lock_guard<std::mutex> guard(g_capture_lock);
  w.begin_object().field("enabled", g_capture.enabled()).field("capacity", static_cast<long long>(g_capture.capacity())).field("stored", static_cast<long long>(g_capture.size()))
      .field("dropped", static_cast<long long>(g_capture.dropped())).field("refused", static_cast<long long>(g_capture.refused())).end_object();
  return w.str();
}

std::string raw_dump(std::size_t index) {
  if (index >= config::kPortCount) return "";
  Slot& slot = g_slots[index];
  // The dump (128 lines of hexadecimal) is made by the port's task only for a while after somebody asks for it: nobody is looking most of the time, and it was built every second.
  // The first answer can be an older dump; the panel asks again every few seconds.
  slot.raw_wanted_until_ms.store(uptime_ms() + 15000);
  std::lock_guard<std::mutex> guard(slot.lock);
  return slot.raw;
}

std::shared_ptr<const std::string> reading(std::size_t index) {
  if (index >= config::kPortCount) return nullptr;
  Slot& slot = g_slots[index];
  std::lock_guard<std::mutex> guard(slot.lock);
  return slot.reading;
}

}  // namespace armor::manager
