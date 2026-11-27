// ARMOR-NODE-SOLAR - the rules of writing to a battery from the node's own panel: who may ask, how often, the record that is kept of it, and the campaign that updates a stack one module at
// a time. The writing itself is the same as for the orders of the broker (battery_ops.hpp, main/battery_maintenance.cpp). It touches no hardware (tests/test_battery_panel.cpp).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "battery_info.hpp"
#include "battery_ops.hpp"
#include "json.hpp"

namespace armor::solar::battery::panel {

// ---- who may ask -----------------------------------------------------------------------------------------------------------------------

struct Gate {
  bool admin = false;
  bool https = false;       // the request came over TLS
  bool enabled = false;     // battery.write_enabled, as the node started
  bool password = false;    // the battery's service password is set
  bool working = true;      // the node is not paused or in standby by the server's order
};

// "" when a request of the panel to write to a battery may go on, else why not. The password of the person is asked on top of this.
inline std::string gate_refusal(const Gate& g) {
  if (!g.admin) return "forbidden";
  if (!g.https) return "https_required";
  if (!g.enabled) return "battery_write_disabled";
  if (!g.password) return "no_password";
  if (!g.working) return "node_paused";
  return "";
}

// No more than this many orders to batteries in a minute, from anybody (the same figure as the server's).
class OrderLimit {
 public:
  static constexpr unsigned kPerMinute = 20;
  static constexpr std::uint64_t kWindowMs = 60'000;
  bool allow(std::uint64_t now_ms) {
    if (count_ == 0 || now_ms - since_ms_ >= kWindowMs) { since_ms_ = now_ms; count_ = 0; }
    if (count_ >= kPerMinute) return false;
    ++count_;
    return true;
  }

 private:
  std::uint64_t since_ms_ = 0;
  unsigned count_ = 0;
};

// The version an image states, as the battery prints it after the "V" (major at 0x401, minor at 0x400). "" for something too short to be an image.
inline std::string image_version(const std::vector<std::uint8_t>& image) {
  if (image.size() < kImageMinBytes) return "";
  return std::to_string(static_cast<unsigned>(image[kImageVersionOffset + 1])) + "." + std::to_string(static_cast<unsigned>(image[kImageVersionOffset]));
}
inline std::string image_board(const std::vector<std::uint8_t>& image) {
  std::string board;
  if (image.size() < kImageMinBytes) return board;
  for (std::size_t i = 0; i < kImageBoardSize; ++i) {
    const std::uint8_t c = image[kImageBoardOffset + i];
    if (c == 0) break;
    if (c < 0x21 || c > 0x7E) return "";
    board += static_cast<char>(c);
  }
  return board;
}

// ---- the record of who did what --------------------------------------------------------------------------------------------------------

struct AuditEntry {
  std::uint64_t at_s = 0;   // seconds of the clock; 0 while the node does not know the time
  std::string user;         // the person of the panel, or "server" for an order of the broker
  int port = 0;
  std::string event;        // armed, refused, started, ended, withdrawn, image, campaign
  std::string op;
  std::string detail;       // the refusal or the result and its code
  std::string serial;
};

// The newest entries, small enough to live in one blob of the flash. The service password is never given to it.
class Audit {
 public:
  static constexpr std::size_t kCapacity = 24;
  static constexpr std::size_t kMaxBytes = 3900;

  void add(AuditEntry e) {
    clip(e.user, 32); clip(e.event, 12); clip(e.op, 24); clip(e.detail, 48); clip(e.serial, 24);
    entries_.push_back(std::move(e));
    while (entries_.size() > kCapacity) entries_.erase(entries_.begin());
    while (entries_.size() > 1 && to_json().size() > kMaxBytes) entries_.erase(entries_.begin());
  }
  const std::vector<AuditEntry>& entries() const { return entries_; }

