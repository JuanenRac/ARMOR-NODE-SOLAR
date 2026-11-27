// ARMOR-NODE-SOLAR - the node's side of the battery maintenance (see battery_maintenance.hpp, core/battery_ops.hpp and docs/BATTERY_MAINTENANCE.md).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// An order arrives as a JSON message on the broker. Writing is two steps, like the switching of the electrical node: `arm` reads the battery (identity, state), checks every precondition and
// answers with a one-time token and the exact operation it will do; `execute` carries the token and the same operation, and only then is anything written. The service password is the node's
// own setting: it never appears in an answer, a log or a message. Everything is refused while the setting is off. Nothing here has run on a board.
//
// The node's own panel takes the same way (panel_arm, panel_execute ...): the same checks, guard and runner, with the answers kept for the panel instead of published, the one-time token kept in
// the node (the browser never sees it) and a record of who did what in flash. Its rules are in core/battery_panel.hpp. With the writes off nothing of it is allocated.
#include "battery_maintenance.hpp"

#include <atomic>
#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
extern "C" {
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}
#include "core/battery_ops.hpp"
#include "core/battery_panel.hpp"
#include "core/command_sig.hpp"
#include "core/battery_stage.hpp"
#include "core/json.hpp"
#include "entropy.hpp"
#include "mqtt_link.hpp"
#include "node_control.hpp"
#include "node_store.hpp"
#include "psram_new.hpp"
#include "solar_manager.hpp"

