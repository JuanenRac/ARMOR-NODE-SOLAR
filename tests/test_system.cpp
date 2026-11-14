// ARMOR-NODE-SOLAR - host tests: the summary of the whole system, the alerts the node raises on its own, their LEDs, their settings, and the poller against a simulated RS-485 stack.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing here has run on a board or on a real battery: the stack is tests/pylontech_rs485_sim.hpp.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/poller.hpp"
#include "../core/port_leds.hpp"
#include "../core/solar_config.hpp"
#include "../core/solar_system.hpp"
#include "pylontech_rs485_sim.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace armor;
using namespace armor::solar;

static bool has_text(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
static bool has_alert(const std::vector<Alert>& alerts, const std::string& code, const std::string& device) {
  for (const Alert& a : alerts) if (a.code == code && a.device == device) return true;
  return false;
}
static config::SummarySettings on(bool publish = true, bool alerts = true) {
  config::SummarySettings s;
  s.publish = publish;
  s.alerts = alerts;
  return s;
}
// a battery stack of one module with the numbers given
static std::vector<pylontech::Module> stack(int soc, double cell_high_v = 3.34, double temp_max_c = 25.0, double temp_min_c = 20.0) {
  pylontech::Module m;
  m.number = 1;
  m.present = true;
  m.voltage_v = 49.8;
  m.current_a = -1.2;
  m.temperature_c = 22.0;
  m.temperature_low_c = temp_min_c;
  m.temperature_high_c = temp_max_c;
  m.cell_low_v = 3.3;
  m.cell_high_v = cell_high_v;
  m.soc_percent = soc;
  m.base_state = "Dischg";
  return {m};
}

// The Poller against the simulated line: the bytes it writes go to the stack, what the stack says comes back `chunk` bytes at a time. Returns the messages it produced.
static std::vector<std::string> drive(Poller& poller, sim::PylontechRs485& bus, std::uint64_t from_ms, std::uint64_t to_ms, SystemSummary* summary = nullptr, std::size_t port = 0) {
  std::vector<std::string> messages;
  for (std::uint64_t t = from_ms; t < to_ms; t += 40) {
    const std::vector<std::uint8_t> tx = poller.next_tx(t);
    if (!tx.empty()) bus.feed(tx);
    const std::vector<std::uint8_t> rx = bus.read();
    if (!rx.empty()) poller.on_rx(rx.data(), rx.size(), t);
    std::string topic, payload;
    if (poller.take_message(t, topic, payload)) {
      messages.push_back(payload);
      if (summary != nullptr) summary->note_battery(port, poller.device(), poller.modules(), t);
    }
  }
  return messages;
}

// ---- the totals -----------------------------------------------------------------------------------------------------------------------

static void test_totals_and_message() {
  SystemSummary s;
  s.configure(on());
  CHECK(system_topic("solar-1") == "armor/solar/solar-1/system" && system_topic("Bad Name").empty() && system_topic("").empty());
  // nothing has answered yet: an empty summary says so, with no numbers that were not read
  std::string empty = s.json("solar-1", 5000, 5000, {});
  CHECK(has_text(empty, "\"kind\":\"system\"") && has_text(empty, "\"inverters\":0") && has_text(empty, "\"batteries\":0") && has_text(empty, "\"alerts\":[]"));
  CHECK(!has_text(empty, "pv_w") && !has_text(empty, "load_w") && !has_text(empty, "soc_percent") && !has_text(empty, "balance_w"));
  CHECK(!s.due(5000));   // nothing to say yet
  s.note_inverter(0, "axpert-1", 856, 119, 1000);
  s.note_inverter(1, "axpert-2", 400, 300, 1000);
  s.note_battery(2, "us3000-1", stack(88), 1000);
  s.note_battery(3, "us3000-2", stack(87), 1000);
  s.note_battery(4, "us3000-empty", {}, 1000);   // answered but no module: not a battery that answered
  const SystemSummary::Totals t = s.totals(2000);
  CHECK(t.inverters == 2 && t.batteries == 2 && t.pv_w == 1256 && t.load_w == 419 && t.has_soc && t.soc_percent == 87.5);
  const std::string m = s.json("solar-1", 7000, 2000, {});
  CHECK(has_text(m, "\"timestamp_ms\":7000") && has_text(m, "\"inverters\":2") && has_text(m, "\"batteries\":2") && has_text(m, "\"pv_w\":1256") && has_text(m, "\"load_w\":419"));
  CHECK(has_text(m, "\"balance_w\":837") && has_text(m, "\"soc_percent\":87.5"));
  // the balance is negative when the house takes more than the panels give
  s.note_inverter(0, "axpert-1", 10, 900, 1500);
  s.note_inverter(1, "axpert-2", 0, 100, 1500);
  CHECK(has_text(s.json("solar-1", 1, 2000, {}), "\"balance_w\":-990"));
  // a reading older than the limit is not counted; forgetting a port takes its last reading out
  CHECK(s.totals(1500 + SystemSummary::kStaleMs + 1).inverters == 0 && s.totals(1500 + SystemSummary::kStaleMs).inverters == 2);
  s.forget(0);
  CHECK(s.totals(2000).inverters == 1);
  // a number beyond what the contract holds is held at its limit
  SystemSummary big;
  big.configure(on());
  big.note_inverter(0, "x", 5e6, 0, 0);
  CHECK(has_text(big.json("n", 0, 0, {}), "\"pv_w\":1000000"));
  // the port number is bounded
  big.note_inverter(99, "x", 1, 1, 0);
  CHECK(big.totals(0).inverters == 1);
}

// ---- when the message goes out -----------------------------------------------------------------------------------------------------------

static void test_due() {
  SystemSummary s;
  config::SummarySettings off;   // the defaults: everything off
  s.configure(off);
  s.note_inverter(0, "a", 1, 1, 100);
  CHECK(!off.publish && !off.alerts && !s.due(100000));   // off until the owner turns it on, whatever was read
  config::SummarySettings st = on(true, false);
  st.every_s = 10;
  s.configure(st);
  CHECK(s.due(100));
  s.mark_sent(100);
  CHECK(!s.due(100 + 9999) && s.due(100 + 10000));   // the next one goes out after `every_s`, while the reading is still fresh
  s.note_inverter(0, "a", 1, 1, 10100);
  CHECK(s.due(10100));
  // once everything has gone stale there is nothing to say
  CHECK(!s.due(10100 + SystemSummary::kStaleMs + 1000));
}

// ---- the alerts ------------------------------------------------------------------------------------------------------------------------

static void test_alerts_off_by_default() {
  SystemSummary s;
  s.configure(config::SummarySettings());   // the defaults
  s.note_battery(0, "us3000-1", stack(3, 3.9, 80.0, -10.0), 0);   // everything wrong
  CHECK(s.evaluate(0).empty());
  // turning the alerts off clears what was on
  s.configure(on(true, true));
  CHECK(s.evaluate(0).size() == 4);
  s.configure(on(true, false));
  CHECK(s.evaluate(0).empty());
  s.configure(on(true, true));
  CHECK(s.evaluate(0).size() == 4);
}

static void test_soc_alert_and_hysteresis() {
  SystemSummary s;
  config::SummarySettings st = on(true, true);
  st.soc_low_percent = 20;
  s.configure(st);
  s.note_battery(0, "us3000-1", stack(40), 0);
  CHECK(s.evaluate(0).empty());
  s.note_battery(0, "us3000-1", stack(20), 1000);
  const std::vector<Alert> a = s.evaluate(1000);
  CHECK(a.size() == 1 && has_alert(a, "soc_low", "us3000-1") && a[0].port == 0);
  s.note_battery(0, "us3000-1", stack(21), 2000);   // still inside the margin: stays on
  CHECK(has_alert(s.evaluate(2000), "soc_low", "us3000-1"));
  s.note_battery(0, "us3000-1", stack(22), 3000);   // cleared
  CHECK(s.evaluate(3000).empty());
  s.note_battery(0, "us3000-1", stack(21), 4000);   // does not come back until the limit is reached again
  CHECK(s.evaluate(4000).empty());
  // the message carries the alert
  s.note_battery(0, "us3000-1", stack(10), 5000);
  const std::vector<Alert> raised = s.evaluate(5000);
  const std::string m = s.json("solar-1", 1, 5000, raised);
  CHECK(has_text(m, "\"alerts\":[{\"code\":\"soc_low\",\"device\":\"us3000-1\"}]"));
  // a battery that stopped answering raises nothing from its old numbers
  CHECK(s.evaluate(5000 + SystemSummary::kStaleMs + 1).empty());
}

static void test_cell_and_temperature_alerts() {
  SystemSummary s;
  config::SummarySettings st = on(true, true);
  st.cell_over_mv = 3650;
  st.temperature_high_c = 50;
  st.temperature_low_c = 0;
  s.configure(st);
  s.note_battery(1, "b", stack(80, 3.649, 49.0, 1.0), 0);
  CHECK(s.evaluate(0).empty());
  s.note_battery(1, "b", stack(80, 3.650, 50.0, 0.0), 1000);
  const std::vector<Alert> a = s.evaluate(1000);
  CHECK(a.size() == 3 && has_alert(a, "cell_over_voltage", "b") && has_alert(a, "temperature_high", "b") && has_alert(a, "temperature_low", "b") && a[0].port == 1);
  // the margins: 20 mV for a cell, 2 degrees for a temperature
  s.note_battery(1, "b", stack(80, 3.640, 48.5, 1.5), 2000);
  CHECK(s.evaluate(2000).size() == 3);
  s.note_battery(1, "b", stack(80, 3.630, 47.9, 2.0), 3000);
  CHECK(s.evaluate(3000).empty());
  // a device with a name the contract would not accept raises no alert (the message would be refused)
  s.note_battery(2, "Bad Name", stack(1), 4000);
  CHECK(s.evaluate(4000).empty());
}

static void test_alert_list_is_capped() {
  SystemSummary s;
  s.configure(on(true, true));
  for (std::size_t port = 0; port < SystemSummary::kMaxPorts; ++port) s.note_battery(port, "bat" + std::to_string(port), stack(1, 3.9, 80.0, -20.0), 0);
  const std::vector<Alert> a = s.evaluate(0);
  CHECK(a.size() == SystemSummary::kMaxAlerts);   // 16 ports x 4 alerts = 64, the contract holds 16
}

// ---- the LEDs ---------------------------------------------------------------------------------------------------------------------------

static void test_led_alert() {
  PortLeds leds;
  leds.set_enabled(0, true);
  leds.set_enabled(1, false);
  CHECK(leds.pattern(0) == 0 && !leds.alerting(0));
  leds.alert(0, true);
  leds.alert(1, true);
  CHECK(leds.alerting(0) && leds.alerting(1));
  CHECK((leds.pattern(0) & 1u) == 1u && (leds.pattern(499) & 1u) == 1u);          // half a second on ...
  CHECK((leds.pattern(500) & 1u) == 0u && (leds.pattern(999) & 1u) == 0u);        // ... half a second off
  CHECK((leds.pattern(1000) & 1u) == 1u);
  CHECK((leds.pattern(0) & 2u) == 0u);                                            // a port that is switched off never lights
  // a good reading's short pulse shows during the dark half, and the alert comes back after it
  leds.good(0, 600);
  CHECK((leds.pattern(650) & 1u) == 1u && (leds.pattern(750) & 1u) == 0u);
  // a failing port blinks fast, a different rhythm from the alert's slow one
  PortLeds failing;
  failing.set_enabled(0, true);
  failing.bad(0, 0);
  CHECK((failing.pattern(0) & 1u) == 1u && (failing.pattern(100) & 1u) == 0u && (failing.pattern(200) & 1u) == 1u);
  leds.alert(0, false);
  CHECK(!leds.alerting(0) && (leds.pattern(1000) & 1u) == 0u);   // at 1000 ms the alert would have been lit
  leds.alert(99, true);   // out of range: nothing happens
  CHECK(!leds.alerting(99));
}

// ---- the settings --------------------------------------------------------------------------------------------------------------------------

static config::Settings valid_settings() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true;
  s.sta.ssid = "casa";
  s.sta.password = "una-clave-larga";
  return s;
}
static bool has(const config::Problems& problems, const std::string& path, const std::string& code) {
  for (const config::Problem& p : problems) if (p.path == path && p.code == code) return true;
  return false;
}

