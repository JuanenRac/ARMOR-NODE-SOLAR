// ARMOR-NODE-SOLAR - restarts a node whose panel does not answer, when the setting asks for it (core/web_recovery.hpp). Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstdint>

// What the web server (web_server.cpp) tells the recovery. They are declared here because web_server.hpp is the same file in every node and only this one has the recovery.
namespace armor::web {

// Whether a request that takes as long as the link does (an upload, the core dump) is being served.
bool long_request_running();
// Hands the task of the HTTPS server a piece of work and says whether it had run the one handed the time before. True with no HTTPS server.
bool tls_task_answers();

}  // namespace armor::web

namespace armor::webrecovery {

// Reads what a restart of this kind left (and says it in the log), and, when `enabled`, starts the task that asks the panel. Called once, after the web server was started.
void start(bool enabled, int after_s);

// How the last start came about: `after_restart` when this recovery restarted the node, and how long the silence had lasted.
struct Last {
  bool after_restart = false;
  std::uint32_t silent_for_s = 0;
  unsigned restarts_in_hour = 0;
};
Last last();

}  // namespace armor::webrecovery