namespace armor::maintenance {
namespace {
namespace bat = armor::solar::battery;
namespace bp = armor::solar::battery::panel;
constexpr char kAuditKey[] = "bataudit", kCampaignKey[] = "batcamp";
constexpr char kTag[] = "armor-battery";

std::mutex g_lock;
bat::Guard g_guard;
bat::FailedUpdate g_failed[bat::Guard::kPorts];   // the last firmware update of each port that did not end well (in memory: lost with a restart)
bat::ImageStage g_stage;
std::string g_node;
bool g_enabled = false;
bat::ExpertMode g_expert_mode = bat::ExpertMode::kBlocklist;   // battery.expert_mode, under g_lock when it changes
std::string g_password;
std::string g_command_key;   // battery.command_key, under g_lock when it changes
bool g_require_sig = false;  // battery.require_sig, under g_lock when it changes
command_sig::NonceCache g_nonces;   // only the worker task reads it

std::uint64_t uptime_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL; }

// Everything that thinks (reading an order, building a plan, checking a firmware image) runs in one task of its own with a large stack: the broker's client and the ports' tasks only hand work
// over. The queue is short; when it is full the oldest piece of work is dropped (an order that is lost is asked again by the server, which gives up on it by itself).
std::mutex g_queue_lock;
std::deque<std::function<void()>> g_queue;
SemaphoreHandle_t g_ready = nullptr;
constexpr std::size_t kQueueMax = 64;

void post(std::function<void()> work) {
  if (g_ready == nullptr) return;
  {
    std::lock_guard<std::mutex> guard(g_queue_lock);
    g_queue.push_back(std::move(work));
    if (g_queue.size() > kQueueMax) g_queue.pop_front();
  }
  xSemaphoreGive(g_ready);
}

void worker(void*) {
  for (;;) {
    xSemaphoreTake(g_ready, portMAX_DELAY);
    for (;;) {
      std::function<void()> next;
      {
        std::lock_guard<std::mutex> guard(g_queue_lock);
        if (g_queue.empty()) break;
        next = std::move(g_queue.front());
        g_queue.pop_front();
      }
      next();
    }
  }
}

std::string token_hex() {
  std::uint8_t raw[16];
  random_bytes(raw, sizeof raw);
  static const char digits[] = "0123456789abcdef";
  std::string out;
  for (std::uint8_t b : raw) { out += digits[b >> 4]; out += digits[b & 15]; }
  return out;
}

// Who asked for what: carried to the answer.
struct Context {
  std::string command_id, action, op;
  int port = 0;
  bool panel = false;           // asked by the node's own panel: the answer is kept for it, not published
  std::string user, serial;     // who, and the serial number typed (for the record)
};

// One answer. The fields that do not apply are left out.
struct Reply {
  Context ctx;
  std::string stage;                 // armed, started, progress, final, rejected, staged
  bool accepted = false;
  std::string refusal = "none";
  std::string token, canonical, summary, risk;
  bool atomic = false;
  bool has_battery = false;
  bat::Identity battery;
  std::string before;
  std::string code;                  // why a preflight failed, when it says more than the refusal
  bool has_result = false;
  bat::RunResult result;
  std::size_t received = 0;
};

std::string reply_json(const Reply& r, bool with_token) {
  json::Writer w;
  w.begin_object().field("kind", "solar_battery_result").field("node_id", g_node);
  w.key("timestamp_ms").integer(static_cast<long long>(mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : uptime_ms()));
  w.field("command_id", r.ctx.command_id).field("port", r.ctx.port).field("action", r.ctx.action);
  if (!r.ctx.op.empty()) w.field("op", r.ctx.op);
  w.field("stage", r.stage).field("accepted", r.accepted).field("refusal", r.refusal);
  if (!r.code.empty() && !r.has_result) w.field("code", r.code);
  if (with_token && !r.token.empty()) w.field("token", r.token);
  if (!r.canonical.empty()) w.field("canonical", r.canonical);
  if (!r.summary.empty()) w.field("summary", r.summary);
  if (!r.risk.empty()) w.field("risk", r.risk).field("atomic", r.atomic);
  if (r.has_battery) {
    w.key("battery").begin_object().field("model", r.battery.model).field("serial", r.battery.serial).field("soft_version", r.battery.soft_version).field("board", r.battery.board);
    if (r.battery.capacity_ah >= 0) w.field("capacity_ah", r.battery.capacity_ah);
    w.end_object();
  }
  if (!r.before.empty() && !r.has_result) w.field("snapshot_before", r.before);
  if (r.has_result) {
    const bat::RunResult& x = r.result;
    w.field("outcome", x.outcome);
    if (!x.code.empty()) w.field("code", x.code);
    w.field("steps_done", static_cast<unsigned>(x.steps_done)).field("steps_total", static_cast<unsigned>(x.steps_total)).field("writes_done", static_cast<unsigned>(x.writes_done));
    if (x.packets_total > 0) w.field("packets_sent", static_cast<unsigned>(x.packets_sent)).field("packets_total", static_cast<unsigned>(x.packets_total));
    if (r.stage == "final") {
      w.field("truncated", x.truncated);
      if (!x.before.empty()) w.field("before", x.before);
      if (!x.after.empty()) w.field("after", x.after);
      if (!x.transcript.empty()) w.field("transcript", x.transcript);
    }
  }
  if (r.stage == "staged") w.field("received", static_cast<unsigned>(r.received));
  w.end_object();
  return w.str();
}

// ---- what is kept for the panel ---------------------------------------------------------------------------------------------------------------
// g_panel_lock is only ever taken after g_lock (or alone), never the other way round: an answer is sometimes given with g_lock held.

struct PanelPort {
  std::string session, user, op, serial, command_id, stage, reply, token;
  json::Value params;
  std::uint64_t armed_ms = 0;
  bool campaign = false;         // a step of the campaign
};
struct PanelState {
  PanelPort ports[bat::Guard::kPorts];
  bp::OrderLimit orders;
  bp::Campaign campaign;
};
std::mutex g_panel_lock;
std::unique_ptr<PanelState> g_panel;   // made only when the panel is first used with the writes on, in the external memory

// Call with g_panel_lock held.
PanelState& panel_data() {
  if (!g_panel) {
    std::string kept;
    const bool has = store::blob_read(kCampaignKey, kept);
    const memory::ExternalScope in_psram;
    g_panel = std::make_unique<PanelState>();
    if (has && g_panel->campaign.from_json(kept)) g_panel->campaign.restarted();
  }
  return *g_panel;
}

// The record and the campaign are written to the flash by the worker, never under a lock somebody waits for.
void note(const std::string& user, int port, const char* event, const std::string& op, const std::string& detail, const std::string& serial) {
  bp::AuditEntry entry;
  entry.at_s = mqtt_link::clock_is_set() ? static_cast<std::uint64_t>(mqtt_link::wall_clock_ms() / 1000) : 0;
  entry.user = user; entry.port = port; entry.event = event; entry.op = op; entry.detail = detail; entry.serial = serial;
  post([entry] {
    std::string text;
    store::blob_read(kAuditKey, text);
    bp::Audit audit;
    audit.from_json(text);
    audit.add(entry);
    if (!store::blob_write(kAuditKey, audit.to_json())) ESP_LOGW(kTag, "the record of the battery operations could not be written");
  });
}

// Call with g_panel_lock held.
void keep_campaign() {
  const std::string text = g_panel->campaign.to_json();
  post([text] { if (!store::blob_write(kCampaignKey, text)) ESP_LOGW(kTag, "the campaign could not be written"); });
}

void record(const Reply& r) {
  const std::string serial = r.has_battery && !r.battery.serial.empty() ? r.battery.serial : r.ctx.serial;
  const std::string user = r.ctx.panel ? r.ctx.user : std::string("server");
  if (r.stage == "started") note(user, r.ctx.port, "started", r.ctx.op, "", serial);
  else if (r.stage == "final") note(user, r.ctx.port, "ended", r.ctx.op, r.result.outcome + " " + r.result.code, serial);
  else if (r.ctx.panel && r.stage == "armed") note(user, r.ctx.port, "armed", r.ctx.op, "", serial);
  else if (r.ctx.panel && r.stage == "rejected") note(user, r.ctx.port, "refused", r.ctx.op, r.code.empty() ? r.refusal : r.code, serial);
}

void say(const Reply& r) {
  record(r);
  if (!r.ctx.panel) { mqtt_link::publish("armor/solar/" + g_node + "/maintenance/result", reply_json(r, true)); return; }
  std::lock_guard<std::mutex> guard(g_panel_lock);
  if (!g_panel || r.ctx.port < 1 || r.ctx.port > bat::Guard::kPorts) return;
  PanelPort& kept = g_panel->ports[r.ctx.port - 1];
  if (kept.command_id != r.ctx.command_id) return;   // an older order of this port
  const memory::ExternalScope in_psram;
  kept.stage = r.stage == "progress" ? std::string("running") : r.stage;
  kept.reply = reply_json(r, false);
  if (r.stage == "armed") { kept.token = r.token; kept.armed_ms = uptime_ms(); }
  bp::Campaign& campaign = g_panel->campaign;
  if (!kept.campaign || campaign.port != r.ctx.port) return;
  if (r.stage == "armed") {
    campaign.armed(r.battery);
    if (campaign.state != "active") kept.token.clear();   // another battery than the module's: the arm cannot be executed
  } else if (r.stage == "rejected") campaign.refused(r.code.empty() ? r.refusal : r.code);
  else if (r.stage == "final") campaign.ended(r.result.outcome, r.result.code);
  else return;
  keep_campaign();
}

void reject(const Context& ctx, const char* why) {
  Reply r;
  r.ctx = ctx; r.stage = "rejected"; r.accepted = false; r.refusal = why;
  // The two answers about the bootloader are not refusals of the shared contract: they travel as the code of a failed preflight.
  const std::string said = why;
  if (said == bat::refusal::kBootloader || said == bat::refusal::kNothingToResume) { r.refusal = bat::refusal::kPreflightFailed; r.code = said; }
  ESP_LOGW(kTag, "port %d: %s refused (%s)", ctx.port, ctx.action.c_str(), why);
  say(r);
}

// The node's own wall clock, as the battery's: the time written is never taken from the order.
bool fill_local_time(bat::Request& request) {
  if (!mqtt_link::clock_is_set()) return false;
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  request.local_year = local.tm_year + 1900; request.local_month = local.tm_mon + 1; request.local_day = local.tm_mday;
  request.local_hour = local.tm_hour; request.local_minute = local.tm_min; request.local_second = local.tm_sec;
  return true;
}

bool needs_clock(bat::Op op) { return op == bat::Op::kSetClock || op == bat::Op::kPcbSetup; }

json::Value params_of(const json::Value& in) {
  const json::Value* p = in.get("params");
  if (p != nullptr && p->is_object()) return *p;
  json::Value empty;
  empty.type = json::Type::kObject;
  return empty;
}

void finish_job(const Context& ctx, bool uses_image, const bat::Plan& plan, const bat::RunResult& result) {
  {
    std::lock_guard<std::mutex> guard(g_lock);
    if (uses_image && ctx.port >= 1 && ctx.port <= bat::Guard::kPorts) {
      bat::FailedUpdate& failed = g_failed[ctx.port - 1];
      if (result.outcome == "ok") failed = bat::FailedUpdate{};
      else if (result.wrote) {
        const bool again = failed.valid && failed.image_crc32 == plan.image_crc32 && failed.identity.serial == plan.expected.serial;
        failed = bat::FailedUpdate{true, plan.expected, plan.image_size, plan.image_crc32, again ? failed.resumes + 1 : 0};
      }
    }
    g_guard.finish(ctx.port, result.wrote, uptime_ms());
    bool campaign_goes_on = false;
    {
      std::lock_guard<std::mutex> panel(g_panel_lock);
      campaign_goes_on = ctx.panel && g_panel && g_panel->campaign.state == "active" && g_panel->campaign.port == ctx.port;
    }
    if (uses_image && !campaign_goes_on) g_stage.clear();   // an image is used once, except along the modules of a campaign of the panel
  }
  Reply r;
  r.ctx = ctx; r.stage = "final"; r.accepted = true; r.refusal = "none"; r.has_result = true; r.result = result;
  ESP_LOGI(kTag, "port %d: %s ended: %s %s", ctx.port, ctx.op.c_str(), result.outcome.c_str(), result.code.c_str());
  say(r);
}

// ---- arm: read the battery, check, make the token ----------------------------------------------------------------------------------------------

// The request of a resumed update: the identity is the one read before the try that failed. "" when it may go on.
std::string resume_request(int port, const std::string& typed, const json::Value& params, bat::Request& request, bat::Plan& plan) {
  const bat::FailedUpdate& failed = g_failed[port - 1];
  if (!failed.valid) return bat::refusal::kNothingToResume;
  request.op = bat::Op::kFirmwareUpdate; request.params = params; request.identity = failed.identity; request.expert_mode = g_expert_mode; request.resume = true;
  std::string why;
  if (!bat::build_plan(request, plan, why)) return why;
  const std::string refused = bat::resume_check(failed, plan, typed);
  if (refused != bat::refusal::kNone) return refused;
  const auto image = g_stage.image();
  return image ? bat::check_firmware_request(plan, *image, failed.identity) : std::string(bat::refusal::kImageMissing);
}

void arm_resume(const Context& ctx, const std::string& typed, const json::Value& params) {
  Reply r;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    bat::Request request;
    bat::Plan plan;
    const std::string refused = resume_request(ctx.port, typed, params, request, plan);
    if (refused != bat::refusal::kNone) { reject(ctx, refused.c_str()); return; }
    r.token = token_hex();
    g_guard.arm(ctx.port, plan.canonical, typed, request.identity, "", r.token, uptime_ms());
    r.canonical = plan.canonical; r.summary = plan.summary; r.atomic = plan.atomic; r.battery = request.identity;
  }
  r.ctx = ctx; r.stage = "armed"; r.accepted = true; r.refusal = "none"; r.risk = "irreversible"; r.has_battery = true;
  say(r);
}