static void test_settings() {
  const config::Settings base = valid_settings();
  // all off by default, and the defaults valid
  CHECK(!base.summary.publish && !base.summary.alerts && base.summary.every_s == 10 && base.summary.soc_low_percent == 20 && base.summary.cell_over_mv == 3650);
  CHECK(base.summary.temperature_high_c == 50 && base.summary.temperature_low_c == 0 && config::validate(base).empty());
  // the stored document carries the block, and reading it back gives the same numbers
  const std::string stored = config::to_json(base, true);
  CHECK(has_text(stored, "\"summary\":{\"publish\":false,\"alerts\":false,\"every_s\":10,\"soc_low_percent\":20,\"cell_over_mv\":3650,\"temperature_high_c\":50,\"temperature_low_c\":0}"));
  config::Settings edited;
  config::Problems problems;
  const char* text = "{\"summary\":{\"publish\":true,\"alerts\":true,\"every_s\":30,\"soc_low_percent\":25,\"cell_over_mv\":3600,\"temperature_high_c\":45,\"temperature_low_c\":2}}";
  CHECK(config::load(text, base, edited, problems) && problems.empty());
  CHECK(edited.summary.publish && edited.summary.alerts && edited.summary.every_s == 30 && edited.summary.soc_low_percent == 25 && edited.summary.cell_over_mv == 3600);
  CHECK(edited.summary.temperature_high_c == 45 && edited.summary.temperature_low_c == 2);
  config::Settings back;
  problems.clear();
  CHECK(config::load(config::to_json(edited, true), valid_settings(), back, problems) && back.summary.every_s == 30 && back.summary.temperature_low_c == 2 && back.summary.publish);
  // a member that is left out stays as it was
  problems.clear();
  config::Settings partial;
  CHECK(config::load("{\"summary\":{\"alerts\":true}}", edited, partial, problems) && partial.summary.alerts && partial.summary.every_s == 30);
  // what is out of range is refused, and nothing is applied
  const char* bad[] = {"{\"summary\":{\"every_s\":2}}", "{\"summary\":{\"every_s\":4000}}", "{\"summary\":{\"soc_low_percent\":0}}", "{\"summary\":{\"soc_low_percent\":100}}",
                       "{\"summary\":{\"cell_over_mv\":2000}}", "{\"summary\":{\"temperature_high_c\":200}}", "{\"summary\":{\"temperature_low_c\":-50}}"};
  for (const char* document : bad) {
    config::Problems p;
    config::Settings out;
    CHECK(!config::load(document, base, out, p) && !p.empty());
  }
  // the low limit must stay under the high one
  problems.clear();
  CHECK(!config::load("{\"summary\":{\"temperature_low_c\":50}}", base, edited, problems) && has(problems, "summary.temperature_low_c", "range"));
  problems.clear();
  CHECK(!config::load("{\"summary\":{\"publish\":\"yes\"}}", base, edited, problems) && !problems.empty());
}

