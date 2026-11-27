// ARMOR-NODE-SOLAR - the node's side of the battery maintenance: the orders to write to a Pylontech battery, arrived over the broker, checked and carried out through the port's console.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/solar_config.hpp"

namespace armor::maintenance {

// Subscribes to armor/solar/<node>/maintenance/command (call it before mqtt_link::start()) and answers on armor/solar/<node>/maintenance/result. Everything is refused while the setting
// battery.write_enabled is off or the battery's service password is not set.
void start(const config::Settings& settings);

// Applies a saved settings document without a restart: switching the battery writes OFF takes effect at once and forgets the orders armed so far; a new service password is taken;
// switching them ON still needs the restart.
void apply(const config::Settings& settings);

// Whether an operation on a battery is under way (the node does not restart in the middle of one).
bool busy();

// ---- the node's own panel ------------------------------------------------------------------------------------------------------------------------
// The same writes, asked by a signed-in administrator over HTTPS (main/web_server.cpp checks that and asks the password again before an arm, an execute and a step of a campaign). `session`
// is the person's session: only the one that armed may execute. The functions that return a text answer "" when the order was taken, else the refusal.

void panel_flags(bool& enabled, bool& password);
std::string panel_state(int port, const std::string& session);   // JSON: the port, the image kept, the last operation of the port and the campaign
std::string panel_audit();                                       // JSON: the record of who did what, oldest first
void panel_note(const std::string& user, int port, const char* event, const std::string& detail);
std::string panel_arm(const std::string& user, const std::string& session, int port, const std::string& op, const json::Value& params, const std::string& serial);
std::string panel_execute(const std::string& user, const std::string& session, int port, const std::string& serial);
std::string panel_cancel(const std::string& user, int port);
// A firmware image uploaded by the browser: a buffer in the external memory to fill (nullptr and `why` when it cannot be taken), then kept in place of any other.
std::shared_ptr<std::vector<std::uint8_t>> panel_image_buffer(std::size_t size, std::string& why);
std::string panel_image_keep(const std::string& user, std::shared_ptr<std::vector<std::uint8_t>> whole);
// action: create (port), check, execute (serial), retry, cancel, finish. `module` says which module a refusal is about.
std::string panel_campaign(const std::string& user, const std::string& session, const std::string& action, int port, const std::string& serial, int& module);

}  // namespace armor::maintenance
