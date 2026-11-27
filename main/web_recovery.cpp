// ARMOR-NODE-SOLAR - restarts a node whose panel does not answer, when the setting asks for it. The rules are core/web_recovery.hpp's, tested on a computer. Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include "web_recovery.hpp"

#include <cerrno>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery_maintenance.hpp"
#include "core/node_guard.hpp"
#include "core/web_recovery.hpp"
#include "github_update.hpp"
#include "node_control.hpp"
#include "node_guard.hpp"
#include "psram_mem.hpp"
#include "web_server.hpp"

namespace armor::webrecovery {
namespace {
constexpr char kTag[] = "armor-web";

// The memory a software reset leaves as it was: the restarts of this kind (the limit holds across them) and why the last one was made.
RTC_NOINIT_ATTR nodeguard::Ledger g_rtc_ledger;
RTC_NOINIT_ATTR std::uint32_t g_rtc_ledger_check;
RTC_NOINIT_ATTR Record g_rtc_record;

nodeguard::Ledger g_ledger;
Last g_last;
int g_after_s = kDefaultAfterS;

std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL; }
std::uint32_t uptime_s() { return static_cast<std::uint32_t>(now_ms() / 1000ULL); }

// A request to the node's own plain port over the loopback address: the public look at the session, which answers anybody. Any HTTP status line counts (in HTTPS mode it is a redirect).
bool plain_answers() {
  const int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) return false;   // no socket left: the panel could not take a connection either
  bool answered = false;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(80);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
  const int result = connect(sock, reinterpret_cast<const sockaddr*>(&address), sizeof address);
  bool connected = result == 0;
  if (!connected && errno == EINPROGRESS) {
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(sock, &writable);
    timeval wait{5, 0};
    int error = 0;
    socklen_t size = sizeof error;
    connected = select(sock + 1, nullptr, &writable, nullptr, &wait) > 0 && getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0;
  }
  if (connected) {
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
    timeval wait{8, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof wait);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof wait);
    static constexpr char kRequest[] = "GET /api/v1/session HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (send(sock, kRequest, sizeof kRequest - 1, 0) == static_cast<int>(sizeof kRequest - 1)) {
      char head[8] = {};
      std::size_t got = 0;
      while (got < sizeof head) {
        const int n = recv(sock, head + got, sizeof head - got, 0);
        if (n <= 0) break;
        got += static_cast<std::size_t>(n);
      }
      answered = got >= 5 && std::memcmp(head, "HTTP/", 5) == 0;
    }
  }
  close(sock);
  return answered;
}

// What explains a silence or must not be cut: an upload or a download of firmware, an operation on a battery, a restart already on its way.
bool excused() {
  if (web::long_request_running() || maintenance::busy() || node_control::restart_pending()) return true;
  const github_update::Progress install = github_update::progress();
  return install.state == "downloading" || install.state == "verifying" || install.state == "done" || github_update::copy_progress().state == "copying";
}

void recovery_task(void*) {
  Watch watch(g_after_s);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kProbeEveryMs));
    std::uint32_t silent = 0;
    if (!plain_answers()) silent |= kPlainSilent;
    if (!web::tls_task_answers()) silent |= kTlsSilent;
    // three in an hour at most, counting the guard's own restarts too
    const bool may = g_ledger.may_restart(uptime_s()) && g_ledger.recent(uptime_s()) + nodeguard::restarts_in_hour() < nodeguard::kMaxRestartsPerHour;
    const Action action = watch.look(now_ms(), silent, excused(), may);
    if (action == Action::kNone) continue;
    const Record record = make_record(watch.silent(), watch.silent_for_s(now_ms()));
    if (action == Action::kHeld) {
      ESP_LOGE(kTag, "web recovery: no answer for %u s (%s) and the limit of restarts is used up: the node stays up", static_cast<unsigned>(record.silent_for_s), silent_name(record.silent));
      continue;
    }
    g_rtc_record = record;
    g_ledger.note_restart(uptime_s());
    g_rtc_ledger = g_ledger;
    g_rtc_ledger_check = nodeguard::ledger_check(g_ledger);
    ESP_LOGE(kTag, "web recovery: no answer for %u s (%s): restarting", static_cast<unsigned>(record.silent_for_s), silent_name(record.silent));
    web::restart_after(3000);
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

}  // namespace

void start(bool enabled, int after_s) {
  const esp_reset_reason_t reason = esp_reset_reason();
  const bool kept = reason == ESP_RST_SW || reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT;
  nodeguard::BootInfo info;
  g_ledger = nodeguard::ledger_at_boot(g_rtc_ledger, g_rtc_ledger_check, kept, info);
  g_rtc_ledger = g_ledger;
  g_rtc_ledger_check = nodeguard::ledger_check(g_ledger);
  if (kept && record_valid(g_rtc_record)) {
    g_last.after_restart = true;
    g_last.silent_for_s = g_rtc_record.silent_for_s;
    ESP_LOGE(kTag, "%s", restart_text(g_rtc_record).c_str());
  }
  g_rtc_record = Record{};   // read once
  if (!enabled) return;
  g_after_s = after_s;
  // No flash is touched here (sockets, a log line, a timer), so the stack can be in the external RAM.
  if (!memory::start_task(recovery_task, "web-recover", 4096, nullptr, 3)) ESP_LOGE(kTag, "web recovery could not be started");
}

Last last() {
  Last out = g_last;
  out.restarts_in_hour = g_ledger.recent(uptime_s());
  return out;
}

}  // namespace armor::webrecovery