void arm(const Context& ctx, bat::Op op, const json::Value& in) {
  const std::string typed = in.string_or("serial", "");
  const json::Value params = params_of(in);
  if (needs_clock(op) && !mqtt_link::clock_is_set()) { reject(ctx, bat::refusal::kClockNotSet); return; }
  const manager::BatteryPortFacts live = manager::battery_port_facts(static_cast<std::size_t>(ctx.port - 1));
  if (!live.pylontech) { reject(ctx, bat::refusal::kNotPylontech); return; }
  // a battery in its bootloader reports nothing: only a resumed update may start without readings
  if (!live.present && !(op == bat::Op::kFirmwareUpdate && params.bool_or("resume", false))) { reject(ctx, bat::refusal::kNoBattery); return; }
  std::string password;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    const char* why = g_guard.can_arm(ctx.port, uptime_ms());
    if (std::string(why) != bat::refusal::kNone) { reject(ctx, why); return; }
    g_guard.begin(ctx.port);
    password = g_password;
  }
  const std::shared_ptr<manager::BatteryJob> job = std::make_shared<manager::BatteryJob>();
  job->plan = bat::preflight_plan();
  const bool resume = op == bat::Op::kFirmwareUpdate && params.bool_or("resume", false);
  if (resume) job->plan = bat::bootloader_listen_plan();
  job->secret = password;
  const auto finish_arm = [ctx, op, typed, params, resume](const bat::RunResult& result) {
    {
      std::lock_guard<std::mutex> guard(g_lock);
      g_guard.finish(ctx.port, false, uptime_ms());
    }
    if (result.outcome != "ok" && resume) { reject(ctx, bat::refusal::kNothingToResume); return; }   // the battery is not saying it is in its bootloader
    if (resume) { arm_resume(ctx, typed, params); return; }
    if (result.outcome != "ok") {
      const char* why = bat::refusal::kPreflightFailed;
      if (result.outcome == "retry") why = bat::refusal::kNoAnswer;
      else if (result.code == bat::refusal::kBootloader) why = bat::refusal::kBootloader;
      else if (result.code == "step_failed:login") why = bat::refusal::kLoginFailed;
      else if (result.outcome == "rejected") why = result.code.c_str();
      reject(ctx, why);
      return;
    }
    const manager::BatteryPortFacts now_facts = manager::battery_port_facts(static_cast<std::size_t>(ctx.port - 1));
    bat::Facts facts;
    facts.pylontech_port = now_facts.pylontech; facts.present = now_facts.present; facts.slave = now_facts.slave; facts.current_ma = now_facts.current_ma; facts.critical_alarm = now_facts.critical_alarm;
    facts.identity = result.identity; facts.typed_serial = typed;
    const std::string pre = bat::preconditions(op, facts, params);
    if (pre != bat::refusal::kNone) { reject(ctx, pre.c_str()); return; }
    bat::Request request;
    request.op = op; request.params = params; request.identity = result.identity; request.expert_mode = g_expert_mode;
    if (needs_clock(op) && !fill_local_time(request)) { reject(ctx, bat::refusal::kClockNotSet); return; }
    bat::Plan plan;
    std::string why;
    if (!bat::build_plan(request, plan, why)) { reject(ctx, why.c_str()); return; }
    std::string token;
    {
      std::lock_guard<std::mutex> guard(g_lock);
      if (plan.uses_image) {
        const auto image = g_stage.image();
        const std::string image_problem = image ? bat::check_firmware_request(plan, *image, result.identity) : std::string(bat::refusal::kImageMissing);
        if (image_problem != bat::refusal::kNone) { reject(ctx, image_problem.c_str()); return; }
      }
      token = token_hex();
      g_guard.arm(ctx.port, plan.canonical, typed, result.identity, result.before, token, uptime_ms());
    }
    Reply r;
    r.ctx = ctx; r.stage = "armed"; r.accepted = true; r.refusal = "none"; r.token = token; r.canonical = plan.canonical; r.summary = plan.summary;
    const bat::OpInfo* info = bat::op_info(op);
    r.risk = info != nullptr ? bat::risk_name(info->risk) : "high"; r.atomic = plan.atomic;
    r.has_battery = true; r.battery = result.identity; r.before = result.before;
    say(r);
  };
  job->on_done = [finish_arm](const bat::RunResult& result) { const bat::RunResult copy = result; post([finish_arm, copy] { finish_arm(copy); }); };
  const std::string why = manager::start_battery_job(static_cast<std::size_t>(ctx.port - 1), job);
  if (!why.empty()) {
    { std::lock_guard<std::mutex> guard(g_lock); g_guard.finish(ctx.port, false, uptime_ms()); }
    reject(ctx, why == "busy" ? bat::refusal::kBusy : bat::refusal::kNotPylontech);
  }
}

