// ARMOR-NODE-SOLAR - the summary of the whole system, and the alerts the node raises on its own (contract: schema solar_system of ARMOR-COMMON, see docs/SOLAR_MESSAGES.md).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
//   armor/solar/<node>/system     one short JSON message, QoS 0, not retained: the power of the panels, the load, the mean state of charge, their balance and the alerts.
//
// The node keeps the latest reading of each port; a reading older than kStaleMs is not counted (an inverter or a battery that stopped answering does not keep its last numbers in the
// totals). The alerts are decided HERE, from the settings of the node (config::SummarySettings, off by default), so that they exist, and show on the LEDs of the ports, when the server is not
// reachable. An alert has a little hysteresis (it is raised at the limit and cleared a margin inside it) so a reading that hovers at the limit does not make the LED and the message flicker.
// Nothing here has run on a board.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"
#include "pylontech.hpp"
#include "solar_config.hpp"
#include "solar_json.hpp"

namespace armor::solar {

inline std::string system_topic(const std::string& node_id) {
  if (!valid_name(node_id)) return "";
  return "armor/solar/" + node_id + "/system";
}

// An alert the node raised: a word of the contract and the device it is about. `port` is the port the device hangs on (the LED to light).
struct Alert {
  std::string code;       // soc_low, cell_over_voltage, temperature_high or temperature_low
  std::string device;
  std::size_t port = 0;
};

class SystemSummary {
 public:
  static constexpr std::size_t kMaxPorts = 16;
  static constexpr std::uint64_t kStaleMs = 60000;      // a reading this old is not counted
  static constexpr std::size_t kMaxAlerts = 16;         // the contract's limit
  // The margins that clear an alert after it was raised.
  static constexpr int kSocMarginPercent = 2;
  static constexpr int kCellMarginMv = 20;
  static constexpr int kTemperatureMarginC = 2;

  void configure(const config::SummarySettings& settings) { settings_ = settings; if (!settings_.alerts) active_.clear(); }
  const config::SummarySettings& settings() const { return settings_; }

  // An inverter answered: the power of its panels and the power it gives to the house, in watts.
  void note_inverter(std::size_t port, const std::string& device, double pv_w, double out_w, std::uint64_t now_ms) {
    if (port >= kMaxPorts) return;
    Slot& s = slots_[port];
    s = Slot{};
    s.kind = Kind::kInverter;
    s.device = device;
    s.pv_w = pv_w;
    s.out_w = out_w;
    s.at_ms = now_ms;
    s.valid = true;
  }
  // A battery stack answered (a stack with no module present is "answered, but empty": it does not count as a battery).
  void note_battery(std::size_t port, const std::string& device, const std::vector<pylontech::Module>& modules, std::uint64_t now_ms) {
    if (port >= kMaxPorts) return;
    Slot& s = slots_[port];
    s = Slot{};
    s.kind = Kind::kBattery;
    s.device = device;
    s.stack = pylontech::summarise(modules);
    s.at_ms = now_ms;
    s.valid = true;
  }
  // A port that was switched off or lost: its last reading goes.
  void forget(std::size_t port) { if (port < kMaxPorts) slots_[port] = Slot{}; }

  struct Totals {
    int inverters = 0, batteries = 0;
    double pv_w = 0, load_w = 0, soc_percent = 0;
    bool has_soc = false;
  };
  Totals totals(std::uint64_t now_ms) const {
    Totals t;
    double soc_sum = 0;
    int soc_count = 0;
    for (const Slot& s : slots_) {
      if (!fresh(s, now_ms)) continue;
      if (s.kind == Kind::kInverter) { ++t.inverters; t.pv_w += s.pv_w; t.load_w += s.out_w; }
      else if (s.stack.modules > 0) {
        ++t.batteries;
        if (s.stack.soc_percent >= 0) { soc_sum += s.stack.soc_percent; ++soc_count; }
      }
    }
    if (soc_count > 0) { t.soc_percent = soc_sum / soc_count; t.has_soc = true; }
    return t;
  }