  std::string to_json() const {
    json::Writer w;
    w.begin_array();
    for (const AuditEntry& e : entries_) {
      w.begin_object().field("at", static_cast<long long>(e.at_s)).field("user", e.user).field("port", e.port).field("event", e.event).field("op", e.op).field("detail", e.detail).field("serial", e.serial).end_object();
    }
    w.end_array();
    return w.str();
  }
  // Anything that is not what to_json writes leaves the record empty.
  void from_json(std::string_view text) {
    entries_.clear();
    json::Value doc;
    if (!json::parse(text, doc) || !doc.is_array()) return;
    for (const json::Value& item : doc.items) {
      if (!item.is_object() || entries_.size() >= kCapacity) continue;
      AuditEntry e;
      e.at_s = static_cast<std::uint64_t>(item.integer_or("at", 0, 0, 4102444800LL));
      e.user = item.string_or("user", ""); e.port = static_cast<int>(item.integer_or("port", 0, 0, 10)); e.event = item.string_or("event", "");
      e.op = item.string_or("op", ""); e.detail = item.string_or("detail", ""); e.serial = item.string_or("serial", "");
      entries_.push_back(std::move(e));
    }
  }

 private:
  static void clip(std::string& text, std::size_t most) { if (text.size() > most) text.resize(most); }
  std::vector<AuditEntry> entries_;
};

// ---- the campaign: a stack, one module at a time, with the cable moved ------------------------------------------------------------------
// The same rules as the server's (ARMOR-SERVER src/battery_campaign.ts). It only keeps the list and the order: every step is an ordinary firmware update with all its locks. A module that
// fails, a doubtful chip or an image that does not fit stops it; nothing forces it.

struct Module {
  int number = 0;
  std::string serial, model, board, chip;   // chip: older, newer or unknown
  std::string status = "pending";           // pending, checking, ready, updating, done, failed
  std::string code;
  bool resume = false;
};

struct Image {
  std::string board, version;
  std::size_t size = 0;
  std::uint32_t crc32 = 0;
};

// The board name the image of a module must carry, or "" when there is no rule (then nothing may be sent).
inline std::string expected_board(std::string_view model, std::string_view chip, std::string_view image_version_text) {
  const bool c_family = model == "US2000C" || model == "US3000C" || model == "UP5000";
  if ((!c_family && model != "US5000") || (chip != "older" && chip != "newer")) return "";
  if (model == "US5000") {
    if (chip == "older") return "US5000";
    long major = 0;
    for (char c : image_version_text) { if (c < '0' || c > '9') break; major = major * 10 + (c - '0'); }
    return major < 2 ? "E32000C_NF4" : "US2000C_NF4";
  }
  return chip == "newer" ? "US2000C_NF4" : "US2000C";
}

// Why a module cannot be in a campaign with this image, or "".
inline std::string module_problem(const Module& m, const Image& image) {
  if (!valid_serial(m.serial)) return "serial_unreadable";
  if (m.model != "US2000C" && m.model != "US3000C" && m.model != "UP5000" && m.model != "US5000") return "unsupported_model";
  const std::string wanted = expected_board(m.model, m.chip, image.version);
  if (wanted.empty()) return "chip_unknown";
  return wanted == image.board ? "" : "image_mismatch";
}

// The modules of a battery from the reading the node publishes (core/solar_json.hpp). The model of a module, when it does not say its own, is the stack's.
inline std::vector<Module> modules_from_reading(std::string_view reading) {
  std::vector<Module> out;
  json::Value doc;
  if (!json::parse(reading, doc) || !doc.is_object() || doc.string_or("kind", "") != "battery") return out;
  const std::string model = doc.string_or("model", "");
  const json::Value* stack = doc.get("stack");
  if (stack == nullptr || !stack->is_array()) return out;
  for (const json::Value& item : stack->items) {
    if (!item.is_object() || !item.bool_or("present", false) || out.size() >= 16) continue;
    Module m;
    m.number = static_cast<int>(item.integer_or("n", 0, 0, 64));
    m.serial = item.string_or("serial", ""); m.model = item.string_or("model", model); m.board = item.string_or("board", ""); m.chip = item.string_or("chip", "unknown");
    out.push_back(std::move(m));
  }
  return out;
}

class Campaign {
 public:
  std::string state;   // "" (none), active, stopped, return_cable, done, cancelled
  int port = 0;
  std::size_t current = 0;
  std::string stop_reason, user;
  Image image;
  std::vector<Module> modules;

