// ARMOR-NODE-SOLAR - the typed operations that WRITE to a Pylontech battery through its console: what each one is, which values it accepts, the exact lines it sends, what must be true before it
// starts and what is read back afterwards. Nothing here is a free command: an operation is built from a name and checked values, never from the person's text (except the expert console, which
// is checked word by word against a block list). It touches no hardware; the runner (battery_runner.hpp) sends what a plan says.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// WHERE THE LINES COME FROM: the maker's own tool (BatteryView) and the console's help text. The lines that were seen on a real battery in the tool's logs are marked (seen): `login`,
// `logout`, `info`, `stat`, `setlowcfg`, `eepe 0`, `trst`, `tbar <serial>`, `totalcoul <Ah>`, `totalcoul`, `ctrl mostest on`, `updata`. The others come from the help text and the tool's
// configuration files and have to be confirmed on a bench battery before they are trusted: `time`, `statc`, `logc`, `datac`, `shut`, `config`, `prot`, `ctrl cfet|dfet|buzz|heat`,
// `ctrl afuse off`. The read-back after every write tells whether a line did what it should; when it did not, the result says so ('failed') and shows the battery's real state.
//
// THE RANGES come from the tool's parameter files (the numbers they state); where a file states an order between two values (the over-voltage protection above the high-voltage alarm ...),
// that order is checked too, and a group of values is only accepted whole, so that no value is ever checked against one the battery keeps and this code has not seen.
// Nothing here has run against a real battery.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "auth.hpp"
#include "battery_info.hpp"
#include "json.hpp"