  // Decides the alerts at `now_ms` from the latest readings and the settings, and remembers which ones are on (for the hysteresis). Empty while the alerts are off.
  std::vector<Alert> evaluate(std::uint64_t now_ms) {
    std::vector<Alert> out;
    if (!settings_.alerts) { active_.clear(); return out; }
    std::vector<std::string> still;
    for (std::size_t port = 0; port < kMaxPorts; ++port) {
      const Slot& s = slots_[port];
      if (!fresh(s, now_ms) || s.kind != Kind::kBattery || s.stack.modules <= 0 || !valid_name(s.device)) continue;
      const pylontech::Stack& k = s.stack;
      const int cell_mv = static_cast<int>(k.cell_high_v * 1000.0 + 0.5);   // whole millivolts, so a limit is compared with no rounding doubt
      const auto decide = [&](const char* code, bool breach, bool inside_margin) {
        const std::string key = std::string(code) + "/" + s.device;
        const bool was = std::find(active_.begin(), active_.end(), key) != active_.end();
        if (breach || (was && !inside_margin)) {
          still.push_back(key);
          if (out.size() < kMaxAlerts) out.push_back(Alert{code, s.device, port});
        }
      };
      if (k.soc_percent >= 0) decide("soc_low", k.soc_percent <= settings_.soc_low_percent, k.soc_percent >= settings_.soc_low_percent + kSocMarginPercent);
      if (k.cell_high_v > 0) decide("cell_over_voltage", cell_mv >= settings_.cell_over_mv, cell_mv <= settings_.cell_over_mv - kCellMarginMv);
      decide("temperature_high", k.temperature_max_c >= settings_.temperature_high_c, k.temperature_max_c <= settings_.temperature_high_c - kTemperatureMarginC);
      decide("temperature_low", k.temperature_min_c <= settings_.temperature_low_c, k.temperature_min_c >= settings_.temperature_low_c + kTemperatureMarginC);
    }
    active_ = std::move(still);
    return out;
  }

  // The message. `wall_ms` is the node's clock (or the time since it started while the clock is not set); `alerts` is what evaluate() just returned.
  std::string json(const std::string& node_id, std::uint64_t wall_ms, std::uint64_t now_ms, const std::vector<Alert>& alerts) const {
    const Totals t = totals(now_ms);
    json::Writer w;
    w.begin_object().field("kind", "system").field("node_id", node_id).key("timestamp_ms").integer(static_cast<long long>(wall_ms));
    w.field("inverters", t.inverters).field("batteries", t.batteries);
    if (t.inverters > 0) {
      const double pv = clamp(t.pv_w, 0, 1000000), load = clamp(t.load_w, 0, 1000000);
      w.key("pv_w").number(pv, 0).key("load_w").number(load, 0).key("balance_w").number(clamp(pv - load, -1000000, 1000000), 0);
    }
    if (t.has_soc) w.key("soc_percent").number(clamp(t.soc_percent, 0, 100), 1);
    w.key("alerts").begin_array();
    for (const Alert& a : alerts) w.begin_object().field("code", a.code).field("device", a.device).end_object();
    w.end_array();
    w.end_object();
    return w.str();
  }

  // Whether the message is due, and the mark that it went out. The first one waits for the first full round, so it is not sent empty.
  bool due(std::uint64_t now_ms) const {
    if (!settings_.publish) return false;
    return now_ms >= next_ms_ && (any_fresh(now_ms));
  }
  void mark_sent(std::uint64_t now_ms) { next_ms_ = now_ms + static_cast<std::uint64_t>(settings_.every_s) * 1000ULL; }

 private:
  enum class Kind { kNone, kInverter, kBattery };
  struct Slot {
    Kind kind = Kind::kNone;
    bool valid = false;
    std::string device;
    double pv_w = 0, out_w = 0;
    pylontech::Stack stack;
    std::uint64_t at_ms = 0;
  };
  static double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
  static bool fresh(const Slot& s, std::uint64_t now_ms) { return s.valid && now_ms >= s.at_ms && now_ms - s.at_ms <= kStaleMs; }
  bool any_fresh(std::uint64_t now_ms) const {
    for (const Slot& s : slots_) if (fresh(s, now_ms)) return true;
    return false;
  }

  config::SummarySettings settings_;
  std::array<Slot, kMaxPorts> slots_;
  std::vector<std::string> active_;
  std::uint64_t next_ms_ = 0;
};

}  // namespace armor::solar
