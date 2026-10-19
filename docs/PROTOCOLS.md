# The protocols: what is decoded, from where, and what is not

Everything here is written from the public description of each device's protocol and tested on a computer with typical replies written by hand.
**No inverter or battery has been connected.** A row moves from "decoded" to "verified" only with a capture from the real device (see the
capability matrix in ARMOR-DOCS).

| Device | Port and speed | Decoded | Not decoded |
| --- | --- | --- | --- |
| Voltronic / MPP Solar inverters (Axpert, PIP, InfiniSolar and clones) | RS232, 2400 baud 8N1 | `QPIGS` (status), `QMOD` (mode), `QPIWS` (warnings and faults), `QPIRI` (ratings and settings), the CRC and the frames | Every setting command (`PCP`, `POP`, `PBT`...) on purpose; `QPIGS2` and parallel-system commands; `QVFW`, `QID` |
| Pylontech US2000, US3000, US5000 (and the C versions) | Console RS232 115200 8N1 | The table of `pwr`: every module's voltage, current, temperatures, cell range, state of charge and states | The detail of one module (`pwr 1`, the cell voltages), `bat`, `info`, the event log |
| Pylontech, RS485 | RS485, 9600 or 115200 | The frame: start, version, address, command, the length and its check, the checksum | The meaning of `INFO`, which differs between protocol versions: not decoded until a capture exists |
| ANT-BMS (home-made batteries) | 3.3 V TTL UART, 19200 baud 8N1 | The status frame of both protocols (old and new): the cells, sensors, state of charge, current, capacities, MOSFET and balancer states, checked with frames captured on four real models | Every write command (on purpose), the settings registers, the device information, the Bluetooth link |

## Voltronic / MPP Solar

- A command is text, a CRC-16 and a carriage return; a reply is `(`, text, a CRC-16 and a carriage return. The CRC is CRC-16/XMODEM, and a CRC byte
  that would be `(`, CR or LF is increased by one. The CRCs of `QPIGS`, `QPIRI`, `QMOD` and `QPIWS` (`B7A9`, `F854`, `49C1`, `B4DA`) and the well-known one of
  `(NAK` (`7373`) are tests.
- Only reading commands can be built by the library: a setting changes how the inverter charges and feeds the house.
- **The USB port of these inverters is not a serial port** (it is an HID device): a microcontroller reads the **RS232** port, through a level converter.
- `QPIGS` fields: grid voltage and frequency, output voltage, frequency, apparent and active power and load percentage, bus voltage, battery voltage,
  charging current, capacity, heat-sink temperature, PV current, PV voltage, battery voltage from the charger, discharging current, eight status bits, and,
  on newer models, the PV charging power. An older reply without the power gets it from voltage times current.
- The names of the `QPIWS` flags follow the public document (32 to 36 flags: PV loss, battery derating and the *battery weak* and equalisation bits included); a bit that the document calls reserved has no name and is never reported.

### Optional readings (standard dialect only, off by default)

- **`QPIGS2`, the second PV input** (setting *Read the second PV input*): the answer is `(03.1 327.3 01026 ` (the current in A, the voltage in V and the power in W), and the reading then carries `pv2_v`, `pv2_a` and `pv2_w`, with `pv_w` as the **sum** of both inputs. An inverter with one input answers NAK (or nothing): after two misses it is not asked again, and the reading is never lost for it.
- **`QPGS0` to `QPGS<n-1>`, the units of a parallel system** (setting *Units of a parallel system to read*, 0 to 10): each answer is `(1 92931701100510 B 00 000.0 00.00 230.6 50.00 0275 0141 005 51.4 001 100 083.3 002 00574 00312 003 ...` (whether the unit is there, its serial number, mode and fault code, the grid and the output, its battery figures, the PV voltage, the totals of the whole system, then bits and settings that are not used). The reading then lists `units` (one entry per unit that answers: a unit that is not there, or does not answer, is skipped) and the totals `total_out_w`, `total_out_va`, `total_load_percent` and `total_charging_a`. The port's own inverter is read as always.
- Both come from the mpp-solar project's list of commands and its sample replies (the tests use them); firmwares add fields at the end of `QPGS`, which is why only the first nineteen are read. **No inverter with two inputs, and no parallel system, has been met.**

### Dialects (chosen by the port's *Protocol* setting; read only in all of them)