// ---- the poller against the simulated RS-485 stack -----------------------------------------------------------------------------------------

static void test_rs485_healthy_stack() {
  sim::PylontechRs485 bus;
  bus.modules = {sim::Module(), sim::Module()};
  bus.modules[1].soc = 86;
  Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
  SystemSummary summary;
  summary.configure(on());
  const std::vector<std::string> messages = drive(poller, bus, 0, 20000, &summary);
  CHECK(!messages.empty() && has_text(messages[0], "\"kind\":\"battery\"") && has_text(messages[0], "\"modules\":2") && has_text(messages[0], "\"soc_percent\":87"));
  CHECK(bus.commands_seen >= 3);   // pwr, bat 1, bat 2 (the identity and history commands are asked too and answered "unknown")
  CHECK(poller.modules().size() == 2 && poller.state(20000) == "reporting");
  const SystemSummary::Totals t = summary.totals(20000);
  CHECK(t.batteries == 1 && t.has_soc && t.soc_percent == 87);
  const std::string m = summary.json("solar-1", 20000, 20000, summary.evaluate(20000));
  CHECK(has_text(m, "\"batteries\":1") && has_text(m, "\"soc_percent\":87") && has_text(m, "\"alerts\":[]"));
}

static void test_rs485_low_battery_raises_the_alert() {
  sim::PylontechRs485 bus;
  bus.modules = {sim::Module()};
  bus.modules[0].soc = 12;
  bus.modules[0].cell_high_mv = 3700;   // and a cell too high
  Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
  SystemSummary summary;
  summary.configure(on());
  drive(poller, bus, 0, 20000, &summary);
  const std::vector<Alert> a = summary.evaluate(20000);
  CHECK(has_alert(a, "soc_low", "us3000-1") && has_alert(a, "cell_over_voltage", "us3000-1") && a.size() == 2);
  PortLeds leds;
  leds.set_enabled(0, true);
  for (const Alert& alert : a) leds.alert(alert.port, true);
  CHECK(leds.alerting(0));
  // the battery recovers: the alert goes and so does the LED
  bus.modules[0].soc = 60;
  bus.modules[0].cell_high_mv = 3400;
  drive(poller, bus, 20000, 40000, &summary);
  CHECK(summary.evaluate(40000).empty());
}

