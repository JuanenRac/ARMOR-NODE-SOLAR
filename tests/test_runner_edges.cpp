// ARMOR-NODE-SOLAR - host tests of the edges of the runner that plays a plan on a battery's console (core/battery_runner.hpp): what is refused before anything is sent, every way a step
// can end, what is kept of the answers, and the firmware transfer when the battery says it is not ready or reports an error. The battery is a handful of strings; nothing has been tried on a real one.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/battery_ops.hpp"
#include "../core/battery_runner.hpp"

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

using namespace armor::solar::battery;

static const char* kSecret = "made-up-pass-1";

static bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

static Step send_step(const char* label, const char* line, int phase = kPhasePreflight) {
  Step step;
  step.label = label;
  step.send = line;
  step.phase = phase;
  return step;
}

static void feed(Runner& runner, const std::string& text, std::uint64_t now) {
  runner.on_rx(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), now);
}

static const std::string kPrompt = "\r\npylon_debug>";

// Plays the replies in order, one per step: the step is begun, the reply arrives. An empty reply is silence.
static RunResult play(const Plan& plan, const std::vector<std::string>& replies, std::vector<Capture>* captures = nullptr, const std::string& secret = kSecret) {
  Runner runner(plan, secret);
  std::uint64_t now = 0;
  runner.start(now);
  for (const std::string& reply : replies) {
    runner.next_tx(now);
    now += 10;
    if (!reply.empty()) feed(runner, reply, now);
  }
  runner.next_tx(now + 6000);   // after a silence it times out
  runner.next_tx(now + 6001);   // after the last step it completes
  CHECK(runner.finished());
  if (captures != nullptr) *captures = runner.captures();
  return runner.result();
}

static void test_redact() {
  CHECK(redact("hello", "") == "hello");
  CHECK(redact("a secret-1 b secret-1", "secret-1") == "a *** b ***");
  CHECK(redact("login hunter2\r\nnext", "") == "login ***\r\nnext");
  CHECK(redact("login ***", "") == "login ***");
  CHECK(redact("login \r\nx", "") == "login \r\nx");
  CHECK(redact("login a login b", "") == "login *** login ***");
  CHECK(redact("login made-up-pass-1", kSecret) == "login ***");
}

static void test_a_plan_that_is_refused_sends_nothing() {
  Plan empty;
  Runner none(empty, kSecret);
  none.start(0);
  CHECK(none.finished() && none.result().outcome == "rejected" && none.result().code == refusal::kUnknownOp && none.next_tx(1).empty());

  Plan with_password;
  Step login = send_step("login", "login {pw}");
  login.secret_in_send = true;
  with_password.steps.push_back(login);
  Runner no_secret(with_password, "");
  no_secret.start(0);
  CHECK(no_secret.finished() && no_secret.result().outcome == "rejected" && no_secret.result().code == refusal::kNoPassword);

  Plan firmware;
  firmware.uses_image = true;
  firmware.image_size = 100;
  firmware.steps.push_back(send_step("info", "info"));
  Runner no_image(firmware, kSecret, nullptr);
  no_image.start(0);
  CHECK(no_image.finished() && no_image.result().code == refusal::kImageMissing);
  const std::vector<std::uint8_t> wrong(99, 1);
  Runner wrong_size(firmware, kSecret, &wrong);
  wrong_size.start(0);
  CHECK(wrong_size.finished() && wrong_size.result().code == refusal::kImageMissing);
  std::vector<std::uint8_t> right(100, 1);
  firmware.image_crc32 = 12345;   // the right size and the wrong checksum
  Runner wrong_sum(firmware, kSecret, &right);
  wrong_sum.start(0);
  CHECK(wrong_sum.finished() && wrong_sum.result().code == refusal::kImageMissing);
  firmware.image_crc32 = crc32(right.data(), right.size());
  Runner fine(firmware, kSecret, &right);
  fine.start(0);
  CHECK(!fine.finished() && fine.can_cancel() && !fine.committed());
}

