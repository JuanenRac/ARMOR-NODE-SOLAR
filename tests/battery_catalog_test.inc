// The node's tables against the shared table (ARMOR-COMMON/src/armor_common/battery_ops.json): the node, the server and the clients must say the same.
#include <cstdlib>
#include <fstream>
#include <sstream>

static void test_the_shared_table() {
  std::string dir = __FILE__;
  const std::size_t slash = dir.find_last_of("/\\");
  dir = slash == std::string::npos ? "." : dir.substr(0, slash);
  std::ifstream file(dir + "/../../ARMOR-COMMON/src/armor_common/battery_ops.json");
  if (!file) {
    const char* required = std::getenv("ARMOR_REQUIRE_SIBLINGS");
    if (required != nullptr && std::string(required) != "" && std::string(required) != "0") { ++checks; ++failures; std::printf("FAIL the shared table (ARMOR-COMMON) is not next to this repository\n"); }
    else std::printf("SKIPPED the shared table (ARMOR-COMMON is not next to this repository)\n");
    return;
  }
  std::stringstream text;
  text << file.rdbuf();
  json::Value shared;
  CHECK(json::parse(text.str(), shared));
  const json::Value* ops = shared.get("ops");
  CHECK(ops != nullptr && ops->is_array());
  if (ops == nullptr || !ops->is_array()) return;
  std::size_t listed = 0;
  for (const OpInfo& i : catalog()) {
    if (i.op == Op::kPreflight) continue;
    ++listed;
    const json::Value* match = nullptr;
    for (const json::Value& o : ops->items) if (o.string_or("id", "") == i.name) match = &o;
    CHECK(match != nullptr);
    if (match == nullptr) continue;
    CHECK(match->string_or("risk", "") == risk_name(i.risk));
    CHECK(match->bool_or("needs_idle", !i.needs_idle) == i.needs_idle && match->bool_or("alarm_ok", !i.alarm_ok) == i.alarm_ok && match->bool_or("atomic", !i.atomic) == i.atomic);
  }
  CHECK(ops->items.size() == listed);
  const json::Value* models_shared = shared.get("models");
  CHECK(models_shared != nullptr && models_shared->items.size() == models().size());
  for (const ModelProfile& m : models()) {
    bool found = false;
    if (models_shared != nullptr) for (const json::Value& s : models_shared->items) {
      if (s.string_or("name", "") != m.name) continue;
      found = true;
      CHECK(s.integer_or("official_ah", -1, 0, 1000) == m.official_ah && s.bool_or("firmware", !m.firmware) == m.firmware);
    }
    CHECK(found);
  }
  const json::Value* groups = shared.get("limit_groups");
  CHECK(groups != nullptr && groups->items.size() == limit_groups().size());
  for (const LimitGroup& g : limit_groups()) {
    const json::Value* match = nullptr;
    if (groups != nullptr) for (const json::Value& s : groups->items) if (s.string_or("name", "") == g.name) match = &s;
    CHECK(match != nullptr);
    if (match == nullptr) continue;
    CHECK(match->integer_or("lowest", 1'000'000, -1'000'000, 1'000'000) == g.lowest && match->integer_or("highest", -1'000'000, -1'000'000, 1'000'000) == g.highest);
    const json::Value* keys = match->get("keys");
    const json::Value* orders = match->get("orders");
    CHECK(keys != nullptr && keys->items.size() == g.keys.size() && orders != nullptr && orders->items.size() == g.orders.size());
    if (keys == nullptr || orders == nullptr || keys->items.size() != g.keys.size() || orders->items.size() != g.orders.size()) continue;
    for (std::size_t k = 0; k < g.keys.size(); ++k) CHECK(keys->items[k].items.size() == 2 && keys->items[k].items[0].text == g.keys[k].key && keys->items[k].items[1].text == g.keys[k].wire);
    for (std::size_t k = 0; k < g.orders.size(); ++k) CHECK(orders->items[k].items.size() == 2 && orders->items[k].items[0].text == g.orders[k].low && orders->items[k].items[1].text == g.orders[k].high);
  }
  const json::Value* protections = shared.get("protections");
  CHECK(protections != nullptr && protections->items.size() == protection_names().size());
  if (protections != nullptr && protections->items.size() == protection_names().size()) for (std::size_t k = 0; k < protection_names().size(); ++k) CHECK(protections->items[k].text == protection_names()[k]);
  const json::Value* blocked = shared.get("expert_blocked");
  CHECK(blocked != nullptr && blocked->items.size() == expert_blocked_words().size());
  if (blocked != nullptr && blocked->items.size() == expert_blocked_words().size()) for (std::size_t k = 0; k < expert_blocked_words().size(); ++k) CHECK(blocked->items[k].text == expert_blocked_words()[k]);
  const json::Value* limits = shared.get("limits");
  CHECK(limits != nullptr);
  if (limits != nullptr) {
    CHECK(limits->integer_or("idle_current_ma", -1, 0, 1'000'000) == kIdleCurrentMa);
    CHECK(limits->integer_or("arm_window_s", -1, 0, 100000) * 1000 == static_cast<long long>(Guard::kArmWindowMs));
    CHECK(limits->integer_or("cooldown_s", -1, 0, 100000) * 1000 == static_cast<long long>(Guard::kCooldownMs));
    CHECK(limits->integer_or("image_min_bytes", -1, 0, 100'000'000) == static_cast<long long>(kImageMinBytes) && limits->integer_or("image_max_bytes", -1, 0, 100'000'000) == static_cast<long long>(kImageMaxBytes));
    CHECK(limits->integer_or("chunk_bytes", -1, 0, 100000) == static_cast<long long>(ImageStage::kMaxChunkBytes) && limits->integer_or("max_chunks", -1, 0, 100000) == static_cast<long long>(ImageStage::kMaxChunks));
  }
  // every refusal the node can give is in the shared list
  const json::Value* refusals = shared.get("refusals");
  CHECK(refusals != nullptr);
  if (refusals != nullptr) {
    for (const char* r : {refusal::kNone, refusal::kDisabled, refusal::kInvalidParams, refusal::kUnknownOp, refusal::kNotPylontech, refusal::kNoBattery, refusal::kUnknownModel, refusal::kUnknownFirmware, refusal::kSerialMismatch,
                          refusal::kSerialUnreadable, refusal::kNotIdle, refusal::kCriticalAlarm, refusal::kBusy, refusal::kCooldown, refusal::kLocked, refusal::kNotArmed, refusal::kBadToken, refusal::kExpired, refusal::kChipUnknown,
                          refusal::kImageMissing, refusal::kImageMismatch, refusal::kUnsupportedModel, refusal::kNoOfficialValue, refusal::kRiskNotAccepted, refusal::kExpertBlocked, refusal::kNoPassword, refusal::kBatteryChanged,
                          refusal::kClockNotSet, refusal::kNotCancellable, refusal::kNoMemory, refusal::kNoAnswer, refusal::kLoginFailed, refusal::kPreflightFailed, refusal::kStageInvalid, refusal::kStageOrder, refusal::kStageData,
                          refusal::kStageSize, refusal::kStageCrc}) {
      bool found = false;
      for (const json::Value& s : refusals->items) if (s.text == r) found = true;
      CHECK(found);
    }
    CHECK(refusals->items.size() == 38);
  }
}