  bool open() const { return state == "active" || state == "stopped" || state == "return_cable"; }
  Module* module() { return current < modules.size() ? &modules[current] : nullptr; }

  // "" when it was started; else why not, and `problem_module` says which module blocks it (0: the whole list).
  std::string create(int at_port, std::string who, std::vector<Module> list, const Image& with, int& problem_module) {
    problem_module = 0;
    if (open()) return "campaign_open";
    if (at_port < 1 || at_port > 10) return "invalid_request";
    if (with.size < kImageMinBytes || with.board.empty()) return "image_missing";
    if (list.empty()) return "no_battery";
    for (std::size_t i = 0; i < list.size(); ++i) {
      const std::string problem = module_problem(list[i], with);
      if (!problem.empty()) { problem_module = list[i].number; return problem; }
      for (std::size_t k = 0; k < i; ++k) if (list[k].serial == list[i].serial) { problem_module = list[i].number; return "serial_repeated"; }
    }
    state = "active"; port = at_port; current = 0; stop_reason.clear(); user = std::move(who); image = with; modules = std::move(list);
    for (Module& m : modules) { m.status = "pending"; m.code.clear(); m.resume = false; }
    return "";
  }

  bool same_image(std::size_t size, std::uint32_t crc) const { return size == image.size && crc == image.crc32; }

  // The cable is on the current module: it may be checked (an arm).
  bool can_check() { Module* m = module(); return state == "active" && m != nullptr && (m->status == "pending" || m->status == "ready"); }
  void checking() { if (Module* m = module()) { m->status = "checking"; m->code.clear(); } }

  // What the arm of the current module answered.
  void armed(const Identity& seen) {
    Module* m = module();
    if (state != "active" || m == nullptr || m->status != "checking") return;
    if (!m->resume && (seen.serial != m->serial || (!seen.model.empty() && seen.model != m->model) || (!seen.board.empty() && !m->board.empty() && seen.board != m->board))) { stop("module_differs"); return; }
    m->status = "ready";
  }
  // `why`: the code when the refusal carries one (bootloader, nothing_to_resume), else the refusal.
  void refused(const std::string& why) {
    Module* m = module();
    if (state != "active" || m == nullptr || (m->status != "checking" && m->status != "ready" && m->status != "updating")) return;
    const bool stops = m->status == "updating" || why == "bootloader" || why == "nothing_to_resume" || why == "chip_unknown" || why == "image_mismatch" || why == "unsupported_model" ||
                       why == "unknown_model" || why == "unknown_firmware" || why == "serial_unreadable";
    if (stops) stop(why);
    else { m->status = "pending"; m->code = why; }   // nothing has been written: the module waits to be checked again
  }
  // The arm was withdrawn or ran out of time: nothing has been written.
  void lapsed(const std::string& why) {
    Module* m = module();
    if (state != "active" || m == nullptr || (m->status != "checking" && m->status != "ready")) return;
    m->status = "pending";
    m->code = why;
  }
  // Whether the module may be written now, with the serial number typed.
  std::string can_execute(const std::string& typed) {
    Module* m = module();
    if (state != "active" || m == nullptr || m->status != "ready") return "campaign_state";
    return typed == m->serial ? "" : "serial_mismatch";
  }
  void updating() { if (Module* m = module()) m->status = "updating"; }
  // The end of the update of the current module.
  void ended(const std::string& outcome, const std::string& code) {
    Module* m = module();
    if (state != "active" || m == nullptr || m->status != "updating") return;
    if (outcome != "ok") { stop(code.empty() ? outcome : code); return; }
    m->status = "done"; m->code.clear(); m->resume = false;
    if (++current >= modules.size()) state = "return_cable";
  }

