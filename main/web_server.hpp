// ARMOR-NODE-SOLAR - the node's web panel and its JSON API.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <string>

#include "core/solar_config.hpp"

namespace armor::web {

// Starts the HTTP server on port 80: the panel (embedded in the firmware) and /api/v1. Every route but the session and the first setup
// needs a login; a node with no user yet answers only the setup.
bool start(const config::Settings& settings);

// What the panel is doing about TLS: the mode of the settings it started with, whether the HTTPS server runs, and the certificate's fingerprint.
struct TlsStatus {
  std::string mode = "both";
  bool running = false;
  std::string fingerprint;
};
TlsStatus tls_status();

// For the recovery of a silent panel (web_recovery.hpp). Whether a request that takes as long as the link does (an upload, the core dump) is being served.
bool long_request_running();
// Hands the task of the HTTPS server a piece of work and says whether it had run the one handed the time before. True with no HTTPS server.
bool tls_task_answers();

// Restarts the node after `delay_ms` (the answer that caused it has time to leave).
void restart_after(unsigned delay_ms);

}  // namespace armor::web