namespace armor::solar::battery {

enum class Op {
  kSetClock, kClearStats, kClearLog, kClearData, kRestart, kShutdown,
  kSocCapacity, kMosfailTest, kSocReset, kPcbSetup,
  kLimitsSet, kLimitsDefault, kProtectionsSet, kProtectionsDefault,
  kOutputSet, kFaultClear,
  kWriteBarcode, kEepromErase, kFirmwareUpdate,
  kExpertCommand,
  kPreflight,      // reads only: what the arm step does
};

enum class Risk { kLow, kMedium, kHigh, kIrreversible };

struct OpInfo {
  Op op;
  const char* name;
  Risk risk;
  bool needs_idle;      // the current must be near zero
  bool alarm_ok;        // may run while a protection is active (it is a recovery)
  bool atomic;          // once the first line is sent it is never cancelled
  bool writes;          // changes something in the battery
};

inline const std::vector<OpInfo>& catalog() {
  static const std::vector<OpInfo> table = {
      {Op::kSetClock, "set_clock", Risk::kLow, false, false, false, true},
      {Op::kClearStats, "clear_stats", Risk::kMedium, false, false, false, true},
      {Op::kClearLog, "clear_log", Risk::kMedium, false, false, false, true},
      {Op::kClearData, "clear_data", Risk::kHigh, false, false, false, true},
      {Op::kRestart, "restart", Risk::kLow, false, true, false, true},
      {Op::kShutdown, "shutdown", Risk::kMedium, false, true, false, true},
      {Op::kSocCapacity, "soc_capacity", Risk::kMedium, true, false, false, true},
      {Op::kMosfailTest, "mosfail_test", Risk::kMedium, true, false, false, true},
      {Op::kSocReset, "soc_reset", Risk::kIrreversible, true, false, true, true},
      {Op::kPcbSetup, "pcb_setup", Risk::kIrreversible, true, false, true, true},
      {Op::kLimitsSet, "limits_set", Risk::kHigh, true, false, false, true},
      {Op::kLimitsDefault, "limits_default", Risk::kHigh, true, false, false, true},
      {Op::kProtectionsSet, "protections_set", Risk::kHigh, true, false, false, true},
      {Op::kProtectionsDefault, "protections_default", Risk::kHigh, true, false, false, true},
      {Op::kOutputSet, "output_set", Risk::kMedium, true, false, false, true},
      {Op::kFaultClear, "fault_clear", Risk::kMedium, false, true, false, true},
      {Op::kWriteBarcode, "write_barcode", Risk::kIrreversible, true, false, true, true},
      {Op::kEepromErase, "eeprom_erase", Risk::kIrreversible, true, false, true, true},
      {Op::kFirmwareUpdate, "firmware_update", Risk::kIrreversible, true, false, true, true},
      {Op::kExpertCommand, "expert_command", Risk::kHigh, false, false, false, true},
      {Op::kPreflight, "preflight", Risk::kLow, false, true, false, false},
  };
  return table;
}
inline const OpInfo* op_info(Op op) {
  for (const OpInfo& i : catalog()) if (i.op == op) return &i;
  return nullptr;
}
inline bool op_from_name(std::string_view name, Op& out) {
  for (const OpInfo& i : catalog()) if (name == i.name && i.op != Op::kPreflight) { out = i.op; return true; }
  return false;
}
inline const char* risk_name(Risk r) { return r == Risk::kLow ? "low" : r == Risk::kMedium ? "medium" : r == Risk::kHigh ? "high" : "irreversible"; }

// ---- the models ---------------------------------------------------------------------------------------------------------------------------

struct ModelProfile {
  const char* name;
  int official_ah;       // the capacity the maker's PCB replacement procedure has the technician write; 0 where the procedure names none
  bool firmware;         // a firmware image can be sent (the update tool's table has the model)
};
inline const std::vector<ModelProfile>& models() {
  static const std::vector<ModelProfile> table = {
      {"US2000C", 50, true}, {"US3000C", 74, true}, {"UP5000", 100, true}, {"US5000", 0, true}, {"US2KBPL", 0, false}, {"UP2500", 0, false},
  };
  return table;
}
inline const ModelProfile* model_profile(std::string_view name) {
  for (const ModelProfile& m : models()) if (name == m.name) return &m;
  return nullptr;
}

// ---- the limits ---------------------------------------------------------------------------------------------------------------------------

struct Limit { const char* key; const char* wire; };
struct Order { const char* low; const char* high; };   // value[low] <= value[high]
struct LimitGroup {
  const char* name;
  long lowest, highest;                 // the absolute bounds the tool's files state (milli-volts, milli-degrees)
  std::vector<Limit> keys;
  std::vector<Order> orders;
};
inline const std::vector<LimitGroup>& limit_groups() {
  static const std::vector<LimitGroup> table = {
      {"module_voltage", 20000, 65000,
       {{"pov", "pov"}, {"povr", "povr"}, {"phv", "phv"}, {"phvr", "phvr"}, {"plv", "plv"}, {"plvr", "plvr"}, {"puv", "puv"}, {"puvr", "puvr"}, {"pslp", "pslp"}},
       {{"povr", "pov"}, {"phv", "pov"}, {"phvr", "phv"}, {"plv", "phv"}, {"puv", "plv"}, {"plv", "plvr"}, {"puv", "puvr"}, {"pslp", "puv"}}},
      {"cell_voltage", 0, 7000,
       {{"bov", "bov"}, {"bovr", "bovr"}, {"bhv", "bhv"}, {"bhvr", "bhvr"}, {"blv", "blv"}, {"blvr", "blvr"}, {"buv", "buv"}, {"buvr", "buvr"}, {"bslp", "bslp"}},
       {{"bovr", "bov"}, {"bhv", "bov"}, {"bhvr", "bhv"}, {"blv", "bhv"}, {"buv", "blv"}, {"blv", "blvr"}, {"buv", "buvr"}, {"bslp", "buv"}}},
      {"temperature", -40000, 100000,
       {{"cot", "bot"}, {"cotr", "botr"}, {"cht", "bht"}, {"chtr", "bhtr"}, {"clt", "blt"}, {"cltr", "bltr"}, {"cut", "but"}, {"cutr", "butr"}},
       {{"cotr", "cot"}, {"cht", "cot"}, {"chtr", "cht"}, {"clt", "cht"}, {"clt", "cltr"}, {"cut", "clt"}, {"cut", "cutr"}}},
  };
  return table;
}

inline const std::vector<const char*>& protection_names() {
  static const std::vector<const char*> table = {"bov", "bhv", "blv", "buv", "bslp", "pov", "phv", "plv", "puv", "pslp", "coc", "coc2", "doc", "doc2"};
  return table;
}
inline bool protection_known(std::string_view name) {
  for (const char* n : protection_names()) if (name == n) return true;
  return false;
}

// ---- plans --------------------------------------------------------------------------------------------------------------------------------

enum class StepKind { kSend, kUpdata };
constexpr int kPhasePreflight = 0, kPhaseWrite = 1, kPhaseVerify = 2;

struct Step {
  StepKind kind = StepKind::kSend;
  int phase = kPhasePreflight;
  std::string label;                       // what the capture is called ("info", "stat", "write", ...)
  std::string send;                        // the line, without the Enter; "{pw}" stands for the service password
  std::string prompt = "pylon_debug>";     // what the answer ends with; ">" for any prompt
  std::vector<std::string> must_contain;   // every one must be in the answer
  bool write = false;                      // changes something
  bool tolerate_unknown = false;           // "Unknown command" is not a failure (an older firmware)
  bool tolerate_invalid = false;           // "Invalid command or fail to excute." is not a failure
  bool no_reply_ok = false;                // silence is the expected answer (the battery switched itself off)
  bool secret_in_send = false;
  bool listen = false;                     // send nothing, only wait for the prompt (the battery restarts by itself)
  std::uint32_t timeout_ms = 5'000;
  std::uint32_t settle_ms = 0;             // wait after it before the next line
};

struct Capture { std::string label; int phase; std::string text; };

struct Plan {
  Op op = Op::kPreflight;
  std::string canonical;                   // the exact operation, in words: what the arm is bound to and what the audit keeps
  std::vector<Step> steps;
  std::string summary;                     // one line for the person
  bool atomic = false;
  bool uses_image = false;
  std::size_t image_size = 0;
  std::uint32_t image_crc32 = 0;
  // Checks of the captures after the last step; returns "" when everything is as it should be, else why not.
  std::function<std::string(const std::vector<Capture>&)> verify;
};

// Every refusal the node can give, with the same words in the contract.
namespace refusal {
constexpr const char* kNone = "none";
constexpr const char* kDisabled = "disabled";
constexpr const char* kInvalidParams = "invalid_params";
constexpr const char* kUnknownOp = "unknown_op";
constexpr const char* kNotPylontech = "not_pylontech";
constexpr const char* kNoBattery = "no_battery";
constexpr const char* kUnknownModel = "unknown_model";
constexpr const char* kUnknownFirmware = "unknown_firmware";
constexpr const char* kSerialMismatch = "serial_mismatch";
constexpr const char* kSerialUnreadable = "serial_unreadable";
constexpr const char* kNotIdle = "not_idle";
constexpr const char* kCriticalAlarm = "critical_alarm";
constexpr const char* kBusy = "busy";
constexpr const char* kCooldown = "cooldown";
constexpr const char* kLocked = "locked";
constexpr const char* kNotArmed = "not_armed";
constexpr const char* kBadToken = "bad_token";
constexpr const char* kExpired = "expired";
constexpr const char* kChipUnknown = "chip_unknown";
constexpr const char* kImageMissing = "image_missing";
constexpr const char* kImageMismatch = "image_mismatch";
constexpr const char* kUnsupportedModel = "unsupported_model";
constexpr const char* kNoOfficialValue = "no_official_value";
constexpr const char* kRiskNotAccepted = "risk_not_accepted";
constexpr const char* kExpertBlocked = "expert_blocked";
constexpr const char* kNoPassword = "no_password";
constexpr const char* kBatteryChanged = "battery_changed";
constexpr const char* kClockNotSet = "clock_not_set";
constexpr const char* kNotCancellable = "not_cancellable";
constexpr const char* kNoMemory = "no_memory";
constexpr const char* kNoAnswer = "no_answer";
constexpr const char* kLoginFailed = "login_failed";
constexpr const char* kPreflightFailed = "preflight_failed";
constexpr const char* kStageInvalid = "stage_invalid";
constexpr const char* kStageOrder = "stage_order";
constexpr const char* kStageData = "stage_data";
constexpr const char* kStageSize = "stage_size";
constexpr const char* kStageCrc = "stage_crc";
}  // namespace refusal

namespace detail {
inline std::string ltrim_answer(const std::string& text) { return trim(text); }

inline Step read_step(const char* label, const char* line, int phase = kPhasePreflight, bool tolerate = false) {
  Step s;
  s.phase = phase; s.label = label; s.send = line; s.tolerate_unknown = tolerate; s.tolerate_invalid = tolerate;
  s.must_contain = {"Command completed successfully"};
  if (tolerate) s.must_contain.clear();
  return s;
}
inline Step write_step(const char* label, const std::string& line, std::vector<std::string> must = {"Command completed successfully"}) {
  Step s;
  s.phase = kPhaseWrite; s.label = label; s.send = line; s.write = true; s.must_contain = std::move(must);
  return s;
}

// The steps that open the console in the administrator mode, the way the maker's tool does: leave whatever mode it is in, then log in.
inline void open_console(std::vector<Step>& steps, int phase) {
  Step wake; wake.phase = phase; wake.label = "wake"; wake.send = ""; wake.prompt = ">"; wake.timeout_ms = 3'000; wake.tolerate_unknown = true; wake.tolerate_invalid = true;
  steps.push_back(wake);
  Step out; out.phase = phase; out.label = "logout"; out.send = "logout"; out.prompt = "pylon>"; out.tolerate_unknown = true; out.tolerate_invalid = true;
  steps.push_back(out);
  Step in; in.phase = phase; in.label = "login"; in.send = "login {pw}"; in.secret_in_send = true; in.prompt = ">"; in.must_contain = {"Command completed successfully"};
  steps.push_back(in);
}
// After `trst` the battery is back in the user mode.
inline Step restart_step() {
  Step s = write_step("restart", "trst", {});
  s.prompt = "pylon>"; s.settle_ms = 1'500; s.timeout_ms = 8'000;
  return s;
}
inline Step login_step(int phase = kPhaseWrite) {
  Step in; in.phase = phase; in.label = "login"; in.send = "login {pw}"; in.secret_in_send = true; in.prompt = ">"; in.must_contain = {"Command completed successfully"};
  return in;
}

inline bool digits(std::string_view s, long& out, std::size_t max = 9) {
  if (s.empty() || s.size() > max) return false;
  long n = 0;
  for (char c : s) { if (c < '0' || c > '9') return false; n = n * 10 + (c - '0'); }
  out = n;
  return true;
}
inline bool contains(const std::string& text, std::string_view needle) { return text.find(needle) != std::string::npos; }
inline const Capture* find_capture(const std::vector<Capture>& all, std::string_view label, int phase) {
  const Capture* found = nullptr;
  for (const Capture& c : all) if (c.label == label && c.phase == phase) found = &c;
  return found;
}
}  // namespace detail

// What the person asked for, as the node gets it.
struct Request {
  Op op = Op::kPreflight;
  json::Value params;                      // an object (possibly empty)
  Identity identity;                       // what the arm step read
  int local_year = 0, local_month = 0, local_day = 0, local_hour = 0, local_minute = 0, local_second = 0;   // the time to write, for set_clock / pcb_setup
};

// The words that may not be sent through the expert console: they have their own typed operations (or must never be sent at all).
inline const std::vector<const char*>& expert_blocked_words() {
  static const std::vector<const char*> blocked = {"updata", "eepe", "eepw", "eepr", "tbar", "tmbar", "flaw", "flae", "mm", "login", "logout", "re", "m2s", "save", "rtcw", "socw", "soccfg", "socclr", "urates", "setprt", "wdg", "twdg", "tflash", "tmpicw"};
  return blocked;
}
inline bool expert_blocked_word(std::string_view word) {
  for (const char* b : expert_blocked_words()) if (word == b) return true;
  return false;
}

// An expert line: one command, ASCII, at most 48 characters, words of letters, digits and a few signs; its first word may not be one of the blocked ones. Returns the clean line or "".
inline std::string expert_line(std::string_view text) {
  std::string out;
  bool gap = false;
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || u > 0x7E) return "";
    if (c == ' ') { gap = !out.empty(); continue; }
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == ':' || c == '/';
    if (!ok) return "";
    if (gap) out += ' ';
    gap = false;
    out += c;
  }
  if (out.empty() || out.size() > 48) return "";
  const std::size_t space = out.find(' ');
  std::string first = out.substr(0, space);
  for (char& c : first) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if (expert_blocked_word(first)) return "";
  // no later word may be a blocked one either (`help` followed by a name is harmless, but a line like "pwr tbar" is not worth the doubt)
  std::size_t pos = space;
  while (pos != std::string::npos) {
    const std::size_t next = out.find(' ', pos + 1);
    std::string w = out.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
    for (char& c : w) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (expert_blocked_word(w)) return "";
    pos = next;
  }
  return out;
}

