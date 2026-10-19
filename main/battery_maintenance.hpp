// ARMOR-SOLAR - the node's side of the battery maintenance: the orders to write to a Pylontech battery, arrived over the broker, checked and carried out through the port's console.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include "core/solar_config.hpp"

namespace armor::maintenance {

// Subscribes to armor/solar/<node>/maintenance/command (call it before mqtt_link::start()) and answers on armor/solar/<node>/maintenance/result. Everything is refused while the setting
// battery.write_enabled is off or the battery's service password is not set.
void start(const config::Settings& settings);

// Whether an operation on a battery is under way (the node does not restart in the middle of one).
bool busy();

}  // namespace armor::maintenance