// ---- execute: the token and the same operation ----------------------------------------------------------------------------------------------

void execute(const Context& ctx, bat::Op op, const json::Value& in) {
  const std::string typed = in.string_or("serial", ""), token = in.string_or("token", "");
  const json::Value params = params_of(in);
  bat::Armed armed;
  bat::Plan plan;
  std::shared_ptr<const std::vector<std::uint8_t>> image;
  std::string password;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_guard.peek(ctx.port, armed)) { reject(ctx, bat::refusal::kNotArmed); return; }
    const bool resume = op == bat::Op::kFirmwareUpdate && params.bool_or("resume", false);
    bat::Request request;
    request.op = op; request.params = params; request.identity = armed.identity; request.expert_mode = g_expert_mode; request.resume = resume;
    std::string why;
    // the conditions of the arm step are checked again: the battery may have started to move current or raised an alarm since
    if (resume) {
      bat::Request again;
      bat::Plan checked;
      const std::string refused_resume = resume_request(ctx.port, typed, params, again, checked);
      if (refused_resume != bat::refusal::kNone) { g_guard.forget(ctx.port); reject(ctx, refused_resume.c_str()); return; }
    } else {
      const manager::BatteryPortFacts now_facts = manager::battery_port_facts(static_cast<std::size_t>(ctx.port - 1));
      bat::Facts facts;
      facts.pylontech_port = now_facts.pylontech; facts.present = now_facts.present; facts.slave = now_facts.slave; facts.current_ma = now_facts.current_ma; facts.critical_alarm = now_facts.critical_alarm;
      facts.identity = armed.identity; facts.typed_serial = typed;
      const std::string pre = bat::preconditions(op, facts, params);
      if (pre != bat::refusal::kNone) { g_guard.forget(ctx.port); reject(ctx, pre.c_str()); return; }
    }
    if (needs_clock(op) && !fill_local_time(request)) { g_guard.forget(ctx.port); reject(ctx, bat::refusal::kClockNotSet); return; }
    if (!bat::build_plan(request, plan, why)) { g_guard.forget(ctx.port); reject(ctx, why.c_str()); return; }
    bat::Armed spent;
    const char* refused = g_guard.execute(ctx.port, plan.canonical, typed, token, uptime_ms(), spent);
    if (std::string(refused) != bat::refusal::kNone) { reject(ctx, refused); return; }
    if (plan.uses_image) {
      image = g_stage.image();
      const std::string problem = image ? bat::check_firmware_request(plan, *image, armed.identity) : std::string(bat::refusal::kImageMissing);
      if (problem != bat::refusal::kNone) { reject(ctx, problem.c_str()); return; }
    }
    g_guard.begin(ctx.port);
    password = g_password;
  }
  const std::shared_ptr<manager::BatteryJob> job = std::make_shared<manager::BatteryJob>();
  job->plan = plan;
  job->secret = password;
  job->image = image;
  const bool uses_image = plan.uses_image;
  job->on_done = [ctx, uses_image, plan](const bat::RunResult& result) { const bat::RunResult copy = result; post([ctx, uses_image, plan, copy] { finish_job(ctx, uses_image, plan, copy); }); };
  job->on_progress = [ctx](const bat::RunResult& result) {
    bat::RunResult copy = result;
    copy.outcome = "running";
    copy.transcript.clear(); copy.before.clear(); copy.after.clear();   // a progress report is a few numbers
    post([ctx, copy] {
      Reply r;
      r.ctx = ctx; r.stage = "progress"; r.accepted = true; r.has_result = true; r.result = copy;
      say(r);
    });
  };
  const std::string why = manager::start_battery_job(static_cast<std::size_t>(ctx.port - 1), job);
  if (!why.empty()) {
    { std::lock_guard<std::mutex> guard(g_lock); g_guard.finish(ctx.port, false, uptime_ms()); }
    reject(ctx, why == "busy" ? bat::refusal::kBusy : bat::refusal::kNotPylontech);
    return;
  }
  Reply r;
  r.ctx = ctx; r.stage = "started"; r.accepted = true; r.canonical = plan.canonical; r.summary = plan.summary;
  const bat::OpInfo* info = bat::op_info(op);
  r.risk = info != nullptr ? bat::risk_name(info->risk) : "high"; r.atomic = plan.atomic;
  say(r);
}