inline std::string two(int v) { char b[8]; std::snprintf(b, sizeof b, "%02d", v); return b; }

// Builds the plan for a request, or says why not (`why` is one of refusal::*). The identity of the battery must already be known (`request.identity`).
inline bool build_plan(const Request& request, Plan& plan, std::string& why) {
  using namespace detail;
  plan = Plan();
  plan.op = request.op;
  const OpInfo* info = op_info(request.op);
  if (info == nullptr || request.op == Op::kPreflight) { why = refusal::kUnknownOp; return false; }
  plan.atomic = info->atomic;
  const Identity& id = request.identity;
  const json::Value& p = request.params;
  const ModelProfile* profile = model_profile(id.model);
  std::vector<Step>& steps = plan.steps;
  open_console(steps, kPhasePreflight);
  steps.push_back(read_step("info", "info"));
  steps.push_back(read_step("stat", "stat"));
  steps.push_back(read_step("time", "time", kPhasePreflight, true));
  steps.push_back(read_step("totalcoul", "totalcoul", kPhasePreflight, true));
  const auto bad = [&](const char* code) { why = code; return false; };
  const auto verify_ok = [](const std::vector<Capture>&) { return std::string(); };
  plan.verify = verify_ok;
  const std::string serial = id.serial;

  switch (request.op) {
    case Op::kSetClock: {
      if (request.local_year < 2020 || request.local_year > 2099 || request.local_month < 1 || request.local_month > 12 || request.local_day < 1 || request.local_day > 31 || request.local_hour < 0 || request.local_hour > 23 ||
          request.local_minute < 0 || request.local_minute > 59 || request.local_second < 0 || request.local_second > 59) return bad(refusal::kInvalidParams);
      const std::string stamp = two(request.local_year % 100) + "-" + two(request.local_month) + "-" + two(request.local_day) + " " + two(request.local_hour) + ":" + two(request.local_minute);
      steps.push_back(write_step("write", "time " + two(request.local_year % 100) + " " + two(request.local_month) + " " + two(request.local_day) + " " + two(request.local_hour) + " " + two(request.local_minute) + " " + two(request.local_second)));
      steps.push_back(read_step("time", "time", kPhaseVerify));
      plan.canonical = "set_clock";   // the time itself is the node's own at the moment of the order, never a value from the request
      plan.summary = "set the battery clock to " + stamp;
      plan.verify = [stamp](const std::vector<Capture>& caps) {
        const Capture* c = find_capture(caps, "time", kPhaseVerify);
        if (c == nullptr) return std::string("no_readback");
        std::string v;
        if (!line_value(c->text, "Time", v)) return std::string("no_readback");
        return v.compare(0, stamp.size(), stamp) == 0 ? std::string() : std::string("clock_differs:" + v);
      };
      break;
    }
    case Op::kClearStats: steps.push_back(write_step("write", "statc")); steps.push_back(read_step("stat", "stat", kPhaseVerify)); plan.canonical = "clear_stats"; plan.summary = "clear the statistics counters"; break;
    case Op::kClearLog: steps.push_back(write_step("write", "logc")); plan.canonical = "clear_log"; plan.summary = "clear the log"; break;
    case Op::kClearData: {
      const std::string which = p.string_or("which", "");
      if (which != "event" && which != "history") return bad(refusal::kInvalidParams);
      steps.push_back(write_step("write", "datac " + which));
      plan.canonical = "clear_data " + which;
      plan.summary = "clear the " + which + " records";
      break;
    }
    case Op::kRestart: steps.push_back(restart_step()); plan.canonical = "restart"; plan.summary = "restart the battery's system"; break;
    case Op::kShutdown: {
      Step s = write_step("write", "shut", {});
      s.no_reply_ok = true; s.prompt = "pylon>"; s.timeout_ms = 4'000;
      steps.push_back(s);
      plan.canonical = "shutdown"; plan.summary = "shut the battery down";
      break;
    }
    case Op::kSocCapacity: {
      if (profile == nullptr || profile->official_ah == 0) return bad(refusal::kNoOfficialValue);
      if (id.capacity_ah >= 0 && id.capacity_ah != profile->official_ah) return bad(refusal::kNoOfficialValue);
      const int ah = profile->official_ah;
      steps.push_back(write_step("write", "totalcoul " + std::to_string(ah), {"Test result:pass"}));
      steps.push_back(read_step("totalcoul", "totalcoul", kPhaseVerify));
      plan.canonical = "soc_capacity " + std::to_string(ah);
      plan.summary = "write the total capacity " + std::to_string(ah) + " Ah";
      plan.verify = [ah](const std::vector<Capture>& caps) {
        const Capture* c = find_capture(caps, "totalcoul", kPhaseVerify);
        if (c == nullptr) return std::string("no_readback");
        const long mah = parse_total_coul_mah(c->text);
        return mah == static_cast<long>(ah) * 1000 ? std::string() : std::string("capacity_differs:" + std::to_string(mah));
      };
      break;
    }
    case Op::kMosfailTest: {
      steps.push_back(write_step("write", "ctrl mostest on"));
      steps.push_back(read_step("mostest", "ctrl mostest", kPhaseVerify));
      plan.canonical = "mosfail_test on"; plan.summary = "switch the MOSFET-failure test on";
      plan.verify = [](const std::vector<Capture>& caps) {
        const Capture* c = find_capture(caps, "mostest", kPhaseVerify);
        std::string v;
        if (c == nullptr || !line_value(c->text, "mostest", v)) return std::string("no_readback");
        return v.compare(0, 2, "ON") == 0 ? std::string() : std::string("mostest_differs:" + v);
      };
      break;
    }
    case Op::kSocReset: {
      if (profile == nullptr || profile->official_ah == 0) return bad(refusal::kNoOfficialValue);
      if (!valid_serial(serial)) return bad(refusal::kSerialUnreadable);   // the erase takes the serial number with it: it is written back
      const int ah = profile->official_ah;
      steps.push_back(write_step("setlowcfg", "setlowcfg"));
      steps.push_back(write_step("eeprom", "eepe 0"));
      steps.push_back(restart_step());
      steps.push_back(login_step());
      steps.push_back(write_step("capacity", "totalcoul " + std::to_string(ah), {"Test result:pass"}));
      steps.push_back(write_step("barcode", "tbar " + serial, {"Test result:pass"}));
      steps.push_back(read_step("info", "info", kPhaseVerify));
      steps.push_back(read_step("totalcoul", "totalcoul", kPhaseVerify));
      steps.push_back(read_step("setlowcfg", "setlowcfg", kPhaseVerify));
      plan.canonical = "soc_reset " + std::to_string(ah) + " " + serial;
      plan.summary = "erase the learned state, restart, write " + std::to_string(ah) + " Ah and the serial number again";
      plan.verify = [ah, serial](const std::vector<Capture>& caps) {
        const Capture* i = find_capture(caps, "info", kPhaseVerify);
        const Capture* t = find_capture(caps, "totalcoul", kPhaseVerify);
        if (i == nullptr || t == nullptr) return std::string("no_readback");
        if (parse_info(i->text).serial != serial) return std::string("barcode_differs");
        return parse_total_coul_mah(t->text) == static_cast<long>(ah) * 1000 ? std::string() : std::string("capacity_differs");
      };
      break;
    }
    case Op::kPcbSetup: {
      if (profile == nullptr || profile->official_ah == 0) return bad(refusal::kNoOfficialValue);
      const std::string label = p.string_or("serial", "");
      if (!valid_serial(label)) return bad(refusal::kInvalidParams);
      if (request.local_year < 2020 || request.local_year > 2099) return bad(refusal::kInvalidParams);
      const int ah = profile->official_ah;
      steps.push_back(write_step("capacity", "totalcoul " + std::to_string(ah), {"Test result:pass"}));
      steps.push_back(write_step("mostest", "ctrl mostest on"));
      steps.push_back(write_step("clock", "time " + two(request.local_year % 100) + " " + two(request.local_month) + " " + two(request.local_day) + " " + two(request.local_hour) + " " + two(request.local_minute) + " " + two(request.local_second)));
      steps.push_back(write_step("barcode", "tbar " + label, {"Test result:pass"}));
      steps.push_back(read_step("info", "info", kPhaseVerify));
      steps.push_back(read_step("totalcoul", "totalcoul", kPhaseVerify));
      steps.push_back(read_step("mostest", "ctrl mostest", kPhaseVerify));
      plan.canonical = "pcb_setup " + std::to_string(ah) + " " + label;
      plan.summary = "after a board replacement: write " + std::to_string(ah) + " Ah, the MOSFET test, the clock and the serial number";
      plan.verify = [ah, label](const std::vector<Capture>& caps) {
        const Capture* i = find_capture(caps, "info", kPhaseVerify);
        const Capture* t = find_capture(caps, "totalcoul", kPhaseVerify);
        if (i == nullptr || t == nullptr) return std::string("no_readback");
        if (parse_info(i->text).serial != label) return std::string("barcode_differs");
        return parse_total_coul_mah(t->text) == static_cast<long>(ah) * 1000 ? std::string() : std::string("capacity_differs");
      };
      break;
    }
    case Op::kLimitsSet: {
      const std::string group_name = p.string_or("group", "");
      const LimitGroup* group = nullptr;
      for (const LimitGroup& g : limit_groups()) if (group_name == g.name) group = &g;
      const json::Value* values = p.get("values");
      if (group == nullptr || values == nullptr || !values->is_object() || values->items.size() != group->keys.size()) return bad(refusal::kInvalidParams);
      std::map<std::string, long> v;
      for (std::size_t i = 0; i < values->items.size(); ++i) {
        const json::Value& item = values->items[i];
        bool known = false;
        for (const Limit& k : group->keys) if (values->names[i] == k.key) known = true;
        if (!known || !item.is_number() || item.number != static_cast<double>(static_cast<long>(item.number))) return bad(refusal::kInvalidParams);
        const long n = static_cast<long>(item.number);
        if (n < group->lowest || n > group->highest) return bad(refusal::kInvalidParams);
        v[values->names[i]] = n;
      }
      if (v.size() != group->keys.size()) return bad(refusal::kInvalidParams);
      for (const Order& o : group->orders) if (v[o.low] > v[o.high]) return bad(refusal::kInvalidParams);
      std::string canon = std::string("limits_set ") + group->name;
      for (const Limit& k : group->keys) {
        steps.push_back(write_step("write", std::string("config ") + k.wire + " " + std::to_string(v[k.key])));
        canon += std::string(" ") + k.key + "=" + std::to_string(v[k.key]);
      }
      steps.push_back(write_step("save", "config save"));
      steps.push_back(read_step("config", "config", kPhaseVerify));
      steps.back().must_contain.clear();
      plan.canonical = canon;
      plan.summary = std::string("write the ") + group->name + " limits";
      std::vector<std::string> wanted;
      for (const Limit& k : group->keys) wanted.push_back(std::to_string(v[k.key]));
      plan.verify = [wanted](const std::vector<Capture>& caps) {
        const Capture* c = find_capture(caps, "config", kPhaseVerify);
        if (c == nullptr || c->text.size() < 8) return std::string("no_readback");
        for (const std::string& w : wanted) if (!contains(c->text, w)) return std::string("value_not_read_back:" + w);
        return std::string();
      };
      break;
    }
    case Op::kLimitsDefault: steps.push_back(write_step("write", "config default")); steps.push_back(write_step("save", "config save")); steps.push_back(read_step("config", "config", kPhaseVerify)); plan.canonical = "limits_default"; plan.summary = "put every limit back to the factory value"; break;
    case Op::kProtectionsSet: {
      const json::Value* items = p.get("items");
      if (items == nullptr || !items->is_object() || items->items.empty() || items->items.size() > 14) return bad(refusal::kInvalidParams);
      bool disables = false;
      std::string canon = "protections_set";
      std::vector<std::pair<std::string, std::string>> list;
      for (std::size_t i = 0; i < items->items.size(); ++i) {
        const std::string name = items->names[i];
        const json::Value& state = items->items[i];
        if (!protection_known(name) || !state.is_string() || (state.text != "en" && state.text != "dis")) return bad(refusal::kInvalidParams);
        for (const auto& done : list) if (done.first == name) return bad(refusal::kInvalidParams);
        list.emplace_back(name, state.text);
        if (state.text == "dis") disables = true;
      }
      if (disables && !p.bool_or("accept_risk", false)) return bad(refusal::kRiskNotAccepted);
      for (const auto& item : list) { steps.push_back(write_step("write", "prot " + item.first + " " + item.second)); canon += " " + item.first + "=" + item.second; }
      Step show = read_step("prot", "prot", kPhaseVerify);
      show.must_contain.clear();
      steps.push_back(show);
      plan.canonical = canon;
      plan.summary = "enable or disable protections";
      break;
    }
    case Op::kProtectionsDefault: steps.push_back(write_step("write", "prot default")); steps.push_back(read_step("prot", "prot", kPhaseVerify)); steps.back().must_contain.clear(); plan.canonical = "protections_default"; plan.summary = "put every protection back to the factory setting"; break;
    case Op::kOutputSet: {
      const std::string output = p.string_or("output", ""), state = p.string_or("state", "");
      if ((output != "cfet" && output != "dfet" && output != "buzz" && output != "heat") || (state != "on" && state != "off")) return bad(refusal::kInvalidParams);
      steps.push_back(read_step("output", ("ctrl " + output).c_str(), kPhasePreflight, true));
      steps.push_back(write_step("write", "ctrl " + output + " " + state));
      steps.push_back(read_step("output", ("ctrl " + output).c_str(), kPhaseVerify));
      plan.canonical = "output_set " + output + " " + state;
      plan.summary = "switch " + output + " " + state;
      std::string upper_state = state == "on" ? "ON" : "OFF";
      plan.verify = [output, upper_state](const std::vector<Capture>& caps) {
        const Capture* c = find_capture(caps, "output", kPhaseVerify);
        std::string v;
        if (c == nullptr || !line_value(c->text, output, v)) {
          // the label may be in capitals
          std::string capital = output;
          for (char& ch : capital) ch = static_cast<char>(ch - 'a' + 'A');
          if (c == nullptr || !line_value(c->text, capital, v)) return std::string("no_readback");
        }
        return v.compare(0, upper_state.size(), upper_state) == 0 && (v.size() == upper_state.size() || v[upper_state.size()] == ' ') ? std::string() : std::string("output_differs:" + v);
      };
      break;
    }
    case Op::kFaultClear: steps.push_back(write_step("write", "ctrl afuse off")); plan.canonical = "fault_clear afuse"; plan.summary = "clear the fuse fault"; break;
    case Op::kWriteBarcode: {
      const std::string label = p.string_or("serial", "");
      if (!valid_serial(label)) return bad(refusal::kInvalidParams);
      steps.push_back(write_step("write", "tbar " + label, {"Test result:pass"}));
      steps.push_back(read_step("info", "info", kPhaseVerify));
      plan.canonical = "write_barcode " + label;
      plan.summary = "write the serial number " + label;
      plan.verify = [label](const std::vector<Capture>& caps) {
        const Capture* i = find_capture(caps, "info", kPhaseVerify);
        if (i == nullptr) return std::string("no_readback");
        return parse_info(i->text).serial == label ? std::string() : std::string("barcode_differs");
      };
      break;
    }
    case Op::kEepromErase: {
      const bool restore = p.bool_or("restore_barcode", true);
      if (restore && !valid_serial(serial)) return bad(refusal::kSerialUnreadable);
      steps.push_back(write_step("eeprom", "eepe 0"));
      steps.push_back(restart_step());
      steps.push_back(login_step());
      if (restore) steps.push_back(write_step("barcode", "tbar " + serial, {"Test result:pass"}));
      steps.push_back(read_step("info", "info", kPhaseVerify));
      plan.canonical = std::string("eeprom_erase") + (restore ? " restore_barcode " + serial : " no_restore");
      plan.summary = "erase the external memory and restart" + std::string(restore ? ", then write the serial number again" : "");
      plan.verify = [restore, serial](const std::vector<Capture>& caps) {
        const Capture* i = find_capture(caps, "info", kPhaseVerify);
        if (i == nullptr) return std::string("no_readback");
        if (restore && parse_info(i->text).serial != serial) return std::string("barcode_differs");
        return std::string();
      };
      break;
    }
    case Op::kFirmwareUpdate: {
      if (profile == nullptr || !profile->firmware) return bad(refusal::kUnsupportedModel);
      if (!valid_serial(serial)) return bad(refusal::kSerialUnreadable);
      if (chip_of(id) == Chip::kUnknown) return bad(refusal::kChipUnknown);
      const long size = static_cast<long>(p.number_or("image_size", 0));
      const double crc = p.number_or("image_crc32", -1);
      const std::string ver = p.string_or("image_version", "");
      if (size < static_cast<long>(kImageMinBytes) || size > static_cast<long>(kImageMaxBytes) || crc < 0 || crc > 4294967295.0 || ver.empty() || ver.size() > 12) return bad(refusal::kInvalidParams);
      const bool clear_eeprom = p.bool_or("clear_eeprom", false);
      Step up;
      up.kind = StepKind::kUpdata; up.phase = kPhaseWrite; up.label = "updata"; up.send = "updata"; up.write = true; up.timeout_ms = 600'000;
      steps.push_back(up);
      Step after; after.phase = kPhaseWrite; after.label = "restart"; after.listen = true; after.prompt = "pylon>"; after.timeout_ms = 30'000; after.settle_ms = 1'500;
      steps.push_back(after);
      steps.push_back(login_step());
      steps.push_back(read_step("info", "info", kPhaseWrite));
      steps.push_back(write_step("barcode", "tbar " + serial, {"Test result:pass"}));
      if (clear_eeprom) { steps.push_back(write_step("eeprom", "eepe 0")); steps.push_back(restart_step()); steps.push_back(login_step()); steps.push_back(write_step("barcode", "tbar " + serial, {"Test result:pass"})); }
      steps.push_back(read_step("info", "info", kPhaseVerify));
      plan.uses_image = true;
      plan.image_size = static_cast<std::size_t>(size);
      plan.image_crc32 = static_cast<std::uint32_t>(crc);
      plan.canonical = "firmware_update " + ver + " " + std::to_string(size) + " " + std::to_string(static_cast<unsigned long>(crc)) + (clear_eeprom ? " clear_eeprom " : " ") + serial;
      plan.summary = "send firmware " + ver + " to the battery" + std::string(clear_eeprom ? ", then erase the external memory" : "") + " and write the serial number again";
      plan.verify = [serial](const std::vector<Capture>& caps) {
        const Capture* i = find_capture(caps, "info", kPhaseVerify);
        if (i == nullptr) return std::string("no_readback");
        const Identity now = parse_info(i->text);
        if (!now.valid) return std::string("battery_not_answering");
        return now.serial == serial ? std::string() : std::string("barcode_differs");
      };
      break;
    }
    case Op::kExpertCommand: {
      const std::string line = expert_line(p.string_or("line", ""));
      if (line.empty()) return bad(refusal::kExpertBlocked);
      Step s; s.phase = kPhaseWrite; s.label = "expert"; s.send = line; s.write = true; s.tolerate_invalid = true; s.tolerate_unknown = true; s.timeout_ms = 8'000;
      steps.push_back(s);
      plan.canonical = "expert_command " + line;
      plan.summary = "send the command '" + line + "' to the console";
      break;
    }
    case Op::kPreflight: break;
  }
  return true;
}

