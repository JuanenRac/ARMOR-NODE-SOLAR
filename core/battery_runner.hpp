// ARMOR-NODE-SOLAR - runs a plan (battery_ops.hpp) on a Pylontech battery's console, line by line, and says how it went.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Like the read-only console probe, it touches no hardware: the firmware sends what `next_tx` returns and gives `on_rx` what arrives. Each step is sent once and waits for the battery's prompt;
// the answer is checked (a line the battery refuses is a failure unless the plan expected it), kept, and the next step follows after its pause. The service password never leaves this object
// in clear: every text that is kept (the captures and the transcript) has it replaced, and so does the text of a `login` line whatever its argument.
//
// The result is one of: ok; rejected (nothing was sent: the plan was refused before); failed (a step failed before anything was written, or what was read back differs); partial (a sequence
// that writes stopped midway - the state of the battery is whatever the transcript shows); retry (the battery did not answer before anything was written: asking again is safe).
//
// An operation that is atomic (EEPROM, serial number, firmware) is never cancelled once it has started to write; the others may be cancelled between two steps.
// Nothing here has run against a real battery.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "battery_ops.hpp"
#include "xmodem.hpp"

namespace armor::solar::battery {

struct RunResult {
  std::string outcome = "failed";   // ok, rejected, failed, partial, retry
  std::string code;                 // why, when it is not ok: timeout, step_failed:<label>, verify:<why>, cancelled, firmware:<why> ...
  std::size_t steps_done = 0, steps_total = 0;
  std::size_t writes_done = 0;
  bool wrote = false;               // at least one writing step was sent
  bool truncated = false;
  std::string before, after;        // the reads before and after the writes (redacted)
  std::string transcript;           // what was sent and received (redacted), bounded
  Identity identity;                // as read by the first info
  std::size_t packets_sent = 0, packets_total = 0;
};

inline std::string redact(std::string text, const std::string& secret) {
  if (!secret.empty()) {
    std::size_t at = 0;
    while ((at = text.find(secret, at)) != std::string::npos) { text.replace(at, secret.size(), "***"); at += 3; }
  }
  // whatever follows `login ` on a line is a password
  std::size_t from = 0;
  while ((from = text.find("login ", from)) != std::string::npos) {
    const std::size_t begin = from + 6;
    std::size_t end = begin;
    while (end < text.size() && text[end] != '\r' && text[end] != '\n' && text[end] != ' ') ++end;
    if (end > begin && text.compare(begin, end - begin, "***") != 0) text.replace(begin, end - begin, "***");
    from = begin + 3;
  }
  return text;
}

class Runner {
 public:
  static constexpr std::size_t kMaxAnswer = 12'288, kMaxTranscript = 6'000, kMaxSnapshot = 2'400;

  Runner(const Plan& plan, std::string secret, const std::vector<std::uint8_t>* image = nullptr) : plan_(plan), secret_(std::move(secret)), image_(image) {
    result_.steps_total = plan.steps.size();
  }

  void start(std::uint64_t now_ms) {
    state_ = State::kNext;
    settle_until_ms_ = now_ms;
    if (plan_.uses_image && (image_ == nullptr || image_->size() != plan_.image_size || crc32(image_->data(), image_->size()) != plan_.image_crc32)) finish_rejected(refusal::kImageMissing);
    else if (plan_.steps.empty()) finish_rejected(refusal::kUnknownOp);
    else if (needs_password() && secret_.empty()) finish_rejected(refusal::kNoPassword);
  }

  bool finished() const { return state_ == State::kDone; }
  const RunResult& result() const { return result_; }
  bool committed() const { return plan_.atomic && result_.wrote; }
  // Whether a request to cancel would be honoured at the next boundary.
  bool can_cancel() const { return !finished() && !(plan_.atomic && result_.wrote); }
  void cancel() { if (can_cancel()) cancel_requested_ = true; }
  const std::vector<Capture>& captures() const { return captures_; }