// ---- stage: one piece of a firmware image ----------------------------------------------------------------------------------------------------

void stage(const Context& ctx, const json::Value& in) {
  const json::Value* piece = in.get("stage");
  if (piece == nullptr || !piece->is_object()) { reject(ctx, bat::refusal::kStageInvalid); return; }
  const std::string id = piece->string_or("id", ""), data = piece->string_or("data", "");
  const long long index = piece->integer_or("index", -1, 0, 100000), total = piece->integer_or("total", 0, 1, 100000), size = piece->integer_or("size", 0, 1, 10'000'000);
  const double crc = piece->number_or("crc32", -1.0);
  if (index < 0 || total < 1 || size < 1 || crc < 0 || crc > 4294967295.0) { reject(ctx, bat::refusal::kStageInvalid); return; }
  std::lock_guard<std::mutex> guard(g_lock);
  if (g_guard.busy(ctx.port)) { reject(ctx, bat::refusal::kBusy); return; }
  if (index == 0 && heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < static_cast<std::size_t>(size) + 65536) { reject(ctx, bat::refusal::kNoMemory); return; }
  g_stage.expire(uptime_ms());
  const std::string problem = g_stage.add(id, static_cast<std::size_t>(index), static_cast<std::size_t>(total), static_cast<std::size_t>(size), static_cast<std::uint32_t>(crc), data, uptime_ms());
  if (!problem.empty()) { reject(ctx, problem.c_str()); return; }
  Reply r;
  r.ctx = ctx; r.stage = "staged"; r.accepted = true; r.received = g_stage.received();
  say(r);
}

void handle(const std::string& received) {
  // The signature of the server, when asked for: the order is read without its nonce and signature; a refused one is dropped before it can arm anything. FAIL CLOSED.
  std::string key;
  bool require = false;
  { std::lock_guard<std::mutex> guard(g_lock); key = g_command_key; require = g_require_sig; }
  std::string payload;
  const std::int64_t wall = mqtt_link::clock_is_set() ? static_cast<std::int64_t>(mqtt_link::wall_clock_ms()) : 0;
  if (!command_sig::control_authorized(require, key, received, g_nonces, wall, payload)) { ESP_LOGW(kTag, "a battery order was dropped: it does not carry a good signature"); return; }
  json::Value in;
  if (!json::parse(payload, in) || !in.is_object()) return;
  if (in.string_or("kind", "") != "solar_battery_command" || in.string_or("node_id", "") != g_node) return;
  Context ctx;
  ctx.command_id = in.string_or("command_id", "");
  ctx.action = in.string_or("action", "");
  ctx.op = in.string_or("op", "");
  ctx.port = static_cast<int>(in.integer_or("port", 0, 1, 10));
  if (!bat::valid_command_id(ctx.command_id) || ctx.port == 0) return;   // not an order this node can answer
  bool enabled = false, has_password = false;
  { std::lock_guard<std::mutex> guard(g_lock); enabled = g_enabled; has_password = !g_password.empty(); }
  if (!enabled) { reject(ctx, bat::refusal::kDisabled); return; }
  if (!node_control::working() && ctx.action != "cancel") { reject(ctx, bat::refusal::kDisabled); return; }   // paused or in standby by the server's order: no battery is written (a running operation can still be cancelled)
  if (!has_password) { reject(ctx, bat::refusal::kNoPassword); return; }
  if (ctx.action == "stage") { stage(ctx, in); return; }
  if (ctx.action == "cancel") {
    Reply r;
    r.ctx = ctx; r.stage = manager::cancel_battery_job(static_cast<std::size_t>(ctx.port - 1)) ? "cancel_requested" : "rejected";
    r.accepted = r.stage == "cancel_requested"; r.refusal = r.accepted ? "none" : bat::refusal::kNotCancellable;
    say(r);
    return;
  }
  bat::Op op = bat::Op::kPreflight;
  if (!bat::op_from_name(ctx.op, op)) { reject(ctx, bat::refusal::kUnknownOp); return; }
  if (ctx.action == "arm") arm(ctx, op, in);
  else if (ctx.action == "execute") execute(ctx, op, in);
  else reject(ctx, bat::refusal::kUnknownOp);
}

// ---- the orders of the node's own panel --------------------------------------------------------------------------------------------------------

json::Value text_value(const std::string& text) { json::Value v; v.type = json::Type::kString; v.text = text; return v; }
json::Value number_value(double number) { json::Value v; v.type = json::Type::kNumber; v.number = number; return v; }
json::Value flag_value(bool on) { json::Value v; v.type = json::Type::kBool; v.boolean = on; return v; }
void add_member(json::Value& object, const char* name, json::Value value) { object.names.emplace_back(name); object.items.push_back(std::move(value)); }