- **PI30** is the one above. **auto** asks in PI30 and, after a silence, in PI18, and keeps the one that answered; a whole reply of the other dialect (even `^0` or `(NAK`) switches at once, and three silences make it look again.
- **REVO** (Revo VM III and others that esphome-pipsolar marks so): the same frames and the same `QPIGS` fields up to the twelfth, then the PV **power** (watts), the PV voltage, the charger's battery voltage and the energy made today; no discharge current, and the PV current is worked out as power over voltage. Its replies may end with **one byte** (the sum of every byte before it, plus one) and CR instead of the CRC; a reply that fits only that makes an *auto* port REVO (a REVO that answers with the CRC has to be chosen by hand). No `QPIWS` is asked: the reading has no warning list.
- **PI18** (InfiniSolar V, LV5048, SunGoldPower and clones), same 2400 baud: request `^P<command length + 3, three digits><command><CRC><CR>`, reply `^D<payload length + 3, three digits><payload><CRC><CR>`, `^0` refuses and `^1` accepts. The CRC is the same as PI30's. Read: `GS` (28 comma-separated fields: grid V x10, Hz x10, output V x10, Hz x10, VA, W, load %, battery V x10, the charger's battery V x10 (two), discharge and charge current, capacity %, temperatures, PV1 and PV2 power and voltage x10, then the codes: configuration changed, the two chargers' states, load connected, the direction of the battery's power, of the DC/AC conversion and of the line, and the parallel id), `MOD` (00 power on, 01 standby, 02 bypass, 03 battery, 04 fault, 05 hybrid) and `FWS` (a fault code, then 16 warning flags). The requests of the library are `GS`, `MOD`, `FWS`, `PIRI`, `ID`, `VFW`, `PI`, `FLAG` and `T`, and nothing that sets.
- Sources: the mpp-solar and esphome-pipsolar projects and the sample replies they publish (the tests use them); no unit of any of these families has been connected.

## Pylontech console