  std::vector<std::uint8_t> next_tx(std::uint64_t now_ms) {
    if (state_ == State::kDone) return {};
    switch (state_) {
      case State::kSettle:
        if (now_ms < settle_until_ms_) return {};
        state_ = State::kNext;
        [[fallthrough]];
      case State::kNext: return begin_step(now_ms);
      case State::kWaitReply:
        if (now_ms - step_started_ms_ > step().timeout_ms) { if (step().no_reply_ok) step_ok(now_ms); else fail("timeout"); }
        return {};
      case State::kUpdWaitProgram:
        if (now_ms - step_started_ms_ > 15'000) fail("firmware:bootloader_not_ready");
        return {};
      case State::kUpdXfer: {
        std::vector<std::uint8_t> out = sender_->next_tx(now_ms);
        result_.packets_sent = sender_->sent();
        if (sender_->failed()) fail(std::string("firmware:") + sender_->error());
        else if (sender_->done()) { state_ = State::kUpdWaitEnd; step_started_ms_ = now_ms; check_update_text(now_ms); }
        return out;
      }
      case State::kUpdWaitEnd:
        if (now_ms - step_started_ms_ > xmodem::kFinishWaitMs) fail("firmware:no_end_flag");
        return {};
      default: return {};
    }
  }

  void on_rx(const std::uint8_t* data, std::size_t length, std::uint64_t now_ms) {
    if (state_ == State::kDone || length == 0) return;
    if (state_ == State::kUpdXfer && sender_) {
      sender_->on_rx(data, length, now_ms);
      result_.packets_sent = sender_->sent();
      for (std::size_t i = 0; i < length; ++i) if (data[i] >= 0x20 && data[i] < 0x7F) append_rx(static_cast<char>(data[i]));
      if (has_update_error()) { fail("firmware:battery_reported_error"); return; }
      if (sender_->failed()) fail(std::string("firmware:") + sender_->error());
      else if (sender_->done()) { state_ = State::kUpdWaitEnd; step_started_ms_ = now_ms; check_update_text(now_ms); }
      return;
    }
    for (std::size_t i = 0; i < length; ++i) append_rx(static_cast<char>(data[i]));
    switch (state_) {
      case State::kWaitReply: check_reply(now_ms); break;
      case State::kUpdWaitProgram:
        if (has_update_error()) { fail("firmware:battery_reported_error"); break; }
        if (contains(rx_, "Program start")) {
          note("< bootloader ready");
          sender_ = std::make_unique<xmodem::Sender>(*image_);
          sender_->begin(now_ms);
          result_.packets_total = sender_->total();
          // 'C' may have come with the banner
          state_ = State::kUpdXfer;
          const std::size_t line_end = rx_.find(static_cast<char>(10), rx_.find("Program start"));
          if (line_end != std::string::npos) {
            const std::size_t k = rx_.find('C', line_end);
            if (k != std::string::npos) { const std::uint8_t c = 'C'; sender_->on_rx(&c, 1, now_ms); }
          }
          step_started_ms_ = now_ms;
        }
        break;
      case State::kUpdWaitEnd: check_update_text(now_ms); break;
      default: break;
    }
  }

 private:
  enum class State { kNext, kWaitReply, kSettle, kUpdWaitProgram, kUpdXfer, kUpdWaitEnd, kDone };

  static bool contains(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }
  bool needs_password() const {
    for (const Step& s : plan_.steps) if (s.secret_in_send) return true;
    return false;
  }
  const Step& step() const { return plan_.steps[index_]; }
  void append_rx(char c) { if (rx_.size() < kMaxAnswer) rx_ += c; else result_.truncated = true; }

  void note(const std::string& line) {
    if (transcript_.size() >= kMaxTranscript) { result_.truncated = true; return; }
    transcript_ += redact(line, secret_);
    transcript_ += '\n';
  }

  std::vector<std::uint8_t> begin_step(std::uint64_t now_ms) {
    if (cancel_requested_ && can_cancel()) { fail("cancelled"); return {}; }
    if (index_ >= plan_.steps.size()) { complete(); return {}; }
    const Step& s = step();
    step_started_ms_ = now_ms;
    if (!carry_.empty() && !s.send.empty()) carry_.clear();
    rx_ = (s.listen ? carry_ : std::string());
    carry_.clear();
    if (s.kind == StepKind::kUpdata) {
      result_.wrote = true;
      state_ = State::kUpdWaitProgram;
      note("> updata");
      return {'u', 'p', 'd', 'a', 't', 'a', '\r'};
    }
    if (s.listen) {
      state_ = State::kWaitReply;
      note("> (waiting for the battery)");
      if (prompt_reached(s)) check_reply(now_ms);
      return {};
    }
    std::string line = s.send;
    const std::size_t at = line.find("{pw}");
    if (at != std::string::npos) line.replace(at, 4, secret_);
    note("> " + (s.secret_in_send ? std::string("login ***") : line));
    if (s.write) { result_.wrote = true; }
    state_ = State::kWaitReply;
    std::vector<std::uint8_t> out(line.begin(), line.end());
    out.push_back('\r');
    return out;
  }

  static std::string trimmed(const std::string& text) {
    std::size_t e = text.size();
    while (e > 0 && (text[e - 1] == ' ' || text[e - 1] == '\r' || text[e - 1] == '\n' || text[e - 1] == '\t' || text[e - 1] == 0)) --e;
    return text.substr(0, e);
  }
  bool prompt_reached(const Step& s) const {
    const std::string t = trimmed(rx_);
    if (t.empty()) return false;
    if (s.prompt == ">") return t.back() == '>';
    return t.size() >= s.prompt.size() && t.compare(t.size() - s.prompt.size(), s.prompt.size(), s.prompt) == 0;
  }

  void check_reply(std::uint64_t now_ms) {
    const Step& s = step();
    if (!prompt_reached(s)) return;
    const std::string answer = redact(rx_, secret_);
    note("< " + answer);
    if (contains(rx_, "Invalid command or fail to excute") && !s.tolerate_invalid) { capture(s, answer); fail("step_failed:" + s.label); return; }
    if (contains(rx_, "Unknown command") && !s.tolerate_unknown) { capture(s, answer); fail("step_failed:" + s.label); return; }
    if (contains(rx_, "Test result:fail")) { capture(s, answer); fail("step_failed:" + s.label); return; }
    for (const std::string& need : s.must_contain) if (rx_.find(need) == std::string::npos) { capture(s, answer); fail("step_failed:" + s.label); return; }
    capture(s, answer);
    step_ok(now_ms);
  }

  void capture(const Step& s, const std::string& answer) { captures_.push_back({s.label, s.phase, answer}); }

  void step_ok(std::uint64_t now_ms) {
    const Step& s = step();
    if (s.write) ++result_.writes_done;
    ++result_.steps_done;
    ++index_;
    settle_until_ms_ = now_ms + s.settle_ms;
    state_ = s.settle_ms > 0 ? State::kSettle : State::kNext;
  }

  bool has_update_error() const { return contains(rx_, "Updata error last time") || contains(rx_, "Please updata again") || contains(rx_, "Update Error"); }

  void check_update_text(std::uint64_t now_ms) {
    if (has_update_error()) { fail("firmware:battery_reported_error"); return; }
    const std::size_t ok = rx_.find("Updata OK");
    if (ok != std::string::npos) {
      note("< Updata OK");
      carry_ = rx_.substr(ok + 9);
      captures_.push_back({"updata", step().phase, "Updata OK"});
      step_ok(now_ms);
      sender_.reset();
    }
  }

  void snapshots() {
    std::string before, after;
    for (const Capture& c : captures_) {
      if (c.label == "wake" || c.label == "logout" || c.label == "login") continue;
      std::string& target = c.phase == kPhasePreflight ? before : after;
      if (c.phase == kPhaseWrite && !(c.label == "info")) continue;
      if (target.size() < kMaxSnapshot) target += "## " + c.label + "\n" + c.text + "\n";
    }
    if (before.size() > kMaxSnapshot) before.resize(kMaxSnapshot);
    if (after.size() > kMaxSnapshot) after.resize(kMaxSnapshot);
    result_.before = before;
    result_.after = after;
    for (const Capture& c : captures_) if (c.label == "info" && c.phase == kPhasePreflight) { result_.identity = parse_info(c.text); break; }
    result_.transcript = transcript_;
  }

  void complete() {
    snapshots();
    std::string problem;
    if (plan_.verify) problem = plan_.verify(captures_);
    if (problem.empty()) { result_.outcome = "ok"; result_.code = ""; }
    else { result_.outcome = "failed"; result_.code = "verify:" + problem; }
    state_ = State::kDone;
  }

  void finish_rejected(const char* code) {
    result_.outcome = "rejected";
    result_.code = code;
    state_ = State::kDone;
  }

  void fail(const std::string& code) {
    snapshots();
    const Step* s = index_ < plan_.steps.size() ? &plan_.steps[index_] : nullptr;
    const bool verifying = s != nullptr && s->phase == kPhaseVerify;
    std::string shown = code;
    if (code == "cancelled") result_.outcome = result_.writes_done > 0 ? "partial" : "failed";
    else if (code.rfind("firmware:", 0) == 0) result_.outcome = "partial";
    else if (!verifying && result_.writes_done > 0) result_.outcome = "partial";
    else if (code == "timeout" && !result_.wrote) result_.outcome = "retry";
    else { result_.outcome = "failed"; if (code == "timeout") shown = "timeout_after_write"; }
    result_.code = shown;
    state_ = State::kDone;
  }

  const Plan& plan_;
  std::string secret_;
  const std::vector<std::uint8_t>* image_;
  State state_ = State::kNext;
  std::size_t index_ = 0;
  std::uint64_t step_started_ms_ = 0, settle_until_ms_ = 0;
  std::string rx_, carry_, transcript_;
  std::vector<Capture> captures_;
  RunResult result_;
  bool cancel_requested_ = false;
  std::unique_ptr<xmodem::Sender> sender_;
};

}  // namespace armor::solar::battery