// Whether the staged image may be sent to this battery under this plan: the size and CRC are what the plan was made for, and the image is the one that fits the battery's model, chip and
// board. "" when it may, else the refusal.
inline std::string check_firmware_request(const Plan& plan, const std::vector<std::uint8_t>& image, const Identity& id) {
  if (!plan.uses_image) return refusal::kUnknownOp;
  if (image.size() != plan.image_size || crc32(image.data(), image.size()) != plan.image_crc32) return refusal::kImageMissing;
  const ImageVerdict v = check_image(image, id);
  if (v.ok) return refusal::kNone;
  if (v.code == "chip_unknown") return refusal::kChipUnknown;
  if (v.code == "unsupported_model") return refusal::kUnsupportedModel;
  if (v.code == "board_mismatch") return refusal::kImageMismatch;
  return refusal::kImageMismatch;
}

// The plan that reads: logs in and reads what a decision needs. What the arm step runs.
inline Plan preflight_plan() {
  using namespace detail;
  Plan plan;
  plan.op = Op::kPreflight;
  plan.canonical = "preflight";
  plan.summary = "read the battery's identity";
  open_console(plan.steps, kPhasePreflight);
  plan.steps.push_back(read_step("info", "info"));
  plan.steps.push_back(read_step("stat", "stat"));
  plan.steps.push_back(read_step("time", "time", kPhasePreflight, true));
  plan.steps.push_back(read_step("totalcoul", "totalcoul", kPhasePreflight, true));
  return plan;
}

