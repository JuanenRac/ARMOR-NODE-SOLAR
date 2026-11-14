# The messages of a gateway node (contract version 0)

This is the shape the library produces (`core/solar_json.hpp`). The messages are part of ARMOR-COMMON (schemas `solar_inverter` and `solar_battery`, with
conformance vectors), ARMOR-SERVER reads them, and `tests/check_samples.py` validates what this library prints against that contract.

```
armor/solar/<node>/<device>/state        JSON, one message per device, every few seconds and on a change
```

`<node>` and `<device>` are lowercase letters, digits, `-` and `_`, at most 32 characters, not starting with `-` or `_`. `timestamp_ms` is the node's clock.

## Inverter

```json
{"kind":"inverter","node_id":"solar-1","device":"axpert-1","timestamp_ms":1000,"mode":"line",
 "grid_v":232.0,"grid_hz":50.0,"out_v":230.0,"out_hz":50.0,"out_va":161,"out_w":119,"load_percent":3,
 "battery_v":57.50,"battery_a":12.0,"battery_percent":100,"pv_v":103.8,"pv_a":14.0,"pv_w":856,"heatsink_c":69,
 "ac_charging":false,"pv_charging":true,"load_on":true,"warnings":["line_fail"]}
```

`mode` is `power_on`, `standby`, `line`, `battery`, `fault`, `power_saving`, `shutdown` or `unknown`. `battery_a` is positive while charging.
`warnings` are the active flags of `QPIWS` by name (see PROTOCOLS.md). A REVO inverter has no warning query: its message also carries `"warnings_known":false`, so the empty list is read as "unknown" and not as "no warnings" (the field is absent for the other dialects).

A number the inverter sends beyond what is possible (a load of 255 %, a heat sink at 900 degrees) is held at the limit the contract accepts so the message still goes out; the held values are named in `"clamped":["load_percent"]`, so a consumer shows them as not real. The field is absent when nothing was held.

Optional, when the port asks for them (see PROTOCOLS.md): `pv2_v`, `pv2_a` and `pv2_w` (a second PV input; `pv_w` is then the sum of both), and for a parallel system `units` (up to ten entries with `unit`, `mode` and, when known, `serial`, `fault_code`, `grid_v`, `out_v`, `out_va`, `out_w`, `load_percent`, `battery_v`, `battery_percent`, `pv_v` and `charging_a`) with `total_out_w`, `total_out_va`, `total_load_percent` and `total_charging_a`.

## Battery stack

```json
{"kind":"battery","node_id":"solar-1","device":"us3000-1","timestamp_ms":3000,"modules":2,"state":"discharging",
 "voltage_v":49.87,"current_a":-2.59,"temperature_min_c":19.5,"temperature_max_c":25.0,"cell_min_v":3.328,"cell_max_v":3.349,
 "soc_percent":88,"alarm":false,
 "stack":[{"n":1,"present":true,"voltage_v":49.872,"current_a":-1.28,"temperature_c":22.0,"soc_percent":88,"state":"Dischg"},{"n":2,"present":false}]}
```

A module may also carry `cells_v` (the voltage of each cell, in order), `temperatures_c` (each temperature sensor), `capacity_ah` and `full_capacity_ah` (what is left
and what the module holds when full), and `cycles`; and, when the console tells them, `model`, `board_version`, `main_soft_version` and `soft_version` of the module, `cell_count`, `max_charge_a` and `max_discharge_a` (the current the module says it allows,
positive), `console_baud`, `soh_counts` and `soh_abnormal` (the health of each cell, and the cells counted from 0 that are not `Normal`), `counters` (the life counters of `stat`) and `last_event` (the newest entry of the module's event log, as printed); the stack carries `model`, `capacity_ah`, `full_capacity_ah` (the sums of its modules), `energy_kwh` (the remaining
energy at the mean voltage) and `cycles` (the highest of its modules). A field the battery did not report is left out.

With no module present the message has `modules: 0` and the empty `stack`, and no other reading: the node says the battery does not answer instead of inventing numbers.
`current_a` is the sum of the modules (negative discharging), `voltage_v` their mean, `soc_percent` the mean state of charge (left out when unknown: the contract has no nulls).

- `pv_energy_total_wh` (from the inverter's `QET` answer) is **provisional**: the unit of that answer has not been confirmed on a real inverter, so treat the figure as indicative until it is.

## System summary and the alerts of the node

```
armor/solar/<node>/system                 JSON, one message every `summary.every_s` seconds, QoS 0, not retained
```

```json
{"kind":"system","node_id":"solar-1","timestamp_ms":5000,"inverters":1,"batteries":2,"pv_w":856,"load_w":119,"soc_percent":88.5,"balance_w":737,
 "alerts":[{"code":"soc_low","device":"us3000-1"}]}
```

It adds up the latest reading of every port: `pv_w` the power of all the panels, `load_w` what the inverters give to the house, `balance_w` their difference (positive while the panels give more than the house takes) and `soc_percent` the mean state of charge of the batteries that answered. A reading older than a minute is not counted, so an inverter or a battery that stopped answering does not stay in the totals; the numbers that need an inverter (or a battery) are left out when none answered. `inverters` and `batteries` count the ports that answered in the last round (a stack with no module present is not a battery that answered).

The alerts are decided on the node, from its own settings, so they exist when the server cannot be reached; they are a fixed list of words and a device name, with no free text: `soc_low`, `cell_over_voltage`, `temperature_high` and `temperature_low`. An alert is raised at its limit and cleared a little inside it (2 points of charge, 20 mV, 2 degrees), so a reading that hovers at the limit does not make the message flicker. While an alert lasts, the LED of the port of that device blinks slowly (half a second on, half a second off); the fast blink is still a port that fails and the short pulse a good reading.

Everything is **off** by default. The settings (the `summary` block of the configuration file of the node):

| Key | Default | Meaning |
|---|---|---|
| `summary.publish` | off | Send the message. The broker must let the node write `armor/solar/<node>/system` (`mqtt_identity.sh` of ARMOR-DEVOPS gives that line to the nodes it makes; an older node needs `upgrade-node`). |
| `summary.alerts` | off | Raise the alerts (in the message and on the LEDs). |
| `summary.every_s` | 10 (5-3600) | Seconds between two messages. |
| `summary.soc_low_percent` | 20 (1-99) | `soc_low` when the state of charge is at or below this. |
| `summary.cell_over_mv` | 3650 (3000-4500) | `cell_over_voltage` when the highest cell is at or above this, in millivolts. |
| `summary.temperature_high_c` | 50 (0-120) | `temperature_high` when the hottest module is at or above this. |
| `summary.temperature_low_c` | 0 (-40-60) | `temperature_low` when the coldest module is at or below this; it must stay under the high limit. |

Nothing of this has run on a board: the host tests (`tests/test_system.cpp`) drive the poller against a simulated RS-485 line with a Pylontech stack (`tests/pylontech_rs485_sim.hpp`: noise, a silent stack, a reply in small pieces, a line cut in the middle of a reply, a module that disappears) and the messages are accepted by the contract of ARMOR-COMMON (`tests/check_samples.py`).