// The order as the broker would bring it.
json::Value order_of(const std::string& serial, const json::Value& params, const std::string& token) {
  json::Value in;
  in.type = json::Type::kObject;
  add_member(in, "serial", text_value(serial));
  add_member(in, "params", params);
  if (!token.empty()) add_member(in, "token", text_value(token));
  return in;
}

bool writes_on() {
  std::lock_guard<std::mutex> guard(g_lock);
  return g_enabled && g_ready != nullptr;
}

bool under_way(const std::string& stage) { return stage == "arming" || stage == "starting" || stage == "started" || stage == "running"; }

// An arm that waited too long is over. Call with g_panel_lock held.
void lapse(PanelState& state, int port) {
  PanelPort& kept = state.ports[port - 1];
  if (kept.stage != "armed" || uptime_ms() - kept.armed_ms <= bat::Guard::kArmWindowMs) return;
  kept.stage = "expired";
  kept.token.clear();
  if (kept.campaign && state.campaign.port == port) { state.campaign.lapsed("expired"); keep_campaign(); }
}

std::string arm_from_panel(const std::string& user, const std::string& session, int port, const std::string& op_name, const json::Value& given, const std::string& serial, bool campaign) {
  bat::Op op = bat::Op::kPreflight;
  if (!bat::op_from_name(op_name, op)) return bat::refusal::kUnknownOp;
  if (port < 1 || port > bat::Guard::kPorts) return "invalid_request";
  json::Value params;
  params.type = json::Type::kObject;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_enabled || g_ready == nullptr) return "battery_write_disabled";
    if (g_guard.busy(port)) return bat::refusal::kBusy;
    if (op == bat::Op::kFirmwareUpdate) {
      // the image's figures are the node's own, never values from the browser
      g_stage.expire(uptime_ms());
      const auto image = g_stage.image();
      if (!image) return bat::refusal::kImageMissing;
      g_stage.touch(uptime_ms());
      add_member(params, "image_size", number_value(static_cast<double>(image->size())));
      add_member(params, "image_crc32", number_value(static_cast<double>(g_stage.crc32())));
      add_member(params, "image_version", text_value(bp::image_version(*image)));
      if (given.bool_or("clear_eeprom", false)) add_member(params, "clear_eeprom", flag_value(true));
      if (given.bool_or("resume", false)) add_member(params, "resume", flag_value(true));
    } else if (given.is_object()) {
      params = given;
    }
  }
  Context ctx;
  ctx.command_id = token_hex().substr(0, 16);
  ctx.action = "arm"; ctx.op = op_name; ctx.port = port; ctx.panel = true; ctx.user = user; ctx.serial = serial;
  {
    std::lock_guard<std::mutex> guard(g_panel_lock);
    PanelState& state = panel_data();
    lapse(state, port);
    PanelPort& kept = state.ports[port - 1];
    if (under_way(kept.stage)) return bat::refusal::kBusy;
    if (!state.orders.allow(uptime_ms())) return "too_many_orders";
    const memory::ExternalScope in_psram;
    kept = PanelPort();
    kept.session = session; kept.user = user; kept.op = op_name; kept.serial = serial; kept.command_id = ctx.command_id; kept.stage = "arming"; kept.params = params; kept.campaign = campaign;
    if (campaign) { state.campaign.checking(); keep_campaign(); }
  }
  post([ctx, op, serial, params] { arm(ctx, op, order_of(serial, params, "")); });
  return "";
}

std::string execute_from_panel(const std::string& user, const std::string& session, int port, const std::string& serial) {
  if (port < 1 || port > bat::Guard::kPorts) return "invalid_request";
  if (!writes_on()) return "battery_write_disabled";
  Context ctx;
  std::string token;
  json::Value params;
  bat::Op op = bat::Op::kPreflight;
  {
    std::lock_guard<std::mutex> guard(g_panel_lock);
    PanelState& state = panel_data();
    lapse(state, port);
    PanelPort& kept = state.ports[port - 1];
    if (kept.stage == "expired") return bat::refusal::kExpired;
    // only the session that armed may execute: the token never left the node
    if (kept.stage != "armed" || kept.token.empty() || kept.session != session || !bat::op_from_name(kept.op, op)) return bat::refusal::kNotArmed;
    if (!state.orders.allow(uptime_ms())) return "too_many_orders";
    if (kept.campaign) {
      const std::string why = state.campaign.can_execute(serial);
      if (!why.empty()) return why;
      state.campaign.updating();
      keep_campaign();
    }
    ctx.command_id = kept.command_id; ctx.action = "execute"; ctx.op = kept.op; ctx.port = port; ctx.panel = true; ctx.user = user; ctx.serial = serial;
    token = kept.token;
    params = kept.params;
    kept.token.clear();          // one try
    kept.stage = "starting";
  }
  post([ctx, op, serial, params, token] { execute(ctx, op, order_of(serial, params, token)); });
  return "";
}

std::string cancel_from_panel(const std::string& user, int port) {
  if (port < 1 || port > bat::Guard::kPorts) return "invalid_request";
  std::string stage, op;
  {
    std::lock_guard<std::mutex> guard(g_panel_lock);
    if (!g_panel) return bat::refusal::kNotArmed;
    stage = g_panel->ports[port - 1].stage;
    op = g_panel->ports[port - 1].op;
  }
  if (stage == "armed") {
    { std::lock_guard<std::mutex> guard(g_lock); g_guard.forget(port); }
    {
      std::lock_guard<std::mutex> guard(g_panel_lock);
      PanelPort& kept = g_panel->ports[port - 1];
      kept.stage = "withdrawn";
      kept.token.clear();
      if (kept.campaign && g_panel->campaign.port == port) { g_panel->campaign.lapsed("withdrawn"); keep_campaign(); }
    }
    note(user, port, "withdrawn", op, "", "");
    return "";
  }
  if (stage == "starting" || stage == "started" || stage == "running") {
    if (!manager::cancel_battery_job(static_cast<std::size_t>(port - 1))) return bat::refusal::kNotCancellable;
    note(user, port, "withdrawn", op, "cancel_requested", "");
    return "";
  }
  return bat::refusal::kNotArmed;
}