static void test_steps_that_end_well_and_the_snapshots() {
  Plan plan;
  Step info = send_step("info", "info");
  info.must_contain = {"Device"};
  plan.steps.push_back(send_step("wake", "\r"));
  plan.steps.push_back(info);
  plan.steps.push_back(send_step("write", "bat sn x", kPhaseWrite));
  plan.steps.back().write = true;
  plan.steps.push_back(send_step("info", "info", kPhaseVerify));
  std::vector<Capture> captures;
  const RunResult result = play(plan, {"\r" + kPrompt, "Device address 1" + kPrompt, "ok" + kPrompt, "Device address 1 again" + kPrompt}, &captures);
  CHECK(result.outcome == "ok" && result.code.empty() && result.steps_done == 4 && result.steps_total == 4 && result.writes_done == 1 && result.wrote);
  CHECK(captures.size() == 4 && contains(result.before, "## info") && !contains(result.before, "## wake") && contains(result.after, "## info") && !contains(result.after, "## write"));
  CHECK(contains(result.transcript, "> info") && contains(result.transcript, "< Device address 1"));

  plan.verify = [](const std::vector<Capture>&) { return std::string("not what was written"); };
  const RunResult bad = play(plan, {"\r" + kPrompt, "Device address 1" + kPrompt, "ok" + kPrompt, "Device" + kPrompt});
  CHECK(bad.outcome == "failed" && bad.code == "verify:not what was written");
  plan.verify = nullptr;

  Plan any;   // a prompt of ">" is any prompt, and another prompt is matched as text
  Step first = send_step("a", "a");
  first.prompt = ">";
  Step second = send_step("b", "b");
  second.prompt = "login:";
  any.steps = {first, second};
  CHECK(play(any, {"anything >", "ready\r\nlogin: \r\n"}).outcome == "ok");
  CHECK(play(any, {"no prompt yet"}).outcome == "retry");   // silence follows: nothing was written, asking again is safe
}

static void test_a_reply_that_is_a_failure() {
  Plan plan;
  plan.steps = {send_step("one", "one"), send_step("two", "two")};
  const RunResult invalid = play(plan, {"Invalid command or fail to excute." + kPrompt});
  CHECK(invalid.outcome == "failed" && invalid.code == "step_failed:one" && invalid.steps_done == 0);
  CHECK(play(plan, {"Unknown command" + kPrompt}).code == "step_failed:one");
  CHECK(play(plan, {"Test result:fail" + kPrompt}).code == "step_failed:one");
  plan.steps[0].tolerate_invalid = true;
  plan.steps[0].tolerate_unknown = true;
  CHECK(play(plan, {"Invalid command or fail to excute." + kPrompt, "fine" + kPrompt}).outcome == "ok");
  CHECK(play(plan, {"Unknown command" + kPrompt, "fine" + kPrompt}).outcome == "ok");
  plan.steps[0].must_contain = {"needle"};
  CHECK(play(plan, {"hay" + kPrompt}).code == "step_failed:one");
  CHECK(play(plan, {"a needle" + kPrompt, "fine" + kPrompt}).outcome == "ok");
}

static void test_silence_before_and_after_a_write() {
  Plan plan;
  plan.steps = {send_step("read", "stat")};
  const RunResult quiet = play(plan, {""});
  CHECK(quiet.outcome == "retry" && quiet.code == "timeout" && !quiet.wrote);

  plan.steps[0].write = true;
  const RunResult after_write = play(plan, {""});
  CHECK(after_write.outcome == "failed" && after_write.code == "timeout_after_write" && after_write.wrote);

  plan.steps[0].no_reply_ok = true;   // the battery switches itself off: silence is the answer
  const RunResult off = play(plan, {""});
  CHECK(off.outcome == "ok" && off.writes_done == 1);

  Plan two;
  two.steps = {send_step("write", "bat x", kPhaseWrite), send_step("then", "stat", kPhaseWrite)};
  two.steps[0].write = true;
  const RunResult partial = play(two, {"ok" + kPrompt, ""});
  CHECK(partial.outcome == "partial" && partial.writes_done == 1 && partial.code == "timeout");
  two.steps[1].phase = kPhaseVerify;   // a failure while verifying what was written is a failure, not a half-done write
  const RunResult verifying = play(two, {"ok" + kPrompt, "Invalid command or fail to excute." + kPrompt});
  CHECK(verifying.outcome == "failed" && verifying.code == "step_failed:then");
  two.steps[1].phase = kPhaseWrite;
  const RunResult broken = play(two, {"ok" + kPrompt, "Invalid command or fail to excute." + kPrompt});
  CHECK(broken.outcome == "partial" && broken.code == "step_failed:then");
}

