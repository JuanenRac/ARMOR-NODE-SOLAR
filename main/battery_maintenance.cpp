// ARMOR-NODE-SOLAR - the node's side of the battery maintenance (see battery_maintenance.hpp, core/battery_ops.hpp and docs/BATTERY_MAINTENANCE.md).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// An order arrives as a JSON message on the broker. Writing is two steps, like the switching of the electrical node: `arm` reads the battery (identity, state), checks every precondition and
// answers with a one-time token and the exact operation it will do; `execute` carries the token and the same operation, and only then is anything written. The service password is the node's
// own setting: it never appears in an answer, a log or a message. Everything is refused while the setting is off. Nothing here has run on a board.
#include "battery_maintenance.hpp"

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
#include "core/battery_stage.hpp"
#include "core/json.hpp"
#include "entropy.hpp"
#include "mqtt_link.hpp"
#include "node_control.hpp"
#include "solar_manager.hpp"

namespace armor::maintenance {
namespace {
namespace bat = armor::solar::battery;
constexpr char kTag[] = "armor-battery";

std::mutex g_lock;
bat::Guard g_guard;
bat::ImageStage g_stage;
std::string g_node;
bool g_enabled = false;
std::string g_password;

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
  bool has_result = false;
  bat::RunResult result;
  std::size_t received = 0;
};

void say(const Reply& r) {
  json::Writer w;
  w.begin_object().field("kind", "solar_battery_result").field("node_id", g_node);
  w.key("timestamp_ms").integer(static_cast<long long>(mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : uptime_ms()));
  w.field("command_id", r.ctx.command_id).field("port", r.ctx.port).field("action", r.ctx.action);
  if (!r.ctx.op.empty()) w.field("op", r.ctx.op);
  w.field("stage", r.stage).field("accepted", r.accepted).field("refusal", r.refusal);
  if (!r.token.empty()) w.field("token", r.token);
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
  mqtt_link::publish("armor/solar/" + g_node + "/maintenance/result", w.str());
}

void reject(const Context& ctx, const char* why) {
  Reply r;
  r.ctx = ctx; r.stage = "rejected"; r.accepted = false; r.refusal = why;
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

void finish_job(const Context& ctx, bool uses_image, const bat::RunResult& result) {
  {
    std::lock_guard<std::mutex> guard(g_lock);
    g_guard.finish(ctx.port, result.wrote, uptime_ms());
    if (uses_image) g_stage.clear();   // an image is used once
  }
  Reply r;
  r.ctx = ctx; r.stage = "final"; r.accepted = true; r.refusal = "none"; r.has_result = true; r.result = result;
  ESP_LOGI(kTag, "port %d: %s ended: %s %s", ctx.port, ctx.op.c_str(), result.outcome.c_str(), result.code.c_str());
  say(r);
}

// ---- arm: read the battery, check, make the token ----------------------------------------------------------------------------------------------

void arm(const Context& ctx, bat::Op op, const json::Value& in) {
  const std::string typed = in.string_or("serial", "");
  const json::Value params = params_of(in);
  if (needs_clock(op) && !mqtt_link::clock_is_set()) { reject(ctx, bat::refusal::kClockNotSet); return; }
  const manager::BatteryPortFacts live = manager::battery_port_facts(static_cast<std::size_t>(ctx.port - 1));
  if (!live.pylontech) { reject(ctx, bat::refusal::kNotPylontech); return; }
  if (!live.present) { reject(ctx, bat::refusal::kNoBattery); return; }
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
  job->secret = password;
  const auto finish_arm = [ctx, op, typed, params](const bat::RunResult& result) {
    {
      std::lock_guard<std::mutex> guard(g_lock);
      g_guard.finish(ctx.port, false, uptime_ms());
    }
    if (result.outcome != "ok") {
      const char* why = bat::refusal::kPreflightFailed;
      if (result.outcome == "retry") why = bat::refusal::kNoAnswer;
      else if (result.code == "step_failed:login") why = bat::refusal::kLoginFailed;
      else if (result.outcome == "rejected") why = result.code.c_str();
      reject(ctx, why);
      return;
    }
    const manager::BatteryPortFacts now_facts = manager::battery_port_facts(static_cast<std::size_t>(ctx.port - 1));
    bat::Facts facts;
    facts.pylontech_port = now_facts.pylontech; facts.present = now_facts.present; facts.current_ma = now_facts.current_ma; facts.critical_alarm = now_facts.critical_alarm;
    facts.identity = result.identity; facts.typed_serial = typed;
    const std::string pre = bat::preconditions(op, facts, params);
    if (pre != bat::refusal::kNone) { reject(ctx, pre.c_str()); return; }
    bat::Request request;
    request.op = op; request.params = params; request.identity = result.identity;
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
    bat::Request request;
    request.op = op; request.params = params; request.identity = armed.identity;
    std::string why;
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
  job->on_done = [ctx, uses_image](const bat::RunResult& result) { const bat::RunResult copy = result; post([ctx, uses_image, copy] { finish_job(ctx, uses_image, copy); }); };
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

void handle(const std::string& payload) {
  json::Value in;
  if (!json::parse(payload, in) || !in.is_object()) return;
  if (in.string_or("kind", "") != "solar_battery_command" || in.string_or("node_id", "") != g_node) return;
  Context ctx;
  ctx.command_id = in.string_or("command_id", "");
  ctx.action = in.string_or("action", "");
  ctx.op = in.string_or("op", "");
  ctx.port = static_cast<int>(in.integer_or("port", 0, 1, 10));
  if (ctx.command_id.size() < 8 || ctx.command_id.size() > 32 || ctx.port == 0) return;   // not an order this node can answer
  for (char c : ctx.command_id) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))) return;
  if (!g_enabled) { reject(ctx, bat::refusal::kDisabled); return; }
  if (!node_control::working() && ctx.action != "cancel") { reject(ctx, bat::refusal::kDisabled); return; }   // paused or in standby by the server's order: no battery is written (a running operation can still be cancelled)
  if (g_password.empty()) { reject(ctx, bat::refusal::kNoPassword); return; }
  if (ctx.action == "stage") { stage(ctx, in); return; }
  if (ctx.action == "cancel") {
    Reply r;
    r.ctx = ctx; r.stage = manager::cancel_battery_job(static_cast<std::size_t>(ctx.port - 1)) ? "cancel_requested" : "rejected";
    r.accepted = r.stage == "cancel_requested"; r.refusal = r.accepted ? "none" : bat::refusal::kNotCancellable;
    say(r);
    return;
  }
  bat::Op op;
  if (!bat::op_from_name(ctx.op, op)) { reject(ctx, bat::refusal::kUnknownOp); return; }
  if (ctx.action == "arm") arm(ctx, op, in);
  else if (ctx.action == "execute") execute(ctx, op, in);
  else reject(ctx, bat::refusal::kUnknownOp);
}

// The broker's client only hands the text over: the reading and the thinking are done by the task of this file.
void on_message(const std::string& topic, const std::string& payload) {
  (void)topic;
  if (payload.size() > 4096) return;   // an order is a few hundred bytes (a piece of an image, a little over six hundred)
  post([payload] { handle(payload); });
}
}  // namespace

bool busy() {
  std::lock_guard<std::mutex> guard(g_lock);
  for (int port = 1; port <= bat::Guard::kPorts; ++port) if (g_guard.busy(port)) return true;
  return false;
}

void start(const config::Settings& settings) {
  g_node = settings.node_id;
  g_enabled = settings.battery.enabled;
  g_password = settings.battery.service_password;
  if (g_node.empty()) return;
  g_ready = xSemaphoreCreateBinary();
  if (g_ready == nullptr || xTaskCreate(worker, "battery-ops", 12288, nullptr, 4, nullptr) != pdPASS) { ESP_LOGE(kTag, "the battery maintenance task could not be started: it stays off"); g_ready = nullptr; return; }
  mqtt_link::subscribe("armor/solar/" + g_node + "/maintenance/command", on_message);
  ESP_LOGI(kTag, "battery maintenance %s", g_enabled ? "is ON" : "is off");
}

}  // namespace armor::maintenance