// While writing is off there is no task of 12 KB. An order of the broker is still answered ("disabled") by a short task, one at a time: others that arrive meanwhile are dropped.
std::atomic<bool> g_refusing{false};
std::string g_refused;   // owned by the one short task

void refuse_once(void*) {
  handle(g_refused);
  g_refused.clear();
  g_refusing = false;
  vTaskDelete(nullptr);
}

// The broker's client only hands the text over: the reading and the thinking are done by a task of this file.
void on_message(const std::string& topic, const std::string& payload) {
  (void)topic;
  if (payload.size() > 4096) return;   // an order is a few hundred bytes (a piece of an image, a little over six hundred)
  if (g_ready != nullptr) { post([payload] { handle(payload); }); return; }
  if (g_refusing.exchange(true)) return;
  g_refused = payload;
  if (xTaskCreate(refuse_once, "battery-off", 8192, nullptr, 4, nullptr) != pdPASS) g_refusing = false;
}
}  // namespace

bool busy() {
  std::lock_guard<std::mutex> guard(g_lock);
  for (int port = 1; port <= bat::Guard::kPorts; ++port) if (g_guard.busy(port)) return true;
  return false;
}

void apply(const config::Settings& settings) {
  std::lock_guard<std::mutex> guard(g_lock);
  g_password = settings.battery.service_password;
  g_command_key = settings.battery.command_key;
  g_require_sig = settings.battery.require_sig;
  bat::parse_expert_mode(settings.battery.expert_mode, g_expert_mode);   // a tightening applies at once
  if (settings.battery.enabled) return;   // switching it ON still waits for the restart; switching it OFF is immediate
  g_enabled = false;
  for (int port = 1; port <= bat::Guard::kPorts; ++port) g_guard.forget(port);   // an order armed a moment ago is no longer valid
  g_stage.clear();
  std::lock_guard<std::mutex> panel(g_panel_lock);
  if (g_panel) for (PanelPort& kept : g_panel->ports) { kept.token.clear(); if (kept.stage == "armed") kept.stage = "withdrawn"; }
}

void start(const config::Settings& settings) {
  g_node = settings.node_id;
  g_enabled = settings.battery.enabled;
  g_password = settings.battery.service_password;
  g_command_key = settings.battery.command_key;
  g_require_sig = settings.battery.require_sig;
  bat::parse_expert_mode(settings.battery.expert_mode, g_expert_mode);
  if (g_node.empty()) return;
  // The 12 KB task exists only with writing switched on (switching it on needs a restart, see apply()).
  if (g_enabled) {
    g_ready = xSemaphoreCreateBinary();
    if (g_ready == nullptr || xTaskCreate(worker, "battery-ops", 12288, nullptr, 4, nullptr) != pdPASS) { ESP_LOGE(kTag, "the battery maintenance task could not be started: it stays off"); g_ready = nullptr; g_enabled = false; }
  }
  mqtt_link::subscribe("armor/solar/" + g_node + "/maintenance/command", on_message);
  ESP_LOGI(kTag, "battery maintenance %s", g_enabled ? "is ON" : "is off");
}

// ---- the panel ---------------------------------------------------------------------------------------------------------------------------------

void panel_flags(bool& enabled, bool& password) {
  std::lock_guard<std::mutex> guard(g_lock);
  enabled = g_enabled && g_ready != nullptr;
  password = !g_password.empty();
}

std::string panel_state(int port, const std::string& session) {
  if (port < 1 || port > bat::Guard::kPorts) port = 1;
  bool enabled = false, password = false;
  panel_flags(enabled, password);
  json::Writer w;
  w.begin_object().field("enabled", enabled).field("password", password).field("https", true).field("port", port);
  if (!enabled) { w.end_object(); return w.str(); }
  const manager::BatteryPortFacts facts = manager::battery_port_facts(static_cast<std::size_t>(port - 1));
  w.key("facts").begin_object().field("pylontech", facts.pylontech).field("present", facts.present).field("slave", facts.slave).field("current_ma", static_cast<long long>(facts.current_ma))
      .field("alarm", facts.critical_alarm).end_object();
  {
    std::lock_guard<std::mutex> guard(g_lock);
    g_stage.expire(uptime_ms());
    const auto image = g_stage.image();
    w.key("image");
    if (!image) w.null();
    else w.begin_object().field("board", bp::image_board(*image)).field("version", bp::image_version(*image)).field("size", static_cast<long long>(image->size())).end_object();
  }
  std::lock_guard<std::mutex> guard(g_panel_lock);
  PanelState& state = panel_data();
  lapse(state, port);
  const PanelPort& kept = state.ports[port - 1];
  w.key("operation");
  if (kept.stage.empty()) w.null();
  else {
    w.begin_object().field("stage", kept.stage).field("op", kept.op).field("user", kept.user).field("mine", kept.session == session).field("campaign", kept.campaign);
    if (kept.stage == "armed") {
      const std::uint64_t age = uptime_ms() - kept.armed_ms;
      w.field("left_s", static_cast<long long>(age < bat::Guard::kArmWindowMs ? (bat::Guard::kArmWindowMs - age) / 1000 : 0));
    }
    w.key("reply");
    if (kept.reply.empty()) w.null();
    else w.raw(kept.reply);
    w.end_object();
  }
  w.key("campaign").raw(state.campaign.to_json());
  w.end_object();
  return w.str();
}

std::string panel_audit() {
  std::string text;
  if (!store::blob_read(kAuditKey, text)) return "[]";
  bp::Audit audit;
  audit.from_json(text);
  return audit.to_json();
}

void panel_note(const std::string& user, int port, const char* event, const std::string& detail) {
  if (writes_on()) note(user, port, event, "", detail, "");
}