Typing `pwr` prints one row per module of the stack (the master answers for all of them); a module that is not there says `Absent`. The parser takes
the rows that begin with a module number, tolerates the prompt and the completion line, and refuses a row with a text where a number should be. **The columns are found
by the names of the header line**, because the firmwares differ: the older ones stop at `B.T.St`, others add `MosTempr` and `M.T.St`, and the newer ones (US5000 V2.3) put an `Id`
column after `Tlow`, `Thigh`, `Vlow` and `Vhigh` and end with `SysAlarm.St`; the `Time` column is two words. Without a header the older layout is assumed.
`bat <n>` prints a row per cell (the newer firmwares add the charge in mAH and a `Y` or `N` for the cell being balanced), `info <n>` the identity (`Device name`, `Specification` such as `48V/74AH` for the rated
capacity, `Cell Number`, versions) and `stat <n>` the history (`CYCLE Times`). The identity and the history are asked once and kept for half an hour.
**Health:** `stat`'s `Pwr Coulomb` is the capacity the battery has learned in milliampere-seconds (a new 50 Ah module says `180000000`, a 74 Ah one `266400000`); over the rated capacity of the `Specification` line it is the module's `health_percent` (the stack's is the mean). An answer ends with the prompt (`$$` and `pylon>` or `pylon_debug>`), for a refusal as well as for a good answer.
Captured on real units (US2000C of four firmware versions, an US2KBPL, an US3000C and a 24 V unit), the console echoes every line followed by an empty one, `info` and `stat` answer as described
(`Device name` says US2KBPL for what is sold as US2000B, and a few fields such as `Manufacturer` or `Barcode` can be empty), the console speed is 115200 (`Console Port rate` in `info`) and the prompt
can be `pylon>` or `pylon_debug>`. The `pwr` and `bat` texts were not in those captures: their layouts remain those of the public descriptions.
**What the node reads besides the table.** From `info`: the model, the board, the main software and the software versions (for example `US2000C`, `V10R04`, `B68.2.0.0`, `V2.6`), `Cell Number`, the current limits of the module
(`Max Charge Curr` and `Max Dischg Curr`, `53000mA`, in amperes without the sign) and the speed the console says it uses (`Console Port rate`: recent units say 115200, an old US2000 with firmware V1.5 says 19200). From `stat`: the
counters of the life of the module under the console's own names, lowercased with underscores (`bat_ov_times`, `coc_times`, `data_items` ... in `counters`); the severe ones (the protections and errors) are marked so the server can tell
when one goes up, and what a counter means is not official. From `soh`: the count of each cell and which cells the console calls anything but `Normal` (`soh_counts`, `soh_abnormal`; a cell with a count and the word `Normal` is not
abnormal). From `data event <n>` (n is the `Data Items` of `stat`, taken to be the newest entry): `last_event`, with the time of the battery's own clock, the three states, and the protection fields exactly as printed (`0x2 PHV`,
`0x4 BVNOR`): they are never decoded, because the layout of the numbers has not been confirmed. Both the old names (`BOT`, `BHT`, `Protect ENA`) and the newer ones (`CBOT`, `Bat Protect ENA`, `Pwr Protect ENA`) are accepted as printed.
All of it is asked when the module's details are asked (once, then every half hour), and a battery that does not answer a question leaves that part out and the reading goes on.
The parsers also read the table of `datalist event|history|misc <item>` (the BMS's log as rows of time, voltage, current, temperatures, the lowest and highest cell, the states and the mnemonics of the events; the history keeps one point every 1800 s)
and the snapshot of `getpwr` (`value#` for each field: the pack, then one line per cell); the console of the panel may ask `datalist`, but the poller does not use either yet.
**Waking a battery.** A battery that has been idle for a while may go to sleep and stop answering the console. On the RJ45 console port, +5 to 12 V (5 to 15 mA) for at least half a second between pin 4 and pin 5 (ground on pin 5) wakes the
US2000C, US3000C, US5000 and UP5000 (from the makers' manuals; the node does not drive it). The frame that opens the console at 1200 baud (`~20014682C0048520FCC3` and a carriage return) is `build_frame(0x20, 0x01, 0x46, 0x82, "8520")`, tested.
**Speeds.** The console of recent batteries runs at 115200; the RS485 protocol runs at 9600 with the battery's DIP switch 1 on and at 115200 with it off (restart the battery after changing it). A port may be set to either, and to 19200 for an old unit.
The console is read-only: the only commands ever sent are `help`, `pwr`, `pwrsys`, `bat`, `info`, `stat`, `soh`, `data` and `datalist`, and a test refuses every command of the console that writes (limits, calibration, storage, clock, counters, shut-down, tests).
Models the firmwares are known under: US2000 / US2000B, US2000C, US2000B Plus, US2KBPL, US3000, US3000C, US5000, UP2500, UP5000, Force L1, Force L2 and the Pytes E-Box 48100R (its console is a relative of Pylontech's).
 The stack summary uses what physics says of a parallel stack: the same voltage (so the mean), currents that add, the extreme temperatures and cell
voltages, the mean state of charge. Any state column other than `Normal` raises `alarm`.

## ANT-BMS

The ANT battery management system (the black board of many home-made LiFePO4 packs, 7S to 32S, from 100 A to 320 A) has a 4-pin JST connector with **3.3 V TTL
UART at 19200 baud, 8N1**. **The board must be powered from the connector's VCC (3.3 V) too**: with only TX, RX and ground it neither starts nor answers.
Its firmware speaks one of **two protocols**, so the node knows both: it asks in the old one and, when nothing answers, in the new one, keeps to the one that
answered, and looks again after three misses. Both are polled with one request that returns one whole frame.

| | Old protocol | New protocol |
| --- | --- | --- |
| Seen on | ANT 16S / 24S 100 to 200 A (2020), 16ZMB-TB-7-16S-300A, 24AHA-TB-24S-200A | ANT-BLE16ZMUB, ANT-BLE24BHUB and later (newer boards, including the 32S) |
| Request | `5A 5A 00 00 01 01` (an older firmware also takes `5A 5A 00 00 00 00`) | `7E A1 01 00 00 BE 18 55 AA 55` (`18 55` is the CRC) |
| Answer | 140 bytes, starts `AA 55 AA FF` | 6 + length + 4 bytes, starts `7E A1 11 00 00 <length>` (152 bytes for 16 cells and 2 sensors), ends `AA 55` |
| Numbers | big-endian | little-endian |
| Check | the sum of bytes 4 to 137, big-endian in the last two bytes | CRC-16 (Modbus) of everything between the first byte and the last four, little-endian before `AA 55` |

**Old frame** (offsets from the first byte): 4 total voltage (u16, 0.1 V); 6 to 69 the cells (32 x u16, mV); 70 current (s32, 0.1 A, negative while discharging);
74 state of charge (u8, %); 75 physical capacity (u32, 1e-6 Ah); 79 remaining capacity (u32, 1e-6 Ah); 83 cycle capacity (u32, 0.001 Ah); 87 seconds since start
(u32); 91 six temperatures (s16, degrees; -40 means no sensor); 103 charge MOSFET state, 104 discharge MOSFET state, 105 balancer state (u8 each); 111 power (s32, W);
115 and 118 the highest and the lowest cell numbers, 116 and 119 their voltages, 121 the mean; 123 the number of cells (u8); 132 the balancing bitmask (u32).

**New frame:** 7 battery state (0 unknown, 1 idle, 2 charging, 3 discharging, 4 standby, 5 error); 8 the number of temperature sensors; 9 the number of cells;
10, 18 and 26 three 8-byte bitmasks (protections, warnings, balancing); 34 the cells (u16, mV); then, after the cells and the sensors (`offset` = 2 x cells + 2 x sensors):
the sensors (s16), the MOSFETs' and the balancer's temperatures at 34 + offset and 36 + offset, total voltage (u16, 0.01 V) at 38 + offset, current (s16, 0.1 A) at 40 + offset,
state of charge and state of health (u16, %) at 42 and 44, the MOSFET and balancer states at 46, 47 and 48, then the capacities and the power (u32) from 50 + offset.

The node makes **one module** of the reading: the message has the cells, the sensors (`temperatures_c`), the state of charge, the current (positive while charging, as
everywhere in A.R.M.O.R.), the remaining and the full capacity (only when the BMS knows its capacity: it says 0 Ah when it was never set), and `cycles`, which is
**an estimate**: the BMS's total cycle capacity divided by its capacity. The `alarm` flag is raised while a MOSFET is off **because of a protection**.

| Code | Charge MOSFET | Discharge MOSFET |
| --- | --- | --- |
| 0, 1 | off, on | off, on |
| 2 | overcharge protection | over-discharge protection |
| 3 | over-current protection | over-current protection |
| 4 | battery full (not an alarm) | second-level over-current (new protocol) |
| 5 | total overvoltage | total undervoltage |
| 6, 7 | battery over-temperature, MOSFET over-temperature | the same |
| 8, 9, 10 | abnormal current, a balance wire dropped, mainboard over-temperature | the same |
| 11 | reserved | charge MOSFET on (not an alarm) |
| 12 | open failed (new protocol) | short-circuit protection |
| 13 | discharge MOSFET abnormal | discharge MOSFET abnormal |
| 14 | waiting (not an alarm) | start exception (old) / open failed (new) |
| 15 | manually turned off (not an alarm) | manually turned off (not an alarm) |
| 16 to 20 | new protocol only: second-level overvoltage, low-temperature protection, voltage difference exceeded, self-detect error | second-level low voltage, low-temperature protection, voltage difference exceeded, self-detect error |

The ports page of the node shows these codes as *what the equipment reported* (for example `new charge=1 discharge=1 balancer=0 cells=16`); the balancer state is 0
off, 1 over the limit, 2 charge differential, 3 over-temperature, 4 automatic, 10 mainboard over-temperature.

### Reading the settings (newer protocol, read only)

The node can also **read** a BMS's device information and settings (the button *Read the BMS settings* of its port): a read request is `7E A1 02 <address lo> <address hi> <bytes> <crc> AA 55`, and a register answers
`7E A1 12 <address> <bytes> <value, little-endian> <crc> AA 55`. The device information (address `0x026C`, 32 bytes: 16 of model and 16 of software version, as text) answers with six bytes more than its length byte says,
so it is read by position and its CRC is not checked. The 56 registers are asked one at a time, two bytes each (four for the three capacities); their addresses, names, scales and units are the table of the esphome-ant-bms project
(`core/ant_registers.hpp`), whose captured requests are the tests' vectors. **Nothing is written**: the BMS's write requests and its password are not built anywhere in this project.

The protocol is written from the serial-protocol description of the VBMS project's wiki and from the AntBms-Arduino and esphome-ant-bms projects, whose captured
frames of four models (two protocols) are the test vectors of the decoder. **Only reading is implemented**: the BMS's write commands (switching the MOSFETs, the
parameters, the password) can disconnect a battery under load and are left out on purpose. The BMS also has a Bluetooth link (the one the vendor's phone app uses to
read and configure it); the node uses the cable, which is the more stable of the two.

## Honest limits

- The Pylontech console has a `pwrsys` command (a summary of the whole system) whose output could not be found in any public description, so it is **not read**: a port that asked for it would be guessing its format. The panel's *Ask the battery* (see NODE_FIRMWARE.md) shows what it answers on a real battery, and with that answer at hand it is a small parser to add.
- The meaning of the numbers in the protection and event fields of the console (`0x2`, `0x4`) is not decoded: only the mnemonics beside them are shown. The newest entry of the event log is asked by the count of `Data Items`; that this is its index is an assumption to check on a real battery.

- The exact text of the console differs between firmware versions: the columns are found by the header's names, but a row that lacks a column the reading needs (voltage, current, temperatures, cell extremes, the four states, the charge) is refused rather than guessed. The `pwrsys` command of the newer firmwares (the stack's own totals) is not read.
- The scaling of the RS485 analogue values differs between versions of Pylontech's document; the frame is built and checked here, the values are not read.
- Nothing has been read from a real device by this project: the ANT-BMS frames in the tests were captured by other people on their own units; the first thing to do with a real one is to capture its replies and add them to the tests.
- Writing to a BMS or to an inverter (their settings, the MOSFET switches, the charge priorities) is not implemented and will not be until it has been studied with the real equipment and tried in isolation: a wrong value can disconnect a battery under load or change how an inverter feeds the house, and an ANT-BMS asks for a password to accept a write.
- The ANT-BMS cycle count is an estimate and the MOSFET codes past 15 (new protocol) are as the esphome-ant-bms project lists them; a BMS that does not know its capacity reports none.