static void test_rs485_faults() {
  // a stack that never answers: no reading, the port is "silent", and the summary does not count a battery
  {
    sim::PylontechRs485 bus;
    bus.modules = {sim::Module()};
    bus.silent = true;
    Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
    SystemSummary summary;
    summary.configure(on());
    CHECK(drive(poller, bus, 0, 30000, &summary).empty() && poller.state(30000) == "silent" && summary.totals(30000).batteries == 0 && !summary.due(30000));
  }
  // noise on the line: bytes arrive that are no console text; nothing is read, and nothing is invented
  {
    sim::PylontechRs485 bus;
    bus.modules = {sim::Module()};
    bus.noise = true;
    Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
    SystemSummary summary;
    summary.configure(on());
    CHECK(drive(poller, bus, 0, 30000, &summary).empty() && summary.totals(30000).batteries == 0);
  }
  // a reply that comes in tiny pieces is put together
  {
    sim::PylontechRs485 bus;
    bus.modules = {sim::Module()};
    bus.chunk = 64;
    Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
    SystemSummary summary;
    summary.configure(on());
    const std::vector<std::string> m = drive(poller, bus, 0, 40000, &summary);
    CHECK(!m.empty() && has_text(m[0], "\"soc_percent\":88") && summary.totals(40000).batteries == 1);
  }
  // a line cut in the middle of the reply: the exchange times out, nothing is published from half a table, and the next round after the line is back works
  {
    sim::PylontechRs485 bus;
    bus.modules = {sim::Module()};
    bus.cut_after = 120;
    Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
    SystemSummary summary;
    summary.configure(on());
    CHECK(drive(poller, bus, 0, 20000, &summary).empty() && summary.totals(20000).batteries == 0);
    bus.cut_after = 0;
    const std::vector<std::string> m = drive(poller, bus, 20000, 60000, &summary);
    CHECK(!m.empty() && summary.totals(60000).batteries == 1);
  }
  // a module that disappears from the stack: the message counts the ones that answer
  {
    sim::PylontechRs485 bus;
    bus.modules = {sim::Module(), sim::Module()};
    bus.modules[1].present = false;
    Poller poller(config::Kind::kPylontech, "solar-1", "us3000-1", 2, 4);
    const std::vector<std::string> m = drive(poller, bus, 0, 20000);
    CHECK(!m.empty() && has_text(m[0], "\"present\":false") && has_text(m[0], "\"modules\":1"));
  }
}

int main() {
  test_totals_and_message();
  test_due();
  test_alerts_off_by_default();
  test_soc_alert_and_hysteresis();
  test_cell_and_temperature_alerts();
  test_alert_list_is_capped();
  test_led_alert();
  test_settings();
  test_rs485_healthy_stack();
  test_rs485_low_battery_raises_the_alert();
  test_rs485_faults();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