// ---- the preconditions -----------------------------------------------------------------------------------------------------------------

// What the node knows about the battery at the port when it is asked: from its last readings and from the preflight.
struct Facts {
  bool pylontech_port = false;
  bool present = false;
  long current_ma = 0;
  bool critical_alarm = false;
  Identity identity;               // from the preflight
  std::string typed_serial;        // what the person typed to confirm
};

inline constexpr long kIdleCurrentMa = 1'000;

// "" when the operation may start, else the refusal.
inline std::string preconditions(Op op, const Facts& f, const json::Value& params) {
  const OpInfo* info = op_info(op);
  if (info == nullptr) return refusal::kUnknownOp;
  if (!f.pylontech_port) return refusal::kNotPylontech;
  if (!f.present) return refusal::kNoBattery;
  if (!f.identity.valid || model_profile(f.identity.model) == nullptr) return refusal::kUnknownModel;
  if (f.identity.soft_version.empty()) return refusal::kUnknownFirmware;
  const bool writes_serial = op == Op::kWriteBarcode || op == Op::kPcbSetup;
  if (f.identity.serial.empty()) {
    // a battery with no serial number (a new board): only the operations that write one go on, and the person types the serial they write
    if (writes_serial) {
      if (f.typed_serial != params.string_or("serial", "") || !valid_serial(f.typed_serial)) return refusal::kSerialMismatch;
    } else if (f.typed_serial != "NOSERIAL") {
      return refusal::kSerialUnreadable;
    }
  } else if (f.typed_serial != f.identity.serial) {
    return refusal::kSerialMismatch;
  }
  if (info->needs_idle && (f.current_ma > kIdleCurrentMa || f.current_ma < -kIdleCurrentMa)) return refusal::kNotIdle;
  if (f.critical_alarm && !info->alarm_ok) {
    // switching an output OFF is a safe move in an alarm
    const bool off = op == Op::kOutputSet && params.string_or("state", "") == "off";
    if (!off) return refusal::kCriticalAlarm;
  }
  return refusal::kNone;
}

