<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-NODE-SOLAR banner" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

<p align="center">
  🇺🇸 <b>English</b> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Solar gateway node: reads inverters and batteries through up to ten serial ports (ESP32-S3 firmware, its web panel and the protocol library)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Version-0.5.9-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Honesty check - what runs today:** The firmware builds in the ESP-IDF 5.5.5 container, its core (the settings, the exchange with each kind of equipment, the arithmetic of the emulated UART, the Bluetooth channel, the release and signature checks: 11,824 checks in 18 test programs) is tested on a computer with stand-in equipment, the messages it makes are accepted by ARMOR-COMMON and the panel was exercised in a browser against a stand-in node. **It has never run on a board and no inverter or battery has been connected**: the Wi-Fi, the panel over TLS, the update, the hardware and emulated UARTs and the formats of the protocols (written from public documents and from memory) are untried. The ANT-BMS frames in its tests were captured by other people on their own units: this project has not read one itself.

---

## 📋 At a glance

| | |
|---|---|
| Version | `0.5.9` |
| Maturity | Functional |
| Role | Solar field node for ESP32-S3: inverters and batteries (family: Field Nodes, parent: ARMOR-COMMON) |
| Stack | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Tests | 11824 checks in 18 host programs and 6 Python checks (`ctest`) |
| Equipment | Voltronic / MPP Solar inverters, Pylontech batteries, ANT-BMS; 10 serial ports |
| Runs on | ESP32-S3-WROOM-1 N16R8 or Waveshare ESP32-S3-ETH; host tests on any computer |
| On real hardware | None yet: no board built and no inverter or battery connected (host tests, the build and the panel in a browser only) |
| License | GPL-3.0-or-later |