static void test_waiting_listening_and_settling() {
  Plan plan;
  Step restart = send_step("restart", "reset");
  restart.settle_ms = 500;
  Step wait = send_step("up", "");
  wait.listen = true;
  plan.steps = {restart, wait, send_step("after", "stat")};
  Runner runner(plan, kSecret);
  runner.start(0);
  CHECK(!runner.next_tx(0).empty());                                   // "reset" goes out
  feed(runner, "bye" + kPrompt, 10);
  CHECK(runner.next_tx(100).empty());                                  // settling for 500 ms
  CHECK(runner.next_tx(499).empty() && !runner.finished());
  CHECK(runner.next_tx(600).empty());                                  // the listening step sends nothing
  feed(runner, "booting..." + kPrompt, 700);
  CHECK(!runner.next_tx(800).empty());                                 // and the next one goes out
  feed(runner, "ok" + kPrompt, 810);
  runner.next_tx(900);
  CHECK(runner.finished() && runner.result().outcome == "ok" && runner.result().steps_done == 3);

  // the prompt may already be there when the listening starts (it came with the reply before)
  Plan carried;
  Step reset = send_step("reset", "reset");
  Step listen = send_step("up", "");
  listen.listen = true;
  carried.steps = {reset, listen};
  Runner second(carried, kSecret);
  second.start(0);
  second.next_tx(0);
  feed(second, "bye" + kPrompt, 10);
  second.next_tx(20);
  feed(second, "up" + kPrompt, 30);
  second.next_tx(40);
  CHECK(second.finished() && second.result().outcome == "ok");
}

static void test_cancelling() {
  Plan plan;
  plan.steps = {send_step("a", "a", kPhaseWrite), send_step("b", "b", kPhaseWrite)};
  plan.steps[0].write = true;
  Runner early(plan, kSecret);
  early.start(0);
  CHECK(early.can_cancel());
  early.cancel();
  early.next_tx(1);
  CHECK(early.finished() && early.result().outcome == "failed" && early.result().code == "cancelled");

  Runner midway(plan, kSecret);
  midway.start(0);
  midway.next_tx(0);
  feed(midway, "ok" + kPrompt, 5);
  midway.cancel();
  midway.next_tx(10);
  CHECK(midway.finished() && midway.result().outcome == "partial" && midway.result().code == "cancelled" && midway.result().writes_done == 1);

  plan.atomic = true;
  Runner committed(plan, kSecret);
  committed.start(0);
  CHECK(committed.can_cancel());
  committed.next_tx(0);                      // the writing step is sent: from here on it is never cancelled
  CHECK(committed.committed() && !committed.can_cancel());
  committed.cancel();
  feed(committed, "ok" + kPrompt, 5);
  committed.next_tx(10);
  feed(committed, "ok" + kPrompt, 15);
  committed.next_tx(20);
  CHECK(committed.finished() && committed.result().outcome == "ok");
  committed.cancel();                        // and nothing to cancel once it is over
  CHECK(!committed.can_cancel());
}

static void test_the_password_goes_out_but_never_stays() {
  Plan plan;
  Step login = send_step("login", "login {pw}");
  login.secret_in_send = true;
  plan.steps = {login, send_step("stat", "stat")};
  Runner runner(plan, kSecret);
  runner.start(0);
  const std::vector<std::uint8_t> sent = runner.next_tx(0);
  CHECK(std::string(sent.begin(), sent.end()) == std::string("login ") + kSecret + "\r");
  feed(runner, std::string("login ") + kSecret + "\r\nwelcome" + kPrompt, 5);
  runner.next_tx(10);
  feed(runner, "ok" + kPrompt, 15);
  runner.next_tx(20);
  CHECK(runner.result().outcome == "ok" && !contains(runner.result().transcript, kSecret) && contains(runner.result().transcript, "login ***"));
  for (const Capture& capture : runner.captures()) CHECK(!contains(capture.text, kSecret));
}

static void test_long_answers_are_bounded() {
  Plan plan;
  plan.steps = {send_step("a", "a"), send_step("b", "b")};
  Runner runner(plan, kSecret);
  runner.start(0);
  runner.next_tx(0);
  feed(runner, std::string(Runner::kMaxAnswer + 500, 'x'), 5);   // longer than is kept, and no prompt
  runner.next_tx(7000);                                          // the step times out
  CHECK(runner.finished() && runner.result().truncated);

  Plan notes;
  for (int i = 0; i < 6; ++i) notes.steps.push_back(send_step("n", "n"));
  Runner many(notes, kSecret);
  many.start(0);
  std::uint64_t now = 0;
  for (int i = 0; i < 6; ++i) {
    many.next_tx(now);
    now += 10;
    feed(many, std::string(2000, 'y') + kPrompt, now);
  }
  many.next_tx(now + 10);
  CHECK(many.finished() && many.result().truncated && many.result().transcript.size() < 2 * Runner::kMaxTranscript + 4000);
}