// ---- who may start what, and when ------------------------------------------------------------------------------------------------------

struct Armed {
  bool used = false;
  int port = 0;
  std::string canonical;
  std::string serial;
  std::string token;
  std::uint64_t at_ms = 0;
  Identity identity;
  std::string before;            // the snapshot read when it was armed
};

// The one-time token pattern of the switching: an operation is armed (a token is made), and only the exact same operation on the exact same battery may use it, once, within the window.
class Guard {
 public:
  static constexpr std::uint64_t kArmWindowMs = 120'000;
  static constexpr std::uint64_t kCooldownMs = 30'000;
  static constexpr int kPorts = 10;
  static constexpr int kFailuresToLock = 3;
  static constexpr std::uint64_t kFailureWindowMs = 10 * 60'000, kLockMs = 5 * 60'000;

  // Whether an arm may start now.
  const char* can_arm(int port, std::uint64_t now_ms) const {
    if (port < 1 || port > kPorts) return refusal::kInvalidParams;
    if (locked(now_ms)) return refusal::kLocked;
    if (busy_[port - 1]) return refusal::kBusy;
    if (cooling(port, now_ms)) return refusal::kCooldown;
    return refusal::kNone;
  }

  // Stores an arm (replacing an older one of the port). The caller made the token (random, hexadecimal).
  void arm(int port, const std::string& canonical, const std::string& serial, const Identity& identity, const std::string& before, std::string token, std::uint64_t now_ms) {
    Armed& a = armed_[port - 1];
    a = Armed();
    a.used = true; a.port = port; a.canonical = canonical; a.serial = serial; a.token = std::move(token); a.at_ms = now_ms; a.identity = identity; a.before = before;
  }