**Jump to:** [Overview](#-overview) · [Architecture](#-architecture) · [Security](#-security--safety) · [Interfaces](#-interfaces) · [Configuration](#-configuration) · [Quick start](#-quick-start) · [Testing](#-testing) · [Troubleshooting](#-troubleshooting) · [Structure](#-repository-structure) · [Related](#-related-projects)

## 🚦 Project status

Each line is one part of this repository and the light says how far it is today. It is written from what the repository can show (tests, builds, runs), not from what is planned.

🟢 Done and verified · 🟡 Works, with known limits · 🟠 Partly done or not verified yet · 🔴 Not done or not working

| Area | State | What it means here |
|---|---|---|
| Software | 🟢 | The firmware builds in the ESP-IDF 5.5.5 container for both boards (Wi-Fi and Ethernet). |
| Tests | 🟢 | 11,824 checks in 18 host programs pass (`ctest`): the settings, the exchange with each kind of equipment, the emulated UART, the Bluetooth channel and the release checks. |
| Protocols and data | 🟡 | The Voltronic, Pylontech and ANT-BMS frames, checksums and readings are checked against the documented frames and recorded samples; none has talked to a real inverter or battery yet. |
| Interface | 🟡 | The web panel is exercised in a browser against a mock of the node, not on a board. |
| Hardware | 🔴 | No board built and no inverter or battery connected: nothing has run on real equipment yet. |
| Security | 🟡 | The set-up code, the login with a limit on wrong passwords, the Bluetooth PIN and the signed updates are tested on the host; there has been no independent review. |
| Updates | 🟠 | The hash and Ed25519 signature checks and the rollback logic pass on the host; an update over the air has never been run on a board. |
| Writing to equipment | 🟠 | Writing to Pylontech batteries is off by default and has only been tested against a simulator. |
| Integration | 🟢 | The messages of the node are validated against the shared schemas and conformance vectors of ARMOR-COMMON. |
| Documentation | 🟢 | Eight languages, the firmware guide, the wiring notes and the changelog are up to date. |

## 🎯 Overview

* **Two boards, one firmware:** an ESP32-S3-WROOM-1 N16R8 on Wi-Fi (the default) and the Waveshare ESP32-S3-ETH on its Ethernet cable (DHCP or a fixed address; its own Wi-Fi network is kept to reach it from a phone). The image is chosen when it is built (`tools/build_node.sh generic s3-wifi` or `generic s3-eth`); the pin table, the default pins of the ports and the way in follow the board, and an image is only for its own board. Both build and are host-tested; neither has run on a board.
* **The firmware of the node** (ESP32-S3-WROOM-1 N16R8 on Wi-Fi, or the Waveshare ESP32-S3-ETH on its cable): ten serial ports, three hardware UARTs and seven emulated ones (up to 19200 baud), each independent and each reading an inverter, a battery or a raw monitor, so a node may read only inverters, only batteries or a mix.
* **The node's web panel,** the radar nodes' too: set-up with a code shown on the USB console, login and users, Wi-Fi (station and access point), broker, update over the air with rollback, log, HTTPS, and the pages of the ports (with what each one hears, for protocols not decoded yet) and of the readings, in eight languages.
* **Voltronic / MPP Solar inverters** (Axpert, PIP, InfiniSolar and clones), RS232 at 2400 baud: the frames and their CRC, and the readings `QPIGS` (grid, output, battery, PV, status bits), `QMOD` (mode), `QPIWS` (warnings and faults by name) and `QPIRI` (ratings). Only reading commands can be built: a setting changes how the house is fed. Three dialects, chosen on the port or found by it (*auto*): PI30, REVO (another `QPIGS`, checksum replies) and PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), read only.
* **Pylontech batteries** (US2000, US3000, US5000): the console's `pwr` table (every module's voltage, current, temperatures and state of charge), `bat <n>` (the voltage and temperature of every cell) and `info <n>` (model, remaining and full capacity, cycles), summarised as one stack with its total capacity and energy, and the RS485 frame with its two checks. The formats are written from memory of the public console and may differ between firmwares. The columns are found by the header's names (the US5000 V2.3 layout with its Id columns, MosTempr and SysAlarm.St included), the remaining charge and the balancing come from `bat`, and the model, rated capacity and cycles from `info` and `stat`, asked once and kept for half an hour.
* **What a Pylontech module tells about itself:** its model and the versions of its board and software, its cells and the current it allows, the counters of its life (`stat`), the health of each cell (`soh`, the abnormal one marked) and the newest entry of its event log, with the protection names exactly as the console prints them. The console only reads, and an administrator can download what a port heard as a text file.
* **The messages of the node** (`armor/solar/<node>/<device>/state`, one per inverter or battery stack), defined in ARMOR-COMMON with schemas and conformance vectors; what the ports make is checked against them.
* **ANT-BMS batteries** (the black boards of home-made packs, 7S to 32S), 3.3 V UART at 19200 baud: both protocols of its firmware (the node asks in one and then in the other and keeps to the one that answers), with the cells, temperatures, state of charge, current, capacities and the states of the MOSFETs and the balancer, checked against frames captured on four real models. Reading only: its write commands can disconnect a battery under load. A button on the ports page also reads the BMS's model, version and 56 protection and balancing settings (newer protocol), read only.
* **Configuration from a phone over Bluetooth,** the same channel as the radar node's: the ARMOR app finds the node as `ARMOR-XXXXXX` and sets its name, Wi-Fi, address, broker and Bluetooth mode with the panel's users and set-up code ([the protocol](docs/BLE_PROVISIONING.md)). It listens only while the node has no user, unless told otherwise. The radio side has never run on a board.
* **The base board with multiplexers (the *mux* profile):** three hardware UARTs, each behind a 74HC4052, serve up to eight ports in groups of 4, 2 and 2 that take turns on their line, each port with its own speed and polarity, and one LED per port through a 74HC595; the cells of a tall Pylontech stack can be read in rotation. For an inverter in the standard dialect, the **second PV input** (`QPIGS2`) and the **units of a parallel system** (`QPGS`) are optional readings. None of it has run on a board.
* **Signed over-the-air updates:** the node installs from GitHub only an image whose SHA-256 matches and that carries a valid Ed25519 signature of the maintainer (the public key is in the firmware, the private one never leaves the maintainer); a file uploaded by hand by an administrator in the panel is still accepted. The pages and messages of the panel are in eight languages.
* **Bluetooth pairing with a PIN:** the six-digit PIN is derived from the set-up code, printed on the USB console and shown to an administrator in the panel; the phone has to enter it before it may write the node's settings.
* **Not yet:** the ANT-BMS's Bluetooth link (the node uses the cable, the more stable of the two) and a run on a real board with real equipment.
* **Only the house may use the panel:** the panel answers the private network by default (or the networks its owner lists, such as a VPN), refuses a list that would lock out whoever saves it, counts wrong passwords for each person as well as each address, and works behind the server's proxy so that it can be reached from outside the house without publishing the node.
* **Unknown is not none:** a REVO inverter has no warning query, so its message carries `warnings_known: false` and Studio shows the warnings as unknown instead of "none".
* **Optional encrypted link to the broker (TLS):** off by default, so the node talks plain MQTT as before. A switch and a box for the certificate of the installation's own authority (PEM, at most 2 KB) in the Broker page make it connect with `mqtts://`, trust that authority only and check that the broker's certificate was made for the address it reached; anything that is not one PEM certificate is refused. Covered by host tests; the handshake has not been run on a board yet.
* **Health report:** about once a minute the node publishes how long it has been up and why it last restarted, its free memory, its link and Wi-Fi signal, how many times it connected to the broker and the last error it logged (`armor/health/<node>`, the `node_health` contract of ARMOR-COMMON). The code that builds it is shared by every node and tested on a computer; it has not run on a board.
* **Writing to Pylontech batteries (off by default):** typed operations only - clock, counters and logs, restart, capacity, the reset of the charge level, limits and protections, outputs, serial number, external memory, firmware by XMODEM-1K and an expert console with a block list - in two steps (arm, then execute with a one-time token), with the battery read before and after, the service password kept only in the node and a simulated console for the tests. Untried on a real battery: see the [bench test plan](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Control from the server:** Studio can restart the node cleanly, pause it for up to an hour (it resumes by itself), put its work in standby (no end, kept across restarts, the node stays connected and keeps answering), make it show itself, ask for a diagnostics snapshot and mark it as in maintenance (`armor/control/<node>/...`, the `node_control` contract of ARMOR-COMMON). Every order takes two steps with a one-time token that belongs to that operation, only the server may write them on the broker, and there is no factory reset, no erase and no deep sleep. While the node is paused or in standby it writes no battery (a running operation can still be cancelled) and keeps reading the inverters and batteries. The code is shared by every node and tested on a computer; it has not run on a board.
* **Absurd values are named, not hidden:** when an inverter sends a number beyond what is possible, the node holds it at the contract's limit and names it in `clamped`, so the server shows it as not real; and the raw dump of a port is only made while the panel is looking at it.
* **Diagnostics without a cable:** after a start that was not clean (a crash, a watchdog, a sag of the supply, a failed allocation) the health report says why for ten minutes; a crash leaves its core dump to look at, download or erase from the panel; the stored settings are copied before every firmware install and recovered if an update damages them; and an administrator can ask the node, through the server, for a diagnostic package or its log with a one-time token.
* **Optional sealing of stored secrets:** off by default (`security.seal_secrets`, or the server's `nodes.nvs_encryption`). When on, the passwords, tokens, keys, PINs and password hashes the node stores are kept sealed with ChaCha20-Poly1305 and everything else stays readable; turning it on or off rewrites them with nothing lost, even if the node stops half-way. No eFuse is burned and the flash is not encrypted: the key lives in a partition of the same chip, so it does **not** protect against someone who holds the board, and a node installed before that partition existed keeps its secrets as they were until it is flashed with the new partition table.

## 🧩 Role in A.R.M.O.R.

ARMOR-NODE-SOLAR is the gateway between the house's solar equipment and the family. It reads inverters and batteries through up to ten serial ports and publishes what it reads for [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), which keeps the history, the sums and the alarms; the *Inverters* and *Batteries* menus of [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) and the Solar screen of [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) show it. It only reads: it never writes a setting to any equipment.

| Name | How | What for |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Receives one message per inverter or battery stack; orders the maintenance of a battery |
| ARMOR-STUDIO | menus, panel link | Shows inverters and batteries and opens the node's panel |
| ARMOR-ANDROID-CONTROL | Bluetooth | Sets the node up in the field and shows the energy screens |
| ARMOR-COMMON | solar contract | The messages are checked against its schemas; the common firmware is copied from it |

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="ARMOR-NODE-SOLAR architecture: who calls it, what it holds and what it depends on" width="100%">
</p>

## 🖼️ Screenshots

Captured at 1920×1080 on a test bench, with sample data and no real installation behind them.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Overview: the state of the node at a glance - network, broker, firmware and what it reports." width="100%"><br><sub>Overview: the state of the node at a glance - network, broker, firmware and what it reports.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Ports: the serial ports of the node and the equipment on each of them." width="100%"><br><sub>Ports: the serial ports of the node and the equipment on each of them.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Readings: what the node is reading right now, with the age of each value." width="100%"><br><sub>Readings: what the node is reading right now, with the age of each value.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Network: Ethernet or Wi-Fi addressing of the node, set from its own web panel." width="100%"><br><sub>Network: Ethernet or Wi-Fi addressing of the node, set from its own web panel.</sub></td></tr>
</table>

## 🔒 Security & Safety

| Threat | Defence |
|---|---|
| Passwords in clear or compiled in | PBKDF2-HMAC-SHA256 with 10 000 rounds; no password in the firmware (a node with no user enters set-up); at most 4 users (admin or viewer) and 8 to 64 characters; at most 6 sessions, 30 minutes idle |
| Taking a freshly flashed node | Set-up needs a start-up code: the one of the build (`CONFIG_ARMOR_SETUP_CODE`) or a random one kept in flash and shown on the USB console; with a fleet secret it is `HMAC-SHA256(secret, MAC)`; it is also the key of the set-up Wi-Fi (WPA2, never bridged) |
| The panel reached from the internet | It answers the private networks of a house by default (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 and 100.64/10 where some VPNs live); the setting `web.admin_sources` lists other IPv4 addresses and networks (8 entries, 120 characters); anything outside is refused except the public identity `GET /api/v1/session`; a list that would lock out the person saving it is rejected |
| The panel in clear over HTTP | HTTPS on 443 with a certificate the node makes itself (self-signed, in flash, SHA-256 fingerprint shown to compare); the panel can be http, https or both; the session cookie is always HttpOnly and SameSite=Strict (and Secure over TLS) |
| A fake firmware from GitHub | An image is installed only with a matching SHA-256 and a valid Ed25519 signature made with the maintainer's key (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` refuse it); a manual upload by an administrator is not checked: an explicit limit |
| Hijacking the Bluetooth set-up | A 6-digit PIN derived from the set-up code, shown on the USB console and to an administrator; the phone must type it before writing settings; the set-up window lasts `CONFIG_ARMOR_BLE_SETUP_MINUTES` (10 by default, 1 to 120) |
| A remote order that does harm | Two steps with a single-use token tied to that operation; only the server may write those topics to the broker; a pause lasts one hour at most; there is no factory reset, no erase and no deep sleep by remote order |
| Stored secrets read by someone who has the board | Optional (off by default, switch `security.seal_secrets` or the server's `nodes.nvs_encryption`): ChaCha20-Poly1305 for passwords, tokens, keys, PINs and hashes. It does not encrypt the flash or burn an eFuse: the key is in a partition of the same chip, so it does not protect against whoever holds the board |
| The broker link in clear | Optional TLS (`mqtts://`) with the owner's own authority (PEM up to 2 KB) and a check of the name; off by default. The handshake has only run in host tests, not on a board |
| A battery written by mistake or by an attacker | Three locks: the node setting `battery.write_enabled` (administrators) with the battery's service password, the server's `ARMOR_BATTERY_WRITE=1`, and the broker's access list for `armor/solar/<node>/maintenance/command` and `.../result` |
| A repeated or stolen write order | Two steps (arm, then run with a single-use token), an arming window of 120 s, 30 s of cooldown between writes to a port, 3 failures in 10 minutes lock it for 5; the battery's serial number is typed again, the battery must be idle and without a critical alarm, and a firmware image is kept for 30 minutes and checked |
| Writing to an inverter or an ANT-BMS | The write commands of an inverter (a setting changes how the house is fed) and of an ANT-BMS (it can disconnect a battery under load) cannot even be built |
| Absurd numbers shown as real | A value beyond what is possible is held at the limit of the contract and named in `clamped`, so the server shows it as not real; a REVO inverter has no warning query, so its message says `warnings_known: false` instead of claiming no warnings |

No frame has been read from a real inverter or battery; the formats are written from public documents and from memory, and the ANT-BMS frames of the tests were captured by other people. Read [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) and [SECURITY.md](SECURITY.md).

## 🔌 Interfaces

| Part | What it sets |
|---|---|
| MQTT | `armor/solar/<node>/<device>/state` (one message per inverter or battery stack, kind `inverter` or `battery` of the ARMOR-COMMON contract), `armor/solar/<node>/maintenance/{command,result}`, `armor/health/<node>` (last will), `armor/control/<id>/{command,result,maintenance}` and `armor/control/all/command` |
| Ports | 10 serial ports (3 hardware UARTs and 7 emulated, up to 19200 baud when emulated); per port a type: `voltronic` (2400 baud, every 5 s), `pylontech` (115200, every 10 s), `ant` (19200, every 5 s) or `raw` (9600, listen only, publishes nothing); inverter dialects `auto`, `pi30`, `revo`, `pi18`; the mux profile has 3 groups of ports |
| Queries | Voltronic: `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (and `QPIGS2`, `QPGS` on a standard inverter with the mux); Pylontech console: `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, event log, up to 8 modules asked cell by cell |
| Panel | The family routes (`tests/check_routes.py` checks that every route the panel calls exists) plus the ports and readings pages; 51 error codes with a text in eight languages (772 rows in `panel/text.js`); see [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) for the menus of Studio |

## ⚙️ Configuration

| Variable | Default | What it sets |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | The settings of each port: on or off, the type, the device name in the topic, the speed (0 = the type's own), the pins (and the DE/RE pin of an RS485), the polling period (0 = the type's own), the number of modules and the inverter dialect; at most 3 backup networks and 2 backup brokers |
| `battery.write_enabled` | off | The node's lock for writing to a Pylontech battery (administrators, with the battery's service password, which never leaves the node) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | The board: ESP32-S3-WROOM-1 N16R8 on Wi-Fi (the default) or the Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | The settings every node shares: the name (empty gives `armor-` and the last bytes of the MAC), the set-up code (empty: random), the fleet secret (16+ characters, never in the repository), the first broker settings, the heartbeat (2 to 300 s), the minutes of Bluetooth set-up (1 to 120) and the reset with BOOT (8 s in the first 30 s) |

## 🚀 Quick start

**Requirements:** a C++17 compiler for the host tests; the ESP-IDF 5.5.5 container to build the image; an ESP32-S3-WROOM-1 N16R8 or a Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

See the [firmware guide](docs/NODE_FIRMWARE.md) (the board, the ports, the first start and the bench checklist) and the [wiring notes](docs/NODE_HARDWARE.md).

## 🧪 Testing

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 host test programs and 6 Python checks run under `ctest`; 11824 checks counted by the host programs, built with `-Werror`.

Python checks: the panel texts in eight languages, the tasks, the save paths, the Bluetooth session under its lock and the routes the panel calls (and, in some nodes, the HTTP codes or the glyphs of the fonts)

The host tests need any C++17 compiler (Linux, WSL, MSYS2); the web panel is also exercised in a real browser against a stand-in node (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

`test_clock` prints only `ok` and has no counter; every other program reports its checks. The real frames of the tests are in `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` and `battery_bench.hpp`.

## 🏗️ Build & Release

`tools/build_node.sh` builds the image in the ESP-IDF 5.5.5 container and the release carries the image, its SHA-256 and its Ed25519 signature, one per board; the image is written by USB-C the first time and updated from the panel afterwards.

## 🩺 Troubleshooting

| You see | Because | Do |
|---|---|---|
| You lost the password or the node no longer joins the network | A node with set-up and no address after 90 s opens its own Wi-Fi `ARMOR-SETUP-xxxxxx` (open `http://192.168.4.1/`) | To start again hold BOOT for 8 seconds in the first 30 seconds after power-up (erases settings and users), or use *Factory reset* in the panel |
| An update from GitHub is refused | The release has no checksum or signature, they do not match (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), has no image for this board (`e_no_asset_for_board`) or is for another project (`e_wrong_firmware`) | Nothing changed on the node; try again or let an administrator upload the file by hand |
| A write to a battery is refused with a code | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`...: the node refuses what is not exactly the operation armed | Read the code in the result, fix the cause and arm the operation again (see [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| A port publishes nothing | The port is off, its type is `raw` (listen only), the speed or the pins do not match the equipment, or the equipment does not answer | Check the port settings and the wiring in [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md); the panel shows the raw dump of a port while it looks at it |

## 📂 Repository Structure

<p align="center">
  <img src="images/repository-structure.svg" alt="Repository structure of ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Text version of the tree</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                the firmware: an ESP-IDF component
├── core/                                the logic with no ESP-IDF, tested on a computer
├── panel/                               the web panel embedded in the firmware
├── tests/                               host tests and Python checks
├── docs/                                10 guides: protocols, messages, maintenance, hardware
├── tools/                               build, pack and check tools
├── .github/                             CI workflow, issue and pull-request templates
├── images/                              banner, icon and the two pictures of this README
├── build.sh build.bat                   build through the shared project workflow
├── test.sh test.bat                     run the tests through the shared workflow
├── run.sh run.bat                       start through the shared project workflow
├── armor.project.json                   manifest: version, role, maturity, build command
├── CHANGELOG.md                         what changed in each version
├── SECURITY.md SUPPORT.md LICENSE       security policy, support and license
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   how to contribute and the code of conduct
├── CMakeLists.txt                       the ESP-IDF project
├── partitions.csv                       the flash table
├── sdkconfig.defaults                   the firmware settings
├── sdkconfig.board.s3-eth               the settings of one board
├── sdkconfig.board.s3-wifi              the settings of one board
├── secrets/                             example of the node file (the real ones are not versioned)
├── build-test.sh build-test.bat         run the tests through the shared workflow
└── .gitattributes .gitignore            Git settings
```

</details>

## 🗺️ Roadmap & Known limits

**Known limits today**

* The Voltronic, Pylontech and ANT-BMS frames, checksums and readings are checked against the documented frames and recorded samples; none has talked to a real inverter or battery yet.
* The web panel is exercised in a browser against a mock of the node, not on a board.
* No board built and no inverter or battery connected: nothing has run on real equipment yet.
* The set-up code, the login with a limit on wrong passwords, the Bluetooth PIN and the signed updates are tested on the host; there has been no independent review.
* The hash and Ed25519 signature checks and the rollback logic pass on the host; an update over the air has never been run on a board.
* Writing to Pylontech batteries is off by default and has only been tested against a simulator.

**Next**

* Run the node on a real board with a real inverter and batteries, then the Bluetooth link of the ANT-BMS (the cable is used today), the update over the air and the writes to a battery.

## 🌍 Languages

README and texts exist in English, Spanish, French, Italian, German, Chinese, Japanese and European Portuguese.

## 🔗 Related Projects

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) is a perimeter-security system made of independent repositories. Each one has its own version, its own tests and its own README; this is the family:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Message contracts, validators, conformance vectors and generated types
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Field-node firmware for ESP32-S3 with three radars and its own web panel
* **ARMOR-NODE-SOLAR** (this repository) - Solar inverter and battery protocols and the messages of a gateway node
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Electrical node: meters, the message of the network's readings and the rules for switching
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Alarm node and panel: zones, arming, delays, siren and PIN, with the server or without it
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Touch panel: the state of the system on a wall screen, arming and acknowledging, and the home of the voice assistant
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - The local network: its devices, the internet and what changes
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Central coordinator: telemetry, alarms, devices, solar readings and cameras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web console: cameras, radar, alarms, solar energy and the 2D/3D site designer
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Android operator client with a live 2D/3D radar
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - iPhone operator client
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Visual inference policy that explains its decisions and never actuates
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Camera zones, masks and detection rules for the Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Offline voice intents with a confirmation that cannot be forged
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Enclosures, electronics and the bench acceptance matrix
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Deployment, the CM5 test bench, backup and TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Offline telemetry simulator with repeatable faults
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Detects, installs and updates the ecosystem's own repositories
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architecture, security baseline and the capability matrix

## 📚 Documentation

* [Node firmware](docs/NODE_FIRMWARE.md): the board, the ports, the first start and the bench checklist
* [Wiring](docs/NODE_HARDWARE.md), [protocols](docs/PROTOCOLS.md) and [messages](docs/SOLAR_MESSAGES.md)
* [Battery maintenance](docs/BATTERY_MAINTENANCE.md) and its [bench test plan](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Studio menus](docs/STUDIO_MENUS.md), [bench tests](docs/BENCH_TESTS.md), [set-up code](docs/SETUP_CODE.md) and [Bluetooth provisioning](docs/BLE_PROVISIONING.md)
* [Capability matrix: what is proven and what is not](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Project catalogue: versions and how the repositories depend on each other](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Changelog of this repository](CHANGELOG.md)

## 🤝 Contributing & Community

* Before a pull request run `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py`: it must pass. See [CONTRIBUTING.md](CONTRIBUTING.md) and the [code of conduct](CODE_OF_CONDUCT.md).
* The README exists in eight languages and is generated from the sources in ARMOR-DOCS: a change to the text is made there.
* A change to a message between components starts in ARMOR-COMMON.
* Do not describe as working on a board what has only been built.
* Report a vulnerability privately: [SECURITY.md](SECURITY.md). Questions, ideas and reports: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTHOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENSE

GPL-3.0-or-later - see [LICENSE](LICENSE).
