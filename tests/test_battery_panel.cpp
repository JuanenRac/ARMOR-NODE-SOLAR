// ARMOR-NODE-SOLAR - the rules of writing to a battery from the node's own panel (core/battery_panel.hpp): who may ask, how often, the record, the image kept and the campaign on a stack.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../core/battery_panel.hpp"
#include "../core/battery_stage.hpp"

namespace bat = armor::solar::battery;
namespace bp = armor::solar::battery::panel;

static int g_checks = 0, g_failed = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { ++g_failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

// Made-up serial numbers: characters 8 and 9 say the chip.
static const char* kOlderA = "PPTBH01111100001";
static const char* kOlderB = "PPTBH01111100002";
static const char* kNewer = "PPTBH01E21100003";

static bp::Module module(int n, const char* serial, const char* model = "US3000C", const char* chip = "older", const char* board = "") {
  bp::Module m;
  m.number = n; m.serial = serial; m.model = model; m.chip = chip; m.board = board;
  return m;
}

static bp::Image image_older() { bp::Image i; i.board = "US2000C"; i.version = "3.1"; i.size = 100000; i.crc32 = 7; return i; }

static bp::Campaign started() {
  bp::Campaign c;
  int at = 0;
  CHECK(c.create(2, "admin", {module(1, kOlderA), module(2, kOlderB)}, image_older(), at) == "");
  return c;
}

static bat::Identity seen(const char* serial, const char* model = "US3000C") {
  bat::Identity id;
  id.valid = true; id.model = model; id.serial = serial;
  return id;
}

static void test_gate() {
  bp::Gate g;
  CHECK(bp::gate_refusal(g) == "forbidden");
  g.admin = true;
  CHECK(bp::gate_refusal(g) == "https_required");
  g.https = true;
  CHECK(bp::gate_refusal(g) == "battery_write_disabled");   // off unless the owner turned it on
  g.enabled = true;
  CHECK(bp::gate_refusal(g) == "no_password");
  g.password = true;
  CHECK(bp::gate_refusal(g) == "");
  g.working = false;
  CHECK(bp::gate_refusal(g) == "node_paused");

  bp::OrderLimit limit;
  for (unsigned i = 0; i < bp::OrderLimit::kPerMinute; ++i) CHECK(limit.allow(1000 + i));
  CHECK(!limit.allow(2000));
  CHECK(!limit.allow(1000 + bp::OrderLimit::kWindowMs - 1));
  CHECK(limit.allow(1000 + bp::OrderLimit::kWindowMs));
}

static void test_image() {
  std::vector<std::uint8_t> short_image(100, 0);
  CHECK(bp::image_version(short_image) == "");
  CHECK(bp::image_board(short_image) == "");
  auto whole = std::make_shared<std::vector<std::uint8_t>>(bat::kImageMinBytes, 0x11);
  (*whole)[bat::kImageVersionOffset] = 4;
  (*whole)[bat::kImageVersionOffset + 1] = 3;
  const std::string name = "US2000C_NF4";
  for (std::size_t i = 0; i < bat::kImageBoardSize; ++i) (*whole)[bat::kImageBoardOffset + i] = i < name.size() ? static_cast<std::uint8_t>(name[i]) : 0;
  CHECK(bp::image_version(*whole) == "3.4");
  CHECK(bp::image_board(*whole) == "US2000C_NF4");

  bat::ImageStage stage;
  CHECK(stage.keep_whole(nullptr, 0) == "stage_size");
  CHECK(stage.keep_whole(std::make_shared<std::vector<std::uint8_t>>(10, 0), 0) == "stage_size");
  CHECK(stage.keep_whole(std::make_shared<std::vector<std::uint8_t>>(bat::kImageMaxBytes + 1, 0), 0) == "stage_size");
  CHECK(!stage.ready());
  CHECK(stage.keep_whole(whole, 1000) == "");
  CHECK(stage.ready() && stage.size() == bat::kImageMinBytes && stage.crc32() == bat::crc32(whole->data(), whole->size()));
  CHECK(stage.keep_whole(std::make_shared<std::vector<std::uint8_t>>(10, 0), 2000) == "stage_size" && stage.ready());   // a refused one drops nothing
  stage.expire(1000 + bat::ImageStage::kKeepMs);
  CHECK(stage.ready());
  stage.touch(1000 + bat::ImageStage::kKeepMs);
  stage.expire(1001 + bat::ImageStage::kKeepMs);
  CHECK(stage.ready());
  stage.expire(1001 + 2 * bat::ImageStage::kKeepMs);
  CHECK(!stage.ready() && stage.image() == nullptr);
}

static void test_audit() {
  bp::Audit audit;
  audit.add({10, "admin", 2, "armed", "set_clock", "", kOlderA});
  CHECK(audit.entries().size() == 1);
  bp::Audit again;
  again.from_json(audit.to_json());
  CHECK(again.entries().size() == 1 && again.entries()[0].user == "admin" && again.entries()[0].port == 2 && again.entries()[0].at_s == 10 && again.entries()[0].serial == kOlderA);
  for (int i = 0; i < 100; ++i) audit.add({static_cast<std::uint64_t>(i), std::string(200, 'u'), 10, std::string(50, 'e'), std::string(80, 'o'), std::string(300, 'd'), std::string(90, 's')});
  CHECK(audit.entries().size() <= bp::Audit::kCapacity);
  CHECK(audit.to_json().size() <= bp::Audit::kMaxBytes);   // it fits one blob of the flash
  CHECK(audit.entries().back().at_s == 99 && audit.entries().back().user.size() == 32 && audit.entries().back().detail.size() == 48);
  again.from_json("not json");
  CHECK(again.entries().empty());
  again.from_json("{}");
  CHECK(again.entries().empty());
}

static void test_rules() {
  CHECK(bp::expected_board("US3000C", "older", "3.1") == "US2000C");
  CHECK(bp::expected_board("UP5000", "newer", "3.1") == "US2000C_NF4");
  CHECK(bp::expected_board("US5000", "older", "1.2") == "US5000");
  CHECK(bp::expected_board("US5000", "newer", "1.9") == "E32000C_NF4");
  CHECK(bp::expected_board("US5000", "newer", "2.0") == "US2000C_NF4");
  CHECK(bp::expected_board("US3000C", "unknown", "3.1") == "");   // a doubtful chip: nothing
  CHECK(bp::expected_board("US2KBPL", "older", "3.1") == "");     // the US2000 Plus never gets firmware
  CHECK(bp::module_problem(module(1, kOlderA), image_older()) == "");
  CHECK(bp::module_problem(module(1, ""), image_older()) == "serial_unreadable");
  CHECK(bp::module_problem(module(1, kOlderA, "US2KBPL"), image_older()) == "unsupported_model");
  CHECK(bp::module_problem(module(1, kOlderA, "US3000C", "unknown"), image_older()) == "chip_unknown");
  CHECK(bp::module_problem(module(1, kNewer, "US3000C", "newer"), image_older()) == "image_mismatch");

  const std::string reading = std::string("{\"kind\":\"battery\",\"model\":\"US3000C\",\"stack\":[{\"n\":1,\"present\":true,\"serial\":\"") + kOlderA +
                              "\",\"chip\":\"older\"},{\"n\":2,\"present\":true,\"model\":\"US2000C\",\"serial\":\"" + kNewer + "\",\"board\":\"NF4.E2\",\"chip\":\"newer\"},{\"n\":3,\"present\":false}]}";
  const std::vector<bp::Module> list = bp::modules_from_reading(reading);
  CHECK(list.size() == 2 && list[0].model == "US3000C" && list[0].serial == kOlderA && list[1].model == "US2000C" && list[1].board == "NF4.E2" && list[1].chip == "newer");
  CHECK(bp::modules_from_reading("{\"kind\":\"inverter\"}").empty());
  CHECK(bp::modules_from_reading("nonsense").empty());
  // a module that does not say its chip is "unknown", and blocks
  CHECK(bp::modules_from_reading("{\"kind\":\"battery\",\"model\":\"US3000C\",\"stack\":[{\"n\":1,\"present\":true}]}")[0].chip == "unknown");
}

static void test_campaign_blocked() {
  bp::Campaign c;
  int at = 0;
  CHECK(c.create(0, "admin", {module(1, kOlderA)}, image_older(), at) == "invalid_request");
  CHECK(c.create(1, "admin", {module(1, kOlderA)}, bp::Image(), at) == "image_missing");
  CHECK(c.create(1, "admin", {}, image_older(), at) == "no_battery");
  CHECK(c.create(1, "admin", {module(1, kOlderA), module(2, kNewer, "US3000C", "unknown")}, image_older(), at) == "chip_unknown" && at == 2);
  CHECK(c.create(1, "admin", {module(1, kOlderA), module(2, kNewer, "US3000C", "newer")}, image_older(), at) == "image_mismatch" && at == 2);
  CHECK(c.create(1, "admin", {module(1, kOlderA), module(2, kOlderA)}, image_older(), at) == "serial_repeated");
  CHECK(c.state.empty() && !c.open() && c.to_json() == "null");   // blocked: nothing was started
  CHECK(c.create(1, "admin", {module(1, kOlderA)}, image_older(), at) == "");
  CHECK(c.create(1, "admin", {module(1, kOlderB)}, image_older(), at) == "campaign_open");
}

static void test_campaign_flow() {
  bp::Campaign c = started();
  CHECK(c.state == "active" && c.can_check() && c.same_image(100000, 7) && !c.same_image(100000, 8));
  CHECK(c.can_execute(kOlderA) == "campaign_state");   // checked first
  c.checking();
  CHECK(!c.can_check());
  c.armed(seen(kOlderA));
  CHECK(c.module()->status == "ready" && c.can_check());
  CHECK(c.can_execute(kOlderB) == "serial_mismatch" && c.can_execute(kOlderA) == "");
  c.updating();
  CHECK(c.cancel() == "busy");   // a module being written is not cut
  c.ended("ok", "");
  CHECK(c.modules[0].status == "done" && c.current == 1 && c.state == "active");
  CHECK(c.finish() == "campaign_state");
  c.checking(); c.armed(seen(kOlderB)); c.updating(); c.ended("ok", "");
  CHECK(c.state == "return_cable" && c.open());
  CHECK(c.finish() == "" && c.state == "done" && !c.open() && c.cancel() == "campaign_state");

  // another battery on the cable: the campaign stops
  c = started();
  c.checking(); c.armed(seen(kOlderB));
  CHECK(c.state == "stopped" && c.stop_reason == "module_differs" && c.can_execute(kOlderA) == "campaign_state");
}

static void test_campaign_failures() {
  bp::Campaign c = started();
  c.checking(); c.refused("not_idle");
  CHECK(c.state == "active" && c.module()->status == "pending" && c.module()->code == "not_idle");   // nothing written: it may be checked again
  c.checking(); c.lapsed("expired");
  CHECK(c.module()->status == "pending" && c.module()->code == "expired");
  c.checking(); c.refused("chip_unknown");
  CHECK(c.state == "stopped" && c.module()->status == "failed");
  CHECK(c.retry() == "" && c.state == "active" && !c.module()->resume);

  c.checking(); c.armed(seen(kOlderA)); c.updating(); c.ended("partial", "firmware:battery_reported_error");
  CHECK(c.state == "stopped" && c.stop_reason == "firmware:battery_reported_error" && c.current == 0);
  CHECK(c.retry() == "" && c.module()->resume);   // the battery may be in its bootloader: the same image again
  c.checking(); c.armed(seen(""));                 // it cannot say who it is then
  CHECK(c.module()->status == "ready");
  c.updating(); c.refused("bad_token");
  CHECK(c.state == "stopped");
  CHECK(c.retry() == "");
  c.checking(); c.refused("nothing_to_resume");
  CHECK(c.state == "stopped" && c.retry() == "" && !c.module()->resume);
  CHECK(c.cancel() == "" && c.state == "cancelled" && c.retry() == "campaign_state");
}

static void test_campaign_kept() {
  bp::Campaign c = started();
  c.checking(); c.armed(seen(kOlderA)); c.updating();
  bp::Campaign again;
  CHECK(again.from_json(c.to_json()));
  CHECK(again.state == "active" && again.port == 2 && again.modules.size() == 2 && again.modules[0].serial == kOlderA && again.modules[0].status == "updating" && again.same_image(100000, 7) && again.image.board == "US2000C" && again.user == "admin");
  again.restarted();   // the node started again in the middle of a module
  CHECK(again.state == "stopped" && again.stop_reason == "operation_lost" && again.modules[0].status == "failed");
  c = started();
  c.checking(); c.armed(seen(kOlderA));
  c.restarted();
  CHECK(c.state == "active" && c.module()->status == "pending" && c.module()->code == "operation_lost");
  CHECK(!again.from_json("null") && again.state.empty());
  CHECK(!again.from_json("{\"state\":\"nonsense\"}") && again.state.empty());
}

int main() {
  test_gate();
  test_image();
  test_audit();
  test_rules();
  test_campaign_blocked();
  test_campaign_flow();
  test_campaign_failures();
  test_campaign_kept();
  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