// ---- the transfer of a firmware image ---------------------------------------------------------------------------------------------------------

static Plan firmware_plan(const std::vector<std::uint8_t>& image) {
  Plan plan;
  plan.op = Op::kPreflight;
  plan.atomic = true;
  plan.uses_image = true;
  plan.image_size = image.size();
  plan.image_crc32 = crc32(image.data(), image.size());
  Step update;
  update.kind = StepKind::kUpdata;
  update.label = "updata";
  update.phase = kPhaseWrite;
  update.write = true;
  plan.steps = {update};
  return plan;
}

static void test_firmware_transfer_edges() {
  const std::vector<std::uint8_t> image(100, 7);
  const Plan plan = firmware_plan(image);

  Runner never(plan, kSecret, &image);   // the bootloader never says it is ready
  never.start(0);
  const std::vector<std::uint8_t> sent = never.next_tx(0);
  CHECK(std::string(sent.begin(), sent.end()) == "updata\r" && never.committed() && !never.can_cancel());
  CHECK(never.next_tx(10'000).empty() && !never.finished());
  never.next_tx(16'000);
  CHECK(never.finished() && never.result().outcome == "partial" && never.result().code == "firmware:bootloader_not_ready");

  Runner earlier(plan, kSecret, &image);   // it says the last update failed
  earlier.start(0);
  earlier.next_tx(0);
  feed(earlier, "Updata error last time, Please updata again\r\n", 5);
  CHECK(earlier.finished() && earlier.result().outcome == "partial" && earlier.result().code == "firmware:battery_reported_error");

  Runner reports(plan, kSecret, &image);   // it reports an error during the transfer
  reports.start(0);
  reports.next_tx(0);
  feed(reports, "Program start\r\nC", 5);
  CHECK(!reports.finished());
  feed(reports, "Update Error", 10);
  CHECK(reports.finished() && reports.result().code == "firmware:battery_reported_error");

  Runner silent(plan, kSecret, &image);   // the image goes through but the battery never says it is done
  silent.start(0);
  silent.next_tx(0);
  feed(silent, "Program start\r\nC", 5);
  std::uint64_t now = 5;
  for (int guard = 0; guard < 10 && !silent.finished(); ++guard) {
    const std::vector<std::uint8_t> out = silent.next_tx(now);
    if (out.empty()) break;
    const std::uint8_t ack = 0x06;
    silent.on_rx(&ack, 1, now += 5);
  }
  CHECK(!silent.finished() && silent.result().packets_total == 1);
  silent.next_tx(now + 6'000);
  CHECK(silent.finished() && silent.result().code == "firmware:no_end_flag" && silent.result().outcome == "partial");

  Runner whole(plan, kSecret, &image);   // and the whole way, the battery saying it is done
  whole.start(0);
  whole.next_tx(0);
  feed(whole, "Program start\r\n", 5);
  feed(whole, "C", 6);
  now = 6;
  for (int guard = 0; guard < 10 && !whole.finished(); ++guard) {
    const std::vector<std::uint8_t> out = whole.next_tx(now);
    if (out.empty()) break;
    const std::uint8_t ack = 0x06;
    whole.on_rx(&ack, 1, now += 5);
  }
  feed(whole, "\r\nUpdata OK\r\n", now + 10);
  whole.next_tx(now + 20);
  CHECK(whole.finished() && whole.result().outcome == "ok" && whole.result().packets_sent == 1);
}

int main() {
  test_redact();
  test_a_plan_that_is_refused_sends_nothing();
  test_steps_that_end_well_and_the_snapshots();
  test_a_reply_that_is_a_failure();
  test_silence_before_and_after_a_write();
  test_waiting_listening_and_settling();
  test_cancelling();
  test_the_password_goes_out_but_never_stays();
  test_long_answers_are_bounded();
  test_firmware_transfer_edges();
  std::printf("runner edges: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
