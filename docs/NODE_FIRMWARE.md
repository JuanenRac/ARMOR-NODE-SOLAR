# The firmware of the solar node

The firmware of **two boards**, one code base with a board profile chosen when the image is built (see *The two boards*): an **ESP32-S3-WROOM-1 N16R8** (16 MB of flash, 8 MB of octal PSRAM, two USB-C sockets, no Ethernet: Wi-Fi is its way in) and the **Waveshare ESP32-S3-ETH** (its way in is the Ethernet cable). It reads solar
inverters and batteries through up to **ten serial ports** and publishes their readings to A.R.M.O.R.'s broker (`armor/solar/<node>/<device>/state`, the
messages of ARMOR-COMMON). It has the same web panel as the radar nodes' (setup, login, users, Wi-Fi, broker, over-the-air update with rollback, log, the panel
over HTTPS) and the pages of its own: the ports and the readings.

**Nothing here has run on a board, and no inverter or battery has been connected.** What has been done: the firmware builds for both boards in the ESP-IDF 5.5.5 container
(a 1.3 MB image in a 3 MB slot), its core (settings, the exchange with each kind of equipment, the emulated UART's arithmetic) is tested on a computer with
stand-ins for the equipment, the messages it makes are accepted by ARMOR-COMMON, and the panel was exercised in a browser against a stand-in node. The host tests are 2,550 checks.

## Configuration from a phone over Bluetooth

The node listens over Bluetooth Low Energy (NimBLE) with the same channel as the radar node's, so the ARMOR app on a phone finds it as `ARMOR-XXXXXX` and sets up its name, Wi-Fi, address, broker and Bluetooth mode, with the panel's users and set-up code ([BLE_PROVISIONING.md](BLE_PROVISIONING.md)). The setting `ble.mode` (Network page) is `setup` by default (the node listens only while it has no user), `always` or `off` (the Bluetooth stack is not even started). What a node lists in its settings differs between the three kinds of A.R.M.O.R. node, and the app only touches what they all have. The radio side builds for both boards and the framing and the access rules are tested on a computer; **it has never run on a board and no phone has talked to it.**

## The two boards

| Profile | Board | Way in | Build |
| --- | --- | --- | --- |
| `s3-wifi` (the default) | ESP32-S3-WROOM-1 N16R8, two USB-C sockets | Wi-Fi: a station and/or its own access point | `tools/build_node.sh generic s3-wifi` -> `dist/generic-s3-wifi.bin` |
| `s3-eth` | Waveshare ESP32-S3-ETH: ESP32-S3R8, 16 MB flash, W5500 Ethernet (RJ45, PoE through a separate module), microSD socket, camera connector | The cable, with DHCP or a fixed address; Wi-Fi may be kept as an access point of its own (192.168.4.x, not bridged to the cable) | `tools/build_node.sh generic s3-eth` -> `dist/generic-s3-eth.bin` |

The profile decides the **pin table**, the **default pins of the ports**, the **default way in** and whether the **W5500 driver** is built. **An image is for ONE board**: an `s3-wifi` image on the Waveshare board would
put ports on the W5500's pins, and an `s3-eth` image on the other board would look for a W5500 that is not there. The panel shows the board's name in the overview and the image's own name is in the file.

### The N16R8 board (`s3-wifi`)

The pin map used is the one printed for this board (two USB-C sockets: **USB & OTG**, the chip's native USB on GPIO 19 and 20, and **USB to serial**, a CH343P on
UART0's GPIO 43 and 44):

| GPIO | Use |
| --- | --- |
| 26 to 32 | The external flash: never offered |
| 33 to 37 | The octal PSRAM of the N16R8 module (35, 36 and 37 are on the header: do not use them on this module) |
| 19, 20 | The native USB: the log and the flashing |
| 0, 3, 45, 46 | Strapping pins (0 is the BOOT button): usable, but what is wired there must not pull them the wrong way while the chip starts |
| 43, 44 | UART0's pins to the USB-serial socket: usable, but a cable in that socket drives them |
| 48 | The on-board RGB LED |
| the rest of the header | Free |

The log comes out of the **USB & OTG** socket (the console is moved to the native USB so that UART0 is free for a device); flash through the same socket.

### The Waveshare ESP32-S3-ETH board (`s3-eth`)

Its USB-C socket is the chip's native USB (the log and the flashing). The pin map is ARMOR-NODE-RADAR's, for the same board:

| GPIO | Use |
| --- | --- |
| 26 to 32, 33 to 37, 19 and 20 | The flash, the octal PSRAM and the native USB: never offered |
| 9 to 14 | The W5500 (reset, interrupt, MOSI, MISO, clock, chip select): never offered |
| 8 | Wired to the camera connector: never offered |
| 4 to 7 | The microSD socket: usable when no card is used (the panel says so) |
| 0, 3, 45, 46 | Strapping pins: usable with care, as on the other board |
| the rest (1, 2, 15 to 18, 21, 38 to 44, 47, 48) | Free |

The defaults of the ports move to free pins (1 to 3: UART1 and UART2 on 16/15 and 18/17, UART0 on the SD pins 4/5; 4 to 9: emulated on 1/2, 21/47, 38/39, 40/41, 42/48 and 43/44). **Port 9 sits on GPIO 43 and 44, UART0's pair: the chip's boot ROM prints its start-up message there at every restart (about a kilobyte, even with the console on the USB), so whatever is wired to port 9 receives it; a piece of equipment that does not tolerate that is better on another pair (change the pins of port 9 in the panel).** **The tenth port has no free pair left and sits on the strapping pins 3 and 46**: leave
it off unless you need it, and only wire equipment whose idle level does not disturb the boot. The emulated ports' interrupt service is not IRAM-safe on this board (the W5500's handler shares it), so an edge that arrives while the
flash is being written (a setting saved, an update) may be delayed: a reading in that instant can be lost, the next one is fine.

**Ethernet:** the address comes from DHCP unless you fix one in *Network* (address, mask, gateway and DNS are checked together). The node's own Wi-Fi network (kept by the set-up, `ARMOR-NODE-SOLAR-xxxxxx`, the setup code as its password) is a separate
network for reaching the node from a phone; it is not joined to the cable. A node that is being set up is reachable through the cable and through the set-up network at once.

## The ten ports

| Port | What it is | Default pins RX, TX, DE |
| --- | --- | --- |
| 1 | hardware UART1 | 16, 15, 7 |
| 2 | hardware UART2 | 18, 17, 8 |
| 3 | hardware UART0 (on pins away from the USB-serial socket) | 4, 5, 6 |
| 4 to 10 | **emulated** (software) | 9 10 -; 11 12 -; 13 14 -; 21 47 -; 38 39 -; 40 41 -; 42 1 - |

Every port is 8N1 and independent: a node may read only inverters, only batteries, or a mix (an inverter on port 1, a battery stack on port 2, another inverter on
an emulated port...). Each enabled port has:

- **Equipment:** *Voltronic / MPP Solar inverter* (2400 baud, `QPIGS`, `QMOD`, `QPIWS`), *Pylontech battery* (115200 baud, the console's `pwr`, then `bat n` and
  `info n` of each module for its cells, capacities, cycles and model), *ANT-BMS battery* (19200 baud, one request and one frame back; both protocols of its firmware, the
  node finds which one) or *Raw monitor* (nothing is asked; what arrives is kept and shown as hexadecimal, to look at a protocol that is not decoded).
- **Device name:** a piece of the topic (`armor/solar/<node>/<name>/state`) and what Studio calls the equipment.
- **Speed**, **seconds between readings**, and for a battery how many modules to read cell by cell.
- **RX, TX and DE pins.** DE is the direction pin of an RS485 transceiver (high while the node sends; tie DE and RE together).

**Emulated ports** time the line by software: an interrupt on every edge of the RX pin notes the time and the bytes are worked out from those times, and a hardware
timer changes the TX pin at every bit. They are for slow lines: **up to 19200 baud** (the panel refuses more). The Pylontech console runs at 115200 baud, so it needs a
hardware port; the Voltronic inverters (2400 baud) and the ANT-BMS (19200 baud) fit an emulated one. The chip has four hardware timers only, so **the seven emulated
ports share one for sending** and one of them sends at a time (a request is a few bytes at a slow speed, and the others go on listening meanwhile); receiving needs no
timer, only the interrupt of each RX pin.

A port with a problem says so in the panel and stays out of the way of the others: *waiting* for its first reading, *reporting*, *silent* (nothing comes: check TX
and RX, the ground, the converter, the speed), *garbled* (bytes come but no answer is valid: wrong speed, missing converter, another protocol) or *error* (the port
could not be opened: unusable pins, a wrong speed). A reading is published only when it is whole: an inverter needs all three answers, so a missing list of warnings
never looks like "no warnings".

## Asking a battery's console a question (*Ask the battery*)

A Pylontech port's card in the panel has a small tool for an administrator: it sends **one question** to the battery's console, from a short list of commands that only read (`pwrsys`, `pwr`, `help`, and `bat`, `info`, `stat`, `soh`, `data` with a module from 1 to 16), and shows the answer exactly as it came. The text that goes on the wire is rebuilt from that list and never copied from what was typed, so no command that changes anything can be sent; the port's readings pause while it waits (at most eight seconds), and in the mux profile the question is asked between two turns. It is there to capture what a command whose output is not described anywhere (`pwrsys`) really answers on the battery in front of you, so that it can be turned into a reading. Not run against a real battery.

`datalist event|history|misc <item>` (the BMS's logs as a table, the item up to 9999, with the optional word `dtls` the real units accept) is on the list too. Every other command of the console is refused, and a test goes through those that write.
**The dump of what a port heard.** *Show what it hears* on a port shows the last bytes the port received as hexadecimal lines with the printable characters beside them, and an administrator can **download it as a text file**: it is how another battery model is diagnosed without a computer.
Only an administrator can ask the node for it (the dump can hold a serial number). A Pylontech port that stays silent shows a hint that the battery **may be asleep** (how to wake it, the pins of the cable and the speeds to try), and the port's speed list says which speed is for the console and which for the RS485 protocol.

## Writing to a battery (off by default)

The one-question console above only reads. Changing what a Pylontech battery keeps (its clock, counters, limits, outputs, serial number, firmware) is a separate capability, **off unless the owner
turns it on** in the node (Network page of the panel, *Battery maintenance*: the switch and the battery's service password, which is kept only here), in the server (`ARMOR_BATTERY_WRITE=1`) and in
the broker's access list. It does not go through the panel at all: the server sends the node typed orders on `armor/solar/<node>/maintenance/command` and the node answers on `.../result`
(ARMOR-COMMON `docs/CONTRACTS.md`). How it works, what is checked first and what each operation sends: [BATTERY_MAINTENANCE.md](BATTERY_MAINTENANCE.md). **Untried on a real battery**: the staged plan
for a spare one is [BATTERY_BENCH_TEST_PLAN.md](BATTERY_BENCH_TEST_PLAN.md).

## The base board with multiplexers (the *mux* profile)

The three hardware UARTs are all a node has for the Pylontech console (115200 baud is beyond the emulated ports), so the base board with **ten RJ45 sockets** (eight for batteries, two for inverters) puts a **74HC4052** multiplexer in front of each UART and a **MAX3232** on every pair of ports. The setting *Ports of the node* (Ports page, `profile` in the settings) chooses between the two ways of using the pins: **direct** (the default: ten ports, each with its own pins, as before) and **mux** (the eight ports of that board).

| Group | UART | TX | RX | S0 | S1 | Ports |
| --- | --- | --- | --- | --- | --- | --- |
| A | UART1 | GPIO 15 | GPIO 16 | GPIO 17 | GPIO 18 | 1 to 4 |
| B | UART2 | GPIO 1 | GPIO 2 | GPIO 38 | GPIO 46 | 5 to 8 |
| C | UART0 | GPIO 40 | GPIO 41 | GPIO 42 | none (ground) | 9 and 10 |

The first four ports are batteries (Pylontech at 115200 baud, or an ANT-BMS at 19200 on the TTL connector of the same port), the last two are inverters. Group B's second select line is GPIO 46 here because GPIO 37, which version 1.0 of the board wires, belongs to the octal PSRAM of both boards and the node refuses it (see [NODE_HARDWARE.md](NODE_HARDWARE.md)). These are the defaults and all of them can be changed in the panel (the pins are checked as always: none may be reserved or shared). The LEDs' shift register (a **74HC595**, one LED per port) takes GPIO 21 (data), 39 (clock) and 47 (latch); with the three pins on *none* the node has no LEDs. The pins are free on both boards.

**Who has the line.** The ports of a group take turns, one at a time (`core/mux_group.hpp`): a port whose cycle is due gets the line, the multiplexer is pointed at it, the UART is given **that port's speed and polarity** (a group may mix a Pylontech console at 115200 and an inverter at 2400) and emptied of whatever the port before left, and the port's exchange runs until its cycle is over; then the next port in order. A port that is not due costs no time; a silent one costs its own timeouts and nobody else's; a *raw* port only listens for a second and a half every six seconds. The three groups run at the same time. A cycle that lasts more than 90 seconds is cut. The reading of an ANT-BMS's settings, asked in the panel, is done between two turns.

**No DE pin on a mux group.** The ports of the mux board are RS232 (a MAX3232 on every pair): there is no RS485 direction pin to drive, and the group's UART is opened with only its TX and RX pins. A device on RS485 belongs on a direct port of the node (settings *RX*, *TX* and *DE* of the port), not on the multiplexer.

**The LEDs:** a short pulse for every good reading of a port, fast blinking for a moment when its cycle ended with no reading (no answer, or a wrong one), and dark when the port is switched off.

**Signals upside down.** A device with TTL levels (an ANT-BMS) wired through a MAX3232 arrives with its levels flipped, and what the node sends it goes out at RS232 levels (about ±5 V). The setting *Signals arrive inverted* of a port makes the UART flip both directions; it does not make the levels right for a TTL input, which needs a path without the MAX3232 (see the wiring notes). RS232 equipment (the Pylontech console, an Axpert) needs no inversion.

**Tall stacks.** A Pylontech stack of eight modules takes a few seconds to read cell by cell, and other ports wait meanwhile. *Modules' cells per cycle* (`cells_per_cycle`) reads the cells of only that many modules in each cycle, in rotation; the others keep the cells of their last turn. The first cycle reads them all.

The profile has been tested on a computer with a stand-in for the multiplexer and the equipment (which lose the bytes when the channel moves, ignore a wrong speed or polarity and inject a stray byte at every change); **it has never run on a board**, nor has any multiplexer switched a real line.

## Wiring

Read `NODE_HARDWARE.md` first. In short: **never connect an RS232 or RS485 line straight to a pin**; use a MAX3232 (3.3 V) for RS232 and a MAX485 for RS485, isolated
if the equipment shares its ground with a battery bank, with a common ground on the node's side. The pins take 3.3 V only.

## First start

1. Build the image for your board (`tools/build_node.sh generic s3-wifi` or `tools/build_node.sh generic s3-eth`, in Linux or WSL with Docker: `dist/generic-<board>.bin`) and flash it through the **USB & OTG** socket
   (the USB-C socket of the Waveshare board): `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 dist/generic-<board>.bin`.
2. A node with no user opens the Wi-Fi **ARMOR-SETUP-xxxxxx** (the password is the setup code) and shows the code on its USB console every 15 seconds. Join it,
   open `http://192.168.4.1/`, enter the code, create the administrator and, if you want, the Wi-Fi network the node is to join. The node restarts.
   On the Waveshare board the cable works too: plug it in and open the address DHCP gave the node (its MAC is on the console); there is no Wi-Fi to give in the set-up.
3. After the setup the node keeps its own Wi-Fi **ARMOR-NODE-SOLAR-xxxxxx** (its password is the setup code; change it in the Wi-Fi page) so it can always be reached, and
   joins the network you gave it. Holding BOOT for 8 seconds within 30 seconds of power-up erases the settings and the users.
4. In the panel: **Serial ports** (enable and set each port, save, restart), **Broker** (the address and the identity), **Readings** (the last reading of every port).
5. On the broker: `sudo scripts/mqtt_identity.sh add solar-node <node id>` (ARMOR-DEVOPS) makes the node's identity, which may write `armor/solar/<node id>/#` and
   nothing else. In Studio, *Inverters* and *Batteries* declare the equipment with the same node id and device name; the first reading fills it in.

## Bench checklist (everything below is untried)

- The node starts, opens the setup Wi-Fi, the panel answers over HTTP and HTTPS, the setup completes, the update from the panel works and rolls back a bad image (the `.bin` to upload is `build/generic-<board>/armor_solar.bin`, not the merged image).
- **Waveshare board:** the W5500 answers on SPI (the overview says so when it does not), the link comes up with the cable, DHCP gives an address, a fixed address is kept after a restart, unplugging the cable is noticed and plugging it back recovers without a restart, the emulated ports still count no overruns while the panel is busy, and PoE powers the board without USB (read the schematic before connecting both).
- A hardware port with a Voltronic inverter: `QPIGS`, `QMOD` and `QPIWS` are answered; compare the panel's numbers with the inverter's display; look at *what it hears*.
- A hardware port with a Pylontech console: `pwr`, `bat n`, `info n`; the formats are written from memory of the public console and may differ between firmwares.
- An emulated port at 2400 baud: the same inverter through it; check the framing errors and the overruns in the ports page while Wi-Fi is busy.
- An RS485 transceiver with the DE pin: the request goes out and the answer comes back, with no echo mistaken for an answer.
- An ANT-BMS on a hardware port and on an emulated one (19200 baud is the limit of the emulated ones): the *what the equipment reported* line of the ports page shows the protocol it found (`old` or `new`) and
  the MOSFET codes; compare the cells, the current and the state of charge with the BMS's own app. Its 3.3 V supply must be on the connector.
- All ten ports enabled at once with equipment on several of them, Wi-Fi busy: the counters of overruns and framing errors of the emulated ports stay at zero, the requests of two emulated ports never overlap
  badly (one sends while the other waits).
- On an inverter port, leave *Protocol* on *auto* the first time: the port page says which dialect answered (PI30, REVO or PI18) after the first reading; if nothing answers, try each by hand (a REVO with a CRC reply has to be chosen by hand) and keep a raw capture of the silence. Check the PV power against the inverter's own display: a REVO read as PI30 shows the wrong PV numbers.
- Keep the text of `pwr`, `bat 1`, `info` and `stat` of each model in a file (the maker's program or a terminal at 115200): only `info` and `stat` have been captured so far, and the layouts of the other two are those of the public descriptions.
- On a Pylontech port, compare `pwr`, `bat 0` and `info 0` in a terminal with what the panel shows (columns by name: a wrong header would shift every value); the cycles and the model appear after the first `info` and `stat`.
- On an ANT-BMS port of the newer protocol, *Read the BMS settings*: the model and version appear, then the table of values; compare a few (the number of cells, the capacity, the cell over- and under-voltage limits) with the BMS's own app. It is read only: check that the BMS's own settings did not change. The old protocol says so instead.
- Read any battery whose protocol is not decoded through a raw port and keep the capture: it is what its decoder will be written from.
- Leave it running for a day: no reset, no growth of the memory shown in the overview, readings in Studio.

## Files

`main/` is the ESP-IDF component (`app_main.cpp`, `solar_manager.cpp` one task per port, `uart_ports.cpp` the hardware and emulated ports, `network.cpp`,
`web_server.cpp`, `api_shared.cpp`, `mqtt_link.cpp`, `node_store.cpp`, `tls_cert.cpp`, and for the Waveshare board `board_ethernet.cpp`, the W5500 driver); `core/` is the part with no hardware in it (`solar_config.hpp` the settings,
`poller.hpp` the exchange with each kind of equipment, `ant_bms.hpp` the ANT-BMS's two protocols, `soft_uart.hpp`, `netplan.hpp`, `auth.hpp` and the protocol library); `panel/` the web panel (eight languages).
`tools/panel_mock.mjs` is a stand-in node for working on the panel without a board.

**Firmware slots.** The panel's *Update* page also lists the two application slots with the version each one holds and boots the other one at the next restart (`POST /api/v1/ota/switch`, an administrator only): the way back to the version that ran before an update, or forward to the one just installed. An empty slot or a firmware of another project is refused.

## Health report

About once a minute the node publishes a health report to `armor/health/<node>` (the `node_health` message of ARMOR-COMMON, see its `docs/CONTRACTS.md`): the firmware version and board, how long the node has been up and why it last restarted (`power_on`, `software`, `panic`, a watchdog, `brownout`...), the free heap now and the least it has had, the link it uses (Wi-Fi with its signal in dBm, or the Ethernet cable) and whether it is up, how many times it has connected to the broker since it started, how many tasks are short of stack and the last error line it logged. It is built by `main/node_health.cpp` and `core/node_health.hpp`, which are shared by every node (`ARMOR-COMMON/tools/sync_firmware_base.py` keeps the copies identical); the choices it makes - the words for a reset reason, which link the node is on, the last error of the log - are tested on a computer in `tests/test_base.cpp`. The report is information only: the node decides nothing from it and nothing is sent back. The broker must let the node write `armor/health/<node>` (`scripts/mqtt_identity.sh add` of ARMOR-DEVOPS does it for a new node, `health-report` for one installed earlier); the server judges the reports and raises its warnings. It has not run on a board.

## Memory

The board has two kinds of memory: some 300 KB of internal memory, which the radio, the drivers and every task stack share, and 8 MB of external RAM (PSRAM) that nothing may touch while the flash is being written. The firmware keeps the internal memory for what needs it and puts the rest outside:

* **TLS** (the HTTPS panel, the update from GitHub, an encrypted broker link) takes its memory from the external RAM and, on a board where it is missing or full, from the internal memory (`CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC` with the allocator of `main/psram_mem.cpp`; the stock "external" mode would fail every request on a board without PSRAM).
* **Requests above 4096 bytes** (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`) go to the external RAM first: the texts and tables of the panel, the JSON documents. 32 KB of the internal memory stay reserved for what must be there.
* **Task stacks**: the tasks that only read counters and queue messages, and never touch the flash, have their stack in the external RAM (`memory::start_task`): the check for a newer release (12 KB), the broker link's sender and watchdog, the health report. The tasks of the radio, the network, the settings, the update itself and the equipment stay on internal stacks.
* **Seen in the log**: every minute the line `memory: internal N KB free (least N KB, biggest block N KB), PSRAM N KB free of N KB, N task stacks (N KB) in PSRAM` (a warning when the internal memory is under 40 KB), and in the status of the panel and in the health report `heap_internal_free`, `heap_internal_min`, `heap_internal_largest` and `psram_free`. The figure `heap_free` adds the external RAM, so it never looks short; judge the memory by the internal figures.

None of it has been measured on a board yet: compare the line of the log before and after the update on the same node, with the same radars and the panel open, and expect more internal memory free than before.

## Self-protection

A node that runs for months has to notice when it is in trouble and put it right by itself, without turning into a restart loop. The rules are pure functions in `core/node_guard.hpp` and `core/ble_window.hpp` (tested on a computer in `tests/test_base.cpp`); `main/node_guard.cpp` reads the real values and acts. All of it is shared by the five nodes (`ARMOR-COMMON/firmware_base`). The thresholds are starting values, to be compared with the readings of a real board.

* **The tasks the node needs exist before Bluetooth starts.** The broker link's tasks (and, in the radar node, the heartbeat and the telemetry) are made at start-up, not when the network comes up, and Bluetooth is the last thing `app_main` starts. Before, the Bluetooth stack took the internal memory first and the telemetry task was never created: the node ran for days publishing a few dozen messages. The tasks that only build and queue messages and never touch the flash have their stack in the external RAM.
* **Bluetooth memory.** The host stack of NimBLE is allocated in the external RAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`) and the controller is given the three activities it uses (`CONFIG_BT_CTRL_BLE_MAX_ACT`). Bluetooth is not started at all when less than 40 KB of internal memory is free, and the log says the internal memory before and after it starts.
* **The set-up window closes.** In mode `setup` Bluetooth switches itself off after `CONFIG_ARMOR_BLE_SETUP_MINUTES` minutes (10 by default, 1 to 120) with nobody connected, or two minutes after a node that already has users gets an address; the stack is stopped and its memory freed, and it stays off until the node is restarted. Mode `always` is the owner's own choice and never closes.
* **The guard.** Every two seconds it looks at the tasks the node registered as needed and at the internal memory. If a task is missing, or the internal memory stays under 16 KB (back to normal over 24 KB), for a minute, it logs it (the line is also the last error of the health report, which is sent at once) and restarts the node once, cleanly. At most three such restarts in an hour of running time: after the third the node stays up and repeats the alarm every ten minutes. After a restart the guard asked for, Bluetooth stays off for that start.
* **The gate at the door of the panel.** While the internal memory is under 28 KB, or its biggest block under 12 KB, a new connection to the panel is refused before anything is allocated for it; it opens again over 40 KB and 16 KB. Open connections, the broker link and the node's own work are not touched; the log says each change with the memory at that moment.
* **A failed allocation** is not left to abort: the incident is written in memory that survives a restart and the node restarts at once; the next start logs `oom heap=<free>/<largest> after <s> s` (the last error of the health report). During the write of a firmware image, or a second time within a minute of running time, it aborts as before so that a loop stays visible.
* **The dead-man switch.** With a broker set up and connected, ten minutes without handing any message to it restart the node (same limit of three an hour); after that it stays up, degraded, and says so.
* **The guard and the server's orders.** A node the server paused or put in standby is not restarted for a task it stopped on purpose (the memory rules stay in force), and neither is one with a restart order on its way; a node kept in standby boots into it again after a restart of the guard, because the standby is kept in flash. A restart ordered on purpose leaves a mark in the memory that survives a software reset: the next start says in the log that it was not the guard's, and it is not counted against the three an hour. The planned-maintenance signal goes out before an ordered restart, a firmware install or a panel upload, so the server holds back the node-offline alert for that bounded time; a restart of the guard does not announce itself.
* **Last will and birth.** The broker link's last will on `armor/health/<node>` is retained, and the node says `{"node_id": "<node>", "online": true}` (retained) each time it connects; the radar node does the same on `armor/node/<node>/health`. The last message of the topic is always the state of the node, also for a server that starts later.
* **Building.** A cached `sdkconfig` keeps the old value of a line added to `sdkconfig.defaults`: delete `.compile/<profile>` (or `build/<profile>`) before building, and run `python tools/check_sdkconfig.py` afterwards; it lists every line of `sdkconfig.defaults` that the build did not take.

None of it has run on a board. To check on one: the log at start (`starting Bluetooth with N KB`, `Bluetooth is up`, `Bluetooth is off`), the minute line of the memory, and a forced shortage of memory to see the gate and the guard act.

## Control from the server

The server can give the node one typed order without the configurator being opened: `restart`, `pause`, `resume`, `standby`, `identify`, `diagnostics`, `maintenance_on` and `maintenance_off`, on `armor/control/<node>/command` (and, to arm only, on `armor/control/all/command`), answered on `armor/control/<node>/result` (the `node_control_command` and `node_control_result` messages of ARMOR-COMMON, see its `docs/CONTRACTS.md`). Only the server's identity may write there (the broker's access list), a node that is not set up or has no broker listens to nothing, and the node acts only on an `execute` that carries the one-time token it gave for that very operation within the last thirty seconds. There is no factory reset, no erase of the settings or the Wi-Fi credentials, no deep sleep and no wake timer.

* `restart` is the clean restart (the connections are closed first), at most one a minute; it sends `restarting` before it goes and may wait a rolling slot so that nodes never restart in the same half minute.
* `pause` stops the node's work for 60 to 3600 seconds (an hour when none is said) and the work starts again by itself; a restart ends it. `standby` switches the work off with no end and keeps that across restarts (the node boots straight into it): the node stays on the network, answers every order and its health report says `standby` since when and for whom. `resume` ends either. Standby is not a power-off.
* `restart`, `pause` and `standby` are refused (`busy_update`, `busy_upload`, `busy_slot_copy`, `busy_battery`) while the node installs a firmware, receives an upload or copies a slot or runs a battery operation.
* `identify` shows the node for 5 to 300 seconds (30 when none is said), never with a siren. `diagnostics` answers a read-only snapshot. `maintenance_on` and `maintenance_off` set a flag the node keeps in memory and reports in its health report, for the server to hold back the node-offline alert.

While paused or in standby the node refuses every order to write a battery (`rejected`, refusal `disabled`; a running operation can still be cancelled) and keeps reading the inverters and batteries and publishing them. The inverter settings written from the panel are not affected.

The rules are in `core/control_gate.hpp` and are tested on a computer (`tests/test_control.cpp`, against the shared vectors of ARMOR-COMMON); `main/node_control.cpp` does the talking, the one-second timer and the flash. It has not run on a board.

Before the node goes quiet on purpose it tells the server, so that the server holds back the node-offline alert for a bounded time instead of raising an alarm for a restart it knows about: the retained message `armor/control/<node>/maintenance` (the `node_maintenance` contract, QoS 1) says `active`, the `reason` (`restart`, `pause`, `standby`, `update`, `upload`, `operator`) and for how long the node expects to be quiet. It is published with the acknowledgement of a restart (before the reboot delay ends), when a pause or a standby begins, and before a firmware install, a slot copy or a panel upload starts; it is cleared (`active` false, also retained) when the node is back and connected (not when it boots into standby: a standby keeps saying `standby` until it is resumed), when a pause ends or is resumed, and when an update, a copy or an upload ends without a restart. A window that is never closed ends by itself when its time is over, so a node that failed to come back does not leave a stale `active`. The node's access list lets it publish only its own `maintenance` topic. A battery operation that is running is not announced: the node refuses to pause, stand by or restart while it runs. It has not run on a board.

## Diagnostics: how the last run ended, the safety of the settings and the diagnostic package

What the node says about itself when it misbehaves, so that nobody has to be at the board with a cable. The rules are one piece of code shared by the five kinds of node and tested on a computer (`core/boot_report.hpp`, `core/config_snapshot.hpp`, `core/diag_package.hpp`, `tests/test_diag.cpp`); `main/node_diag.cpp`, `main/node_guard.cpp` and `main/node_store.cpp` do the talking and the flash. **None of it has run on a board yet.**

* **How the last run ended (the `boot` member of the health report).** After a start that was not clean (a crash, a watchdog, a sag of the supply, an allocation that failed, a restart of the node's own guard) the health report carries, for the first ten minutes of the run, the cause (`panic`, `task_watchdog`, `interrupt_watchdog`, `other_watchdog`, `brownout`, `power_glitch`, `cpu_lockup`, `out_of_memory`, `guard_missing_task`, `guard_low_memory`, `guard_dead_man` or `guard`), the reset reason of the chip, how long the node had run and the internal memory it had at the last look the guard took (every two seconds, in memory that survives a software restart), whether a core dump is waiting in flash, and whether someone asked for the restart on purpose. A clean start (power on, the reset pin, a restart someone asked for) says nothing. The server keeps it once, in the node's log of restarts and in the History.
* **The memory gate and the core dump in the health report.** `memory_gate` says whether new panel connections are being refused now (closed or open, how many times it changed, and the levels it works by) and `coredump` whether a core dump is waiting (and its size).
* **The core dump.** A crash writes its core dump (ELF, with a checksum, up to 32 tasks) to the `coredump` partition. The panel's *Update* page has a *Crash report (core dump)* card for administrators: look for it (the task, the place and the reason), download it, or erase it. The routes (`GET /api/v1/coredump/info`, `GET /api/v1/coredump`, `POST /api/v1/coredump/erase`) need the administrator's session like every other panel request, and the dump is never written to the log or sent over the broker: it can hold the memory of the moment of the crash, secrets included. The partition is 128 KB in the partition table of this repository; a board flashed by cable before that has the older 64 KB one (a partition table is not changed by an update over the air), where a long dump may not fit and the node then shows none.
* **Sealing the stored secrets (optional, off by default).** The setting `security.seal_secrets` (System page of the panel) keeps the passwords, tokens, keys, PINs and password hashes the node stores sealed with ChaCha20-Poly1305 and leaves everything else readable. It rewrites the stored values when it is turned on or off, one at a time, and loses nothing if the node stops in the middle; a value says by itself whether it is sealed. The key is a record in the `nvs_keys` partition of the same flash chip: no eFuse is burned and the flash is not encrypted, so this does **not** protect against someone who holds the board and reads the chip; it keeps the secrets out of a copy of the settings or a reading of the store. A node installed with the earlier partition table has no such partition and keeps its secrets as they were.
* **Settings that survive an update.** Right before a firmware is installed (from GitHub or by an upload to the panel) the node keeps a copy of the stored settings next to them, with the version that wrote it and a check number. At every start the stored settings are read with the tolerant reader and the copy too; the copy is used only when the stored text cannot be read at all, or when it loses fewer fields. Fifteen seconds after the start the stored text is read again and compared with what the node expects: a difference (a failed save, a damaged store) is written again once, and the health report says `config_check` (`snapshot` `restored` or `failed`, `reverted` `reapplied` or `failed`) for the first ten minutes.
* **The diagnostic package and the log on request.** An administrator can ask the node, through the server, for its `diagnostics` with `detail` `package` or `log` (the order of "Control from the server", with the same two steps and the same one-time token). The answer carries the memory as it is now, the tasks with the least stack each one has had, why the node last started, the versions it runs, a **short list of settings** and the newest few kilobytes of the log; a node sends at most one every fifteen seconds. The settings are carried the other way round from "everything but the passwords": only the leaves of a fixed list of names (modes, intervals, channels, language, switches) go in, as short text, and the address of the broker only as scheme, host and port; a name that says pass, secret, token, key, pin, user, cert, session and the like is never carried even if its leaf is on the list; every value found under such a name is also washed out of the whole package, the log included, and the log is washed of what looks like a credential (`password=...`, `Authorization: Bearer ...`, a user and password in an address, a long run of hexadecimal digits). A test builds a package from settings full of passwords, keys and names of the house and looks for each one in the answer.