std::string panel_arm(const std::string& user, const std::string& session, int port, const std::string& op, const json::Value& params, const std::string& serial) {
  return arm_from_panel(user, session, port, op, params, serial, false);
}

std::string panel_execute(const std::string& user, const std::string& session, int port, const std::string& serial) { return execute_from_panel(user, session, port, serial); }

std::string panel_cancel(const std::string& user, int port) { return writes_on() ? cancel_from_panel(user, port) : std::string("battery_write_disabled"); }

std::shared_ptr<std::vector<std::uint8_t>> panel_image_buffer(std::size_t size, std::string& why) {
  why.clear();
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_enabled || g_ready == nullptr) why = "battery_write_disabled";
  else if (size < bat::kImageMinBytes || size > bat::kImageMaxBytes) why = "bad_size";
  for (int port = 1; why.empty() && port <= bat::Guard::kPorts; ++port) if (g_guard.busy(port)) why = bat::refusal::kBusy;
  // the image waits in the external memory only: without it, nothing is taken
  if (why.empty() && heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < size + 65536) why = bat::refusal::kNoMemory;
  if (!why.empty()) return nullptr;
  const memory::ExternalScope in_psram;
  return std::make_shared<std::vector<std::uint8_t>>(size);
}

std::string panel_image_keep(const std::string& user, std::shared_ptr<std::vector<std::uint8_t>> whole) {
  if (!whole) return bat::refusal::kImageMissing;
  const std::string board = bp::image_board(*whole), version = bp::image_version(*whole);
  if (board.empty()) return "image_header";
  const std::size_t size = whole->size();
  {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_enabled) return "battery_write_disabled";
    for (int port = 1; port <= bat::Guard::kPorts; ++port) if (g_guard.busy(port)) return bat::refusal::kBusy;
    if (!g_stage.keep_whole(std::move(whole), uptime_ms()).empty()) return "bad_size";
  }
  note(user, 0, "image", "firmware_update", board + " " + version + " " + std::to_string(size), "");
  return "";
}

std::string panel_campaign(const std::string& user, const std::string& session, const std::string& action, int port, const std::string& serial, int& module) {
  module = 0;
  if (!writes_on()) return "battery_write_disabled";
  if (action == "create") {
    if (port < 1 || port > bat::Guard::kPorts) return "invalid_request";
    const manager::BatteryPortFacts facts = manager::battery_port_facts(static_cast<std::size_t>(port - 1));
    if (!facts.pylontech) return bat::refusal::kNotPylontech;
    if (!facts.present || facts.slave) return bat::refusal::kNoBattery;   // the list of a stack is read from its first battery
    bp::Image image;
    {
      std::lock_guard<std::mutex> guard(g_lock);
      g_stage.expire(uptime_ms());
      const auto kept = g_stage.image();
      if (!kept) return bat::refusal::kImageMissing;
      image.board = bp::image_board(*kept); image.version = bp::image_version(*kept); image.size = kept->size(); image.crc32 = g_stage.crc32();
      g_stage.touch(uptime_ms());
    }
    const std::shared_ptr<const std::string> reading = manager::reading(static_cast<std::size_t>(port - 1));
    if (!reading) return bat::refusal::kNoBattery;
    std::lock_guard<std::mutex> guard(g_panel_lock);
    PanelState& state = panel_data();
    const memory::ExternalScope in_psram;
    const std::string why = state.campaign.create(port, user, bp::modules_from_reading(*reading), image, module);
    if (!why.empty()) { note(user, port, "campaign", "firmware_update", "refused " + why, ""); return why; }
    keep_campaign();
    note(user, port, "campaign", "firmware_update", "started " + std::to_string(state.campaign.modules.size()), "");
    return "";
  }
  std::string module_serial;
  bool resume = false;
  int at = 0;
  std::size_t size = 0;
  std::uint32_t crc = 0;
  {
    std::lock_guard<std::mutex> guard(g_panel_lock);
    PanelState& state = panel_data();
    bp::Campaign& campaign = state.campaign;
    if (!campaign.open()) return "campaign_state";
    at = campaign.port;
    lapse(state, at);
    size = campaign.image.size; crc = campaign.image.crc32;
    if (const bp::Module* m = campaign.module()) { module_serial = m->serial; resume = m->resume; module = m->number; }
    if (action == "check") {
      if (!campaign.can_check()) return "campaign_state";
    } else if (action == "retry") {
      const std::string why = campaign.retry();
      if (why.empty()) { keep_campaign(); note(user, at, "campaign", "firmware_update", "retry", module_serial); }
      return why;
    } else if (action == "finish") {
      const std::string why = campaign.finish();
      if (why.empty()) { keep_campaign(); note(user, at, "campaign", "firmware_update", "finished", ""); }
      return why;
    }
  }
  if (action == "check") {
    {
      std::lock_guard<std::mutex> guard(g_lock);
      g_stage.expire(uptime_ms());
      if (!g_stage.image() || g_stage.size() != size || g_stage.crc32() != crc) return bat::refusal::kImageMissing;   // the same image again
    }
    json::Value params;
    params.type = json::Type::kObject;
    if (resume) add_member(params, "resume", flag_value(true));
    return arm_from_panel(user, session, at, "firmware_update", params, module_serial, true);
  }
  if (action == "execute") return execute_from_panel(user, session, at, serial);
  if (action == "cancel") {
    bool armed = false;
    {
      std::lock_guard<std::mutex> guard(g_panel_lock);
      const PanelPort& kept = g_panel->ports[at - 1];
      armed = kept.campaign && kept.stage == "armed";
    }
    if (armed) cancel_from_panel(user, at);
    std::string why;
    {
      std::lock_guard<std::mutex> guard(g_panel_lock);
      why = g_panel->campaign.cancel();
      if (why.empty()) keep_campaign();
    }
    if (why.empty()) note(user, at, "campaign", "firmware_update", "cancelled", "");
    return why;
  }
  return "invalid_request";
}

}  // namespace armor::maintenance