  // The token is spent by the first attempt, right or wrong. Returns kNone and fills `out` when the operation may start.
  const char* execute(int port, const std::string& canonical, const std::string& serial, const std::string& token, std::uint64_t now_ms, Armed& out) {
    if (port < 1 || port > kPorts) return refusal::kInvalidParams;
    if (locked(now_ms)) return refusal::kLocked;
    Armed& a = armed_[port - 1];
    if (!a.used) return refusal::kNotArmed;
    const bool expired = now_ms - a.at_ms > kArmWindowMs;
    const bool match = auth::same_text(a.token, token) && a.canonical == canonical && a.serial == serial;
    const Armed copy = a;
    a = Armed();   // one arm, one try
    if (expired) return refusal::kExpired;
    if (!match) { note_failure(now_ms); return auth::same_text(copy.token, token) ? refusal::kInvalidParams : refusal::kBadToken; }
    if (busy_[port - 1]) return refusal::kBusy;
    if (cooling(port, now_ms)) return refusal::kCooldown;
    out = copy;
    return refusal::kNone;
  }

  // The arm of a port, without spending it (the node needs to know which battery it was made for before it can say what the operation is).
  bool peek(int port, Armed& out) const {
    if (port < 1 || port > kPorts || !armed_[port - 1].used) return false;
    out = armed_[port - 1];
    return true;
  }