  std::string retry() {
    Module* m = module();
    if (state != "stopped" || m == nullptr) return "campaign_state";
    // a transfer that did not end well leaves the battery in its bootloader: only the same image is tried again there
    if (m->code.rfind("firmware:", 0) == 0 || m->code == "bootloader") m->resume = true;
    else if (m->code == "nothing_to_resume") m->resume = false;
    m->status = "pending"; state = "active"; stop_reason.clear();
    return "";
  }
  std::string cancel() {
    if (!open()) return "campaign_state";
    const Module* m = module();
    if (m != nullptr && m->status == "updating") return "busy";   // a module that is being written is not cut
    state = "cancelled";
    return "";
  }
  // The cable is back on the first battery of the stack.
  std::string finish() {
    if (state != "return_cable") return "campaign_state";
    state = "done";
    return "";
  }
  // The node started again: whatever was under way is over.
  void restarted() {
    Module* m = module();
    if (state != "active" || m == nullptr) return;
    if (m->status == "updating") stop("operation_lost");
    else if (m->status == "checking" || m->status == "ready") { m->status = "pending"; m->code = "operation_lost"; }
  }

  std::string to_json() const {
    if (state.empty()) return "null";
    json::Writer w;
    w.begin_object().field("state", state).field("port", port).field("current", static_cast<unsigned>(current)).field("stop_reason", stop_reason).field("user", user);
    w.key("image").begin_object().field("board", image.board).field("version", image.version).field("size", static_cast<long long>(image.size)).field("crc32", static_cast<long long>(image.crc32)).end_object();
    w.key("modules").begin_array();
    for (const Module& m : modules) {
      w.begin_object().field("n", m.number).field("serial", m.serial).field("model", m.model).field("board", m.board).field("chip", m.chip).field("status", m.status).field("code", m.code).field("resume", m.resume).end_object();
    }
    w.end_array().end_object();
    return w.str();
  }
  bool from_json(std::string_view text) {
    *this = Campaign();
    json::Value doc;
    if (!json::parse(text, doc) || !doc.is_object()) return false;
    const json::Value* im = doc.get("image");
    const json::Value* list = doc.get("modules");
    if (im == nullptr || !im->is_object() || list == nullptr || !list->is_array() || list->items.size() > 16) return false;
    state = doc.string_or("state", ""); port = static_cast<int>(doc.integer_or("port", 0, 0, 10)); current = static_cast<std::size_t>(doc.integer_or("current", 0, 0, 16));
    stop_reason = doc.string_or("stop_reason", ""); user = doc.string_or("user", "");
    image.board = im->string_or("board", ""); image.version = im->string_or("version", "");
    image.size = static_cast<std::size_t>(im->integer_or("size", 0, 0, 10'000'000)); image.crc32 = static_cast<std::uint32_t>(im->integer_or("crc32", 0, 0, 4294967295LL));
    for (const json::Value& item : list->items) {
      Module m;
      m.number = static_cast<int>(item.integer_or("n", 0, 0, 64)); m.serial = item.string_or("serial", ""); m.model = item.string_or("model", ""); m.board = item.string_or("board", "");
      m.chip = item.string_or("chip", "unknown"); m.status = item.string_or("status", "pending"); m.code = item.string_or("code", ""); m.resume = item.bool_or("resume", false);
      modules.push_back(std::move(m));
    }
    if (port < 1 || modules.empty() || (state != "active" && state != "stopped" && state != "return_cable" && state != "done" && state != "cancelled")) { *this = Campaign(); return false; }
    return true;
  }

 private:
  void stop(const std::string& why) {
    if (Module* m = module()) { m->status = "failed"; m->code = why; }
    state = "stopped";
    stop_reason = why;
  }
};

}  // namespace armor::solar::battery::panel