  void begin(int port) { busy_[port - 1] = true; }
  void finish(int port, bool wrote, std::uint64_t now_ms) { busy_[port - 1] = false; if (wrote) last_write_ms_[port - 1] = now_ms + 1; }
  bool busy(int port) const { return port >= 1 && port <= kPorts && busy_[port - 1]; }
  void forget(int port) { if (port >= 1 && port <= kPorts) armed_[port - 1] = Armed(); }
  bool locked(std::uint64_t now_ms) const { return lock_until_ms_ != 0 && now_ms < lock_until_ms_; }

 private:
  bool cooling(int port, std::uint64_t now_ms) const { return last_write_ms_[port - 1] != 0 && now_ms >= last_write_ms_[port - 1] && now_ms - last_write_ms_[port - 1] < kCooldownMs; }
  void note_failure(std::uint64_t now_ms) {
    if (failures_ == 0 || now_ms - first_failure_ms_ > kFailureWindowMs) { failures_ = 0; first_failure_ms_ = now_ms; }
    if (++failures_ >= kFailuresToLock) { lock_until_ms_ = now_ms + kLockMs; failures_ = 0; }
  }
  Armed armed_[kPorts];
  bool busy_[kPorts] = {};
  std::uint64_t last_write_ms_[kPorts] = {};
  int failures_ = 0;
  std::uint64_t first_failure_ms_ = 0, lock_until_ms_ = 0;
};

}  // namespace armor::solar::battery
