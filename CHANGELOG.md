# Changelog

All notable changes to this project are documented here.

## [0.5.6] - UART stress guard, pin notes and optional sealing of secrets

- **The secrets the node stores can be kept sealed (optional, off by default):** the setting `security.seal_secrets` (in the panel, with its explanation in the eight languages) seals the passwords, tokens, keys, PINs and password hashes in flash with ChaCha20-Poly1305 (`core/secret_seal.hpp`, checked against the RFC vectors) and leaves everything else readable; turning it on or off rewrites the stored values one by one with nothing lost, even if the node stops half-way. The key is a record in the new `nvs_keys` partition: no eFuse is burned and the flash is not encrypted, so it does not protect against someone who holds the board, and a node installed with the earlier partition table keeps its secrets as they were. Tests on a computer cover the cipher, the settings in both directions and a stop in the middle.
- **Improv over USB says "provisioned" only once the node has joined the network, and gives the address of the panel:** the node leaves a mark, restarts to join, and the page is then told the state and handed `http://<address>/` (or `https://` when the panel serves only HTTPS), or that it could not connect after 45 seconds.
- **The boot-ROM pins are marked:** on the Ethernet board, GPIO 43 and 44 carry the chip's boot text at every reset; the pin lists say so in the eight languages.
- The diagnostic package carries the interval of the meter-settings check (`check_settings_s`).
- **A stress test of the emulated serial ports against writes to the flash** (`tests/test_uart_stress.cpp`): streams at 2400, 9600 and 19200 baud, the worst a line can do (a level change at every bit) and ordinary traffic, edges moved by a tenth of a bit, the edge ring filling while the task that works out the bytes is stopped for tens of milliseconds, and the recovery after a stall longer than the ring. The time the ring outlasts is now a function in `core/soft_uart.hpp` (`max_stall_ms`, `slots_needed`) and the firmware refuses to build with a ring that would not last 50 ms at the fastest emulated speed.
- **The node guide says what is on GPIO 43 and 44 of the Ethernet board** (the default pair of port 9): UART0's pins, where the chip's boot ROM prints about a kilobyte at every restart.

## [0.5.5] - Wrong passwords per minute as a setting, the broker certificate from a file and one table of status codes

- **Fixed: a node with stored settings no longer restarts in a loop at power-up.** The start-up code read the stored settings and the copy made before the last update on the stack of the main task (about 3 KB each, plus the reader's own copy); with settings stored the 16 KB stack overflowed and the node restarted again and again before it could start the network. They now live on the heap.
- **The limit on wrong passwords per minute is a setting of the node:** `system.login_limit_per_minute` (30 as always, 0 for none, at most 120) with its field and its explanation in the panel in the eight languages; the limit of each address and of each person is not touched by it.
- **The certificate of the broker can be loaded from a file** on the Broker page (it is checked for the BEGIN and END lines, a private key is refused, and it must fit the 2048 characters the node keeps); nothing is sent to the node until the page is saved.
- **New checks in the host tests:** the Bluetooth session stays under its lock and a request or an answer never crosses to another connection (`check_ble_session.py`); the routes every node serves, the routes the panel calls and the stand-in node the panel is tried against agree (`check_routes.py`); the guard on the paths of a save also keeps the tolerant read of the stored settings, the serial set-up that makes the Wi-Fi the way in, and the answer to a release without an image for the board.
- **The universal image holds no fleet secret unless asked for:** `tools/build_node.sh generic` adds `secrets/generic.conf` only with `ARMOR_WITH_FLEET_SECRET=1`, names that image `*-local` and says it is never to be published, and a public build is refused when the configuration it ended with holds a secret or a set-up code; the option's help says the same.
- **The server and the stand-in node answer only with the codes of the one table** (`core/http_status.hpp`), and a host test (`check_http_codes.py`) fails when either uses another.
- **The mux profile documents that it has no DE pin:** the ports of the mux board are RS232; a device on RS485 belongs on a direct port.

## [0.5.4] - Diagnostics, held values named and a lighter raw dump

- The repository has a security policy, a code of conduct, a contributing guide and a support file.
- The banner follows the animated style of the ecosystem banners, with the name, the kind and the technologies of the project.
- **ESP-IDF 5.5.5 is the one version:** `tools/build_node.sh` uses the 5.5.5 container, and the firmware guide, the requirements of the READMEs and the manifest name it (it said 5.4 in places).
- **A number the inverter sent beyond what is possible and that the node held at the contract's limit is now named** in `clamped` (for example `["load_percent"]`), so the server shows it as not real instead of as a true 200 %.
- **The raw dump of a port is only made while the panel is looking at it** (for fifteen seconds after each request) instead of 128 lines every second per port.
- **How the last run ended:** after a start that was not clean (a crash, a watchdog, a sag of the supply, a failed allocation, a restart by the node's own guard) the health report carries the cause for the first ten minutes, so nobody has to be at the board with a cable.
- **The memory gate and the core dump in the health report:** whether new panel connections are being refused now and whether a core dump is waiting.
- **Crash report in the panel:** a crash writes its core dump (ELF, with a checksum, up to 32 tasks) to the `coredump` partition (128 KB in the table of the repository); the Update page has a *Crash report (core dump)* card for administrators to look at it, download it or erase it. A node flashed before with the older 64 KB table keeps it and may not fit a long dump.
- **Settings that survive an update:** right before a firmware is installed the node keeps a copy of the stored settings next to them, with the version that wrote it and a check number, and reads both with the tolerant reader at every start.
- **Diagnostic package and log on request:** an administrator can ask the node, through the server, for its `diagnostics` with `detail` `package` or `log` (the same two steps and one-time token as the other orders); the answer carries the memory as it is now and the tasks with the least stack they have had free.

## [0.5.3] - The node protects its memory, and the server can control it

- The checksum and the image are read again from GitHub: the HTTP request buffer is 4 KB, because the request that follows GitHub's redirect carries an address of about a thousand characters.
- **The node protects itself when its internal memory runs short:** the tasks it cannot do without (the broker link, the health report and the node's own work) are made at start-up and Bluetooth is started last; a guard looks for them and for the internal memory and, after a minute of trouble, says it (log and an immediate health report) and restarts the node once, cleanly - never more than three times an hour, then it stays up and repeats the alarm.
- **Bluetooth uses less internal memory and closes by itself:** the Bluetooth host stack is allocated in the external RAM and the controller is limited to the three activities it uses; in mode `setup` it switches off after 10 minutes (`CONFIG_ARMOR_BLE_SETUP_MINUTES`) with nobody connected, or two minutes after a node that has users gets an address, and frees its memory; it is not started when less than 40 KB of internal memory is free nor after a restart the node asked for itself.
- **New panel connections are refused while the internal memory is short** (under 28 KB free or a 12 KB biggest block, open again over 40 KB and 16 KB), before anything is allocated for them; the broker link and the node's work are not touched.
- **A failed allocation restarts the node cleanly once** and the next start reports `oom heap=<free>/<largest>`; during a firmware write, or twice within a minute, it aborts as before. A node that hands nothing to a connected broker for ten minutes restarts through the same limit of three an hour, then stays up, degraded.
- **Last will and birth, retained:** the node says `online` true on `armor/health/<node>` each time it connects and the broker keeps its last will (`online` false) there, so a server that starts later knows the state of every node (the radar node does the same on its presence topic).
- **Memory log and the figures of the health report:** the internal memory, the external RAM and the biggest block (see the Memory section of the node's firmware guide), with the stack of the telemetry task kept at 6144 bytes.
- **Smaller fixes:** the MQTT client of the radar node no longer restarts the node when it cannot be created (it tries again), the report of a sensor's firmware and mode is written under the same lock it is read with, the Improv page says `provisioned` only after the Wi-Fi network was joined, and `tools/check_sdkconfig.py` compares a build with `sdkconfig.defaults`.
- **The server can control the node:** restart, pause, resume, standby (kept across restarts, the node stays connected), identify, diagnostics and maintenance on and off, on `armor/control/<node>/command`, with a one-time token bound to the operation; there is no factory reset, no erase and no deep sleep. While the node is paused or in standby it refuses every battery write (a running operation can still be cancelled), and restart, pause and standby wait for a battery operation to end. The health report says what the node is doing. Rules and host tests are shared with the other nodes; nothing of it has run on a board.
- **Planned maintenance:** before the node goes quiet on purpose (a restart, a pause, a standby, a firmware install or copy, a panel upload) it publishes a retained `armor/control/<node>/maintenance` message (`node_maintenance`: active, why and for how long) so that the server holds back the node-offline alert for a bounded time, and clears it (retained too) when it is back; a node that boots into standby keeps saying so until it is resumed, and a window that is never closed ends by itself. The pure parts are in `control_gate.hpp` with host tests; nothing of this has run on a board.
- **The guard and the server's orders work together:** a node the server paused or put in standby is not restarted by the guard for a task it stopped on purpose (nor one with a restart order on its way; the memory rules stay in force), a restart that was ordered leaves a mark so the next start tells it from a restart of the guard and does not count it against the three an hour, the standby kept in flash survives a restart of the guard, and the planned-maintenance signal goes out before an ordered restart, an install and an upload.
- **Fixed:** the log line that closes the Bluetooth set-up window passed a text chosen at run time as the format of the message, which the logging macro does not accept at build time.

## [0.5.2] - Write to Pylontech batteries through typed operations, off by default

- **Writing to Pylontech batteries, off by default:** the node can do through a battery's console most of what the maker's own tool does - set its clock, clear its counters and logs, restart and shut it down, write its capacity, reset its charge level, change its limits and protections (a whole group at a time, inside the ranges the tool states), switch its outputs, write its serial number, erase its external memory and send it firmware - but only as typed operations, never as a free command (the expert console has a block list). Orders arrive on `armor/solar/<node>/maintenance/command` and are answered on `.../result`.
- **Two steps and a one-time token:** `arm` reads the battery, checks every precondition and answers with the exact operation and a token; `execute` carries the token and the same operation, and the node reads the battery again first. The battery must be the one whose serial number was typed, at rest and with no protection active (restart, shutdown, clearing a fuse fault and switching an output off are allowed in an alarm), one operation at a time per port, a pause after a write, and three wrong tokens in ten minutes lock every order for five.
- **Read before, read back after:** every operation reads what it will change, writes, reads it back and compares; the result is ok, failed (with the real state), partial (a sequence that stopped midway) or retry (nothing was written). The battery's service password is a setting of the node, stored like its other secrets and never shown or sent; every text kept, the transcript included, has it replaced.
- **Firmware:** `updata` and XMODEM-1K with a 16-bit CRC (packets of 1024 bytes, up to 20 resends, a receiver that asks for the old checksum mode or never asks is refused before anything is sent). Nothing is sent unless the battery's model, its chip (its Board and characters 8 and 9 of its serial number must agree) and the image's own header (board name and version) fit together; the serial number is written back afterwards. The image reaches the node in small pieces, each checked, and is used once.
- **Panel:** the Network page has the switch and the service password (eight languages).
- **A task of its own:** reading the orders, building the plans and checking a firmware image run in one task with a large stack (the broker's client and the ports' tasks only hand work over), so that neither of them needs more stack for this.
- **Tested against a simulated console, not a battery:** 2,476 checks drive every operation against a fake battery that answers the way the logs of the maker's tool show (both modes, the refused lines, the restart that returns to the user mode, the bootloader's handshake and its failures), including that the password never appears and that nothing is sent without its preconditions. `docs/BATTERY_MAINTENANCE.md` describes it and `docs/BATTERY_BENCH_TEST_PLAN.md` is the staged plan to try it on a spare battery.

## [0.5.1] - Update from GitHub through long redirects, more of what a Pylontech battery says and the green frames of the Summary

- **More of what a Pylontech battery says:** the counters of its life (`stat`, with the names the console prints), the model, the versions of the board and the software, the number of cells, the current it allows each way and the speed of its console (`info`), the state of health of each cell (`soh`, with the cells the console calls abnormal) and the newest entry of its event log (`data event`, with the protection fields exactly as printed, never decoded). They are tested with literal answers of real batteries (a US2000 with the `pylon>` prompt, a US2000C, a US2KBPL, a worn battery with high counters).
- **A header of two words is one column:** the `pwr` table of an old unit has `LC. St` in its header and `OFF` in its rows; it is now read by the name of the column instead of by luck.
- **New parsers:** `datalist` (the BMS's log as a table, events and history) and `getpwr` (a snapshot of the stack in one answer), tested with the real tables; the panel's console may ask `datalist`.
- **The console still only reads:** `help`, `pwr`, `pwrsys`, `bat`, `info`, `stat`, `soh`, `data` and `datalist`; a test refuses every command of the console that writes.
- **The frame that opens the console at 1200 baud** (`~20014682C0048520FCC3`) is reproduced by `build_frame`, as a test against a value of the maker's own documentation.
- **Speeds:** a Pylontech port may be set to 9600 (RS485 with the battery's DIP switch 1 on) or 115200 (console), and to 19200 for an old unit; the panel says which is which.
- **A silent Pylontech battery may be asleep:** the hint says how to wake it and which pins and speeds to check, and the help of the ports explains the console cable, the wake voltage, the dry contacts (read only), the LEDs and the dump.
- **The dump of what a port heard can be downloaded** as a text file, by an administrator only.
- **Update from GitHub works again when GitHub answers with a long redirect:** the checksum file is fetched through a redirect whose headers add up to more than 5 KB, and the 4 KB header buffer made the HTTP client give up. The buffer is now 20 KB, which the memory settings place in the external RAM (above 16 KB), so it costs no internal memory; a board with no external RAM falls back to 6 KB. The log says how much internal memory was free before and after the client was opened.
- **The real reason is told:** when the checksum or the signature of a release cannot be read, the panel says it was the connection, the size or the HTTP status (new texts in the eight languages), and "the release carries no checksum" is only said for a release that really has none.
- **The Summary draws every value inside a green frame,** like the green label of a port that reports, in every node panel (one shared style rule and one helper).
- **The help of the panel is brought up to date in the eight languages:** the limit of 30 failed sign-ins per minute in total, the tolerant reading of the saved settings after an update, the update from GitHub and the notice that a release has no image for the board, Bluetooth set-up (the derived PIN and the answer only to the phone that asked), the web modes and the certificate fingerprint, and the clock and its time server.
- **The node announces that it is gone:** the connection to the broker carries a last will that tells the server the node is offline, and the server never counts it as a report.
- **No more aborts on a failed initialisation:** the network, the stored settings and the Ethernet start-up degrade with a log line instead of rebooting in a loop.
- **Fewer data races:** the cached lengths of the firmware slots, the timer that restarts the node and the flag that the restart after an update was scheduled are guarded.
- **Tests:** the header buffer against a simulated redirect of 5 KB, the reasons for a checksum or signature that cannot be read (and that "no checksum" is not one of them), the green frames in the Summary, and the words every help text must contain in the eight languages.

## [0.5.0] - Tolerant settings, global login limit, Bluetooth race, missing board image

- Settings kept in flash are read as far as they can be after an update: unknown members are ignored and a value that cannot be accepted falls back alone, so the node never starts over because of one field.
- The panel login limits the wrong passwords of all sources together to thirty per minute.
- Bluetooth answers go only to the phone that asked; waiting requests are discarded on connect and disconnect.
- A newer release without an image for this board is reported in the panel (8 languages) instead of passing for up to date.

## [0.4.9] - Shared node base update

- Shared base update: a wider login table, slot-sized download limit, stricter JSON text and a set-up code kept in capitals.
- The status line of each answer of the panel's server lives in the core with a test that every code it uses has its own line.

## [0.4.8] - Stored passwords follow their host and a bounded PI18 reply

- A stored password is tied to its host: changing the address of the broker (or of a backup broker slot) without typing a password erases the stored one instead of sending it to the new host, and the panel says under the field that the password has to be typed again. The same host (another port, other letter case) keeps it.
- The PI18 general status reply is held to the same limits as the other dialects (a load of 255 % or an 8-digit output power no longer reaches the message that the server would refuse whole).
- The access point bandwidth accepts only 20 or 40 MHz (a value in between was accepted and run as 20); an address in the list of networks that may use the panel is refused when a part has a leading zero, as the address parser of the network settings already did.

## [0.4.7] - Sign-in form for password managers

- The sign-in page is a real form with named fields (`username`, `current-password`), so the browser's or system's password manager offers to save and fill the credentials; the set-up page names its fields the same way for the new password.
- A new tick, "Remember my user name on this device", keeps only the user name in the browser (off by default, forgotten as soon as it is unticked, and the panel works with storage blocked); the password is never stored by the panel.
- A short note under the form, in the eight languages, says that the password is saved only by the browser's own password manager. The existing "keep me signed in" tick is unchanged.

## [0.4.6] - Health report

- Every node publishes a health report about once a minute to armor/health/<node> (the node_health contract): uptime, why it last restarted, free and least free heap, the link and Wi-Fi signal, how many times it connected to the broker, tasks short of stack and the last error it logged.
- The report is built by core/node_health.hpp and main/node_health.cpp, shared by the five nodes through ARMOR-COMMON's firmware base; its decisions are covered by the host tests of tests/test_base.cpp. It has not run on a board.
- The broker link counts its connections for the report.

## [0.4.5] - An optional encrypted link to the broker (TLS)

- New settings mqtt.tls (off by default) and mqtt.ca_pem (one certificate in PEM, at most 2 KB; anything else is refused). With TLS on, the node connects with mqtts://, trusts only that authority and checks that the broker's certificate was made for the address it reached; it waits for its clock first (up to two minutes), because certificates have dates. With the switch off nothing changes: the node talks plain MQTT as before.
- The Broker page of the panel has a switch and a box for the certificate of the installation's own authority, in the eight languages, with the new error texts. The saved backup brokers must be mqtts:// too while the switch is on.
- Host tests of the settings: defaults off, the JSON round trip, line ends from a browser, and the refusal of a text that is not PEM, a chain, a text that is too long and a plain address with TLS on. The handshake has not been run on a board.

## [0.4.4] - An inverter without a warning query says that its warnings are unknown

- A REVO inverter has no warning query: its message now carries warnings_known: false, so the empty list is read as unknown and not as no warnings.

## [0.4.3] - Only the house may use the panel, wrong passwords counted per person, and a panel that works behind the server's proxy

- **The panel answers only the house.** A new setting, *Networks that may use this panel* (`web.admin_sources`), says which addresses may sign in or change anything: empty means the private ranges of a house (10/8, 172.16/12, 192.168/16, link-local, loopback and 100.64/10, where Tailscale lives), otherwise the list of addresses and networks you write (`192.168.1.0/24, 10.66.0.0/24`). Any other address is told `outside_network`, except the public look at the node's identity, so a port opened by mistake on the router does not let the Internet try passwords here. A list that would shut out the person who is saving it is refused (`would_lock_out`).
- **Wrong passwords are counted for the person too**, not only for the address: five wrong passwords for a name slow that name down from any address (and, behind the server's proxy, where every request has the server's address, one mistyped name no longer stops everybody).
- **The panel works behind the server's proxy:** its page, its style, its script, its calls and the firmware upload use relative paths, so the same panel opens at `http://node/` and at `https://house/api/v1/nodes/<id>/panel/` (the Bluetooth and map sockets keep their own paths).
- **The fingerprint of the node's certificate is public:** `GET /api/v1/session` carries `cert_sha256`, which is what the server pins before it opens the panel of an HTTPS node; the panel already shows it in the web card so that it can be compared by hand.
- The new setting, its hint and the two new refusals are in the panel in the eight languages. Tests: 3,090 checks in the host test programs (new ones cover the list of networks, the key of a person and the setting).
- **A login or a set-up must carry the marker of the panel** (`X-Requested-With`), like every request that changes something, so a page of another site cannot sign a person in to an account it chose; the server's own sign-in to a node (update, adoption) sends it.
- **The current password of the account is counted too:** wrong ones in "change my password" are slowed down like those of a login, because a stolen session could repeat them.
- **A user change is checked before it is applied:** a role is no longer changed and then the new password refused.

## [0.4.2] - The emulated serial ports out of reach of the flash, two panel functions that answered an error and winter temperatures

- **The emulated serial ports no longer crash the node during a flash write.** The ring of edges of an emulated port is 16 KB, and on this board a plain allocation of that size is served from the external RAM, which the chip cannot read while the cache is off for a write to flash (the settings, a certificate, an update): the interrupt of an edge that arrived then stopped the node. The ring and the bits being sent now live in the chip's own memory.
- **Reading the settings of a BMS and asking the Pylontech console work from the panel:** the node answered *accepted* with a status line that did not exist and the panel read it as an error; 202 and the other codes the node uses (408, 423) now have their lines.
- **PI18 inverters below zero:** the temperatures of the status may be negative; a status with one was refused as a whole and the inverter disappeared in the cold.
- **ANT-BMS settings:** the capacities of the new BMS were shown in the wrong unit (280000000 Ah); they are read as micro-ampere-hours, and the total of the cycles as milli-ampere-hours.
- **A group of ports behind a multiplexer** sets the speed and channel again after the settings of a BMS or the console were read on its line (the next port ran at the BMS's speed and failed once), and a port that loses the line in the middle of an exchange starts its next turn clean.
- A Pylontech stack of up to sixteen modules can be read cell by cell (the setting stopped at eight); the invert switch of a port is shown as it is in the direct profile.
- A user change that could not be written is told as a storage error, the panel tells every error the node can answer with in the eight languages (a test fails when a new code has no text), the comments say eight languages, ten ports and both boards, and `secrets/node.conf.example` no longer carries a usable set-up code.
- A build option turns off the reset by the BOOT button for a node in a place anyone can reach.
- The documents say what the node does today: the Bluetooth pairing has a passkey and `pv_energy_total_wh` is provisional.
- The Bluetooth note of the panel no longer calls the pairing *just works*: it says what the pairing asks for and what protects it.
- Tests: 3,013 checks in ten programs plus the panel texts; new ones cover the negative temperatures, the units of the BMS settings and the abandoned exchange.
- Shared with the other nodes (from ARMOR-COMMON's firmware base): updates are signed for the project, the board, the version and the hash (a node refuses an older image, another board's image and an address that is not the repository's), the pairing of Bluetooth closes after repeated failures, the random generator keeps away from the converter, a user write is all-or-nothing, a repeated key in a JSON document is refused, four-number firmware versions are understood, Improv over USB on a board with a cable also tells the node to use the Wi-Fi it was given, and the upload by hand is refused while an installation runs.

## [0.4.1] - The Bluetooth PIN in the panel

- **Signed firmware:** a node installs from GitHub only a firmware whose signature verifies. Each release carries, next to the image and its SHA-256, an Ed25519 signature of that hash (`<image>.sig`); the node downloads the image, hashes it and installs it only when the hash is the published one and the signature verifies with the public key of the releases, which is in the firmware (the private key never leaves the maintainer's computer). A release without a signature, or with a wrong one, is refused and nothing changes.
- An administrator can still upload a firmware file by hand in the panel, as before: that is the administrator's own act and is the way out if a key ever has to change.
- The panel tells the two new refusals (no signature, bad signature) in the eight languages.
- The verification (SHA-512 and Ed25519, with their constants worked out by a script instead of typed) is tested on a computer against signatures made by another implementation, with every kind of damage to the message, the signature and the key.
- An administrator can read the Bluetooth PIN in the panel (Bluetooth card, *Show the Bluetooth PIN*), without going to the USB console: it is asked of the node only then and forgotten when hidden; new route GET /api/v1/ble-pin, administrators only; the texts are in the eight languages.

## [0.4.0] - A PIN for Bluetooth pairing

- Bluetooth pairing now asks for a 6-digit PIN that the node prints on its USB console next to the setup code; the link is authenticated and nothing is accepted over Bluetooth without it (until now any phone in range could pair without a PIN).
- The Bluetooth note of the panel explains the PIN in all eight languages.

## [0.3.9] - Portuguese in the panel and a warning for the access point and the station at once

- The panel speaks Portuguese, the eighth language, in every page, hint and message of the settings.
- The overview warns when the node's own Wi-Fi network and its connection to your router are on at the same time (they share one radio and one channel), and the Wi-Fi help explains what that costs and when to switch the access point off.

## [0.3.8] - Slot copy, background update check, and an inverter dialect that is learned only from a real reply

- A firmware slot can be copied to the other one from the panel, and the update of a node waits for the image to be confirmed only after the node has an address or its own access point is reachable, rolling back after five minutes otherwise.
- The station connection that was paused to leave the access point free retries after ten minutes at most instead of waiting forever.
- The firmware check against GitHub runs in the background and the panel asks for its result, so a slow search no longer freezes the panel.
- Saving the settings is serialised, so two browsers saving at once cannot mix their changes.
- Logging out needs the page's own token, which stops another page from logging the panel out.
- The REVO dialect is learned only when the reply parses as a REVO status line, so another inverter's answer no longer locks the wrong dialect.
- The documentation of the emulated ports of the node explains the shared timer and its limits.

## [0.3.7] - Copy-log icon and copy of a firmware slot to the other

- **Copy one firmware slot to the other:** next to the switch between ota_0 and ota_1, the Firmware card of the panel has a button that copies the firmware that runs over the other slot (task of its own, with a percentage, the image checked at the end, no restart, no change of the boot slot); `POST /api/v1/ota/copy` starts it and `GET /api/v1/ota/copy` tells how far it is. The slot sizes shown by the status are measured again after a copy or an update.
- **Node panel:** the log of the panel has a small copy icon (two sheets) that puts the whole log on the clipboard and says "Copied" for a moment; the same panel code is shared by all the nodes.

## [0.3.6] - Absurd numbers and the REVO frame

- A number the inverter sends beyond what is possible (an overload above 200 %, a sensor that sends 255 for no reading) is held at the limit the server accepts, so the message is no longer thrown away just when the inverter is failing.
- In the REVO dialect, a checksum byte that happens to be a carriage return no longer cuts the reply short (about two replies in 256 were lost); tests for both.
- Looking for a new version on GitHub no longer fails with connect_failed when the node was short of internal memory (a browser on the HTTPS panel at the same moment): the TLS buffers now live in the external memory, and the search and the download try three times.

## [0.3.5] - Hardening of the web server, the broker link and the sessions

- The web server gives up a request that announces a body and sends nothing (or an upload that stalls) after fifteen seconds, instead of keeping its only thread for ever.
- The restart after an installation from GitHub no longer depends on a browser asking how far it is: a timer of the node watches the installation and restarts it when it is done.
- A "remember me" session is no longer taken for free after thirty minutes when the table fills up, and a session token that starts again replaces the old one instead of living twice.
- On a saved broker that is not the first one, the node tries the first one again every half hour instead of forgetting it until the next restart.

## [0.3.4] - No deadlock between the node and the broker

- **A message is never published while a lock of the node is held:** `mqtt_link::publish()` puts the message in a short queue (48, the oldest dropped first) and a task of its own hands it to the client. Before, a task that held a lock of its module and published could wait for the client's own lock while the client, running the handler of an incoming command, waited for that module lock: both for ever, with nothing to restart the board.
- **The task watchdog restarts the board:** it panics after 10 s (it only printed before).

## [0.3.3] - What the inverter says about itself, and the shared improvements

- **The inverter's serial number, firmware, ratings and settings, and the energy its panels have made in all** are asked after the readings (standard dialect only, never at their cost): `QID` and `QVFW` once, `QPIRI` (rated output, the battery's voltages and currents, the type of battery, the priority of the output source and of the charger, the input range) every ten minutes, because it can be changed at the unit, and `QET` every five. A question that gets no good answer twice in a row is dropped and the reading goes out either way. The message carries them (`serial`, `firmware`, `pv_energy_total_wh`, `ratings`) as optional fields of the shared contract. `QET` is read in Wh, as the protocol gives it; that has not been confirmed on a real inverter.
- **Waits that grow between tries.** A Wi-Fi network that does not answer is tried again after 1 s, then 2, 4, 8... up to 30 s (a little scatter on every wait, so that many nodes that lost the same router do not all knock together), and the broker after 1 s growing to 60 s; a try that works starts the schedule over. Before, every retry was a fixed 3 s (Wi-Fi) or the client's fixed 10 s (broker).
- **Improv Wi-Fi over the USB cable.** A web page that speaks the Improv standard (ESP Web Tools) can give the node its Wi-Fi network through the same USB socket the log comes out of: the node answers its state, identifies itself, describes the device and takes the SSID and password (checked and stored through the same settings document the panel and the Bluetooth set-up use), then restarts to join. A packet is only acted upon whole and with a right checksum. The packets are tested on a computer; the USB side has never run on a board.
- **The stack each task has left**, in the status (`tasks`: the least room ever left, smallest first, with a `low` flag under 512 bytes) and as a warning in the log, once per task, before a near overflow becomes a crash.
- **Tests:** the parsers, the order and the schedule of the questions, a unit that answers NAK, says nothing or garbles to each of them, and the other dialects, which are not asked; the schedule of retries, the Improv packets and the stack report.

## [0.3.2] - A warning for a pair that mixes hardware

- **The ports page warns when the two ports of a pair ask for different hardware** (mux profile, ports 1-2, 3-4, 5-6 and 7-8): each MAX3232 of the base board serves one pair, which is fitted either as RS232 (Pylontech, inverter) or as TTL (ANT-BMS), not both. With both ports of a pair switched on, one on an ANT-BMS and the other on a Pylontech or an inverter, each card says so; a port with the raw kind does not count. It is a hint, not an error: the settings are still saved. In the seven languages.

## [0.3.1] - LEDs 9 and 10 on pins both boards have

- **The default pins of LEDs 9 and 10 are GPIO 45 and GPIO 3** (0.3.0 said GPIO 8, which is wired to the camera connector of the Waveshare ESP32-S3-ETH, is never offered there and so made the default settings invalid on that board). Both are strapping pins that only read at reset: an LED behind a 470 ohm to ground does not pull them the wrong way. Still changeable in the panel. Not run on a board.

## [0.3.0] - LEDs 9 and 10 on their own pins

- **LEDs 9 and 10 (the two inverter ports) hang straight on a GPIO each** (defaults GPIO 45 and GPIO 3, settings `leds.led9` and `leds.led10` in the panel; -1 leaves one out), so the base board needs no second 74HC595 for two LEDs. The register goes back to eight bits (`shift_out`) and drives LEDs 1 to 8; bits 8 and 9 of the pattern go to the two pins. This replaces the chained second register of 0.2.9. Both pins are checked against the other pins like any other. Not run on a board.

## [0.2.9] - Ten LEDs

- **LEDs 9 and 10 for the two inverter ports**, on a second 74HC595 chained behind the first one (its SER on the first one's QH', the same clock and latch, LED 9 on its QA and LED 10 on its QB): the node shifts sixteen bits every time (`shift_out16`), so a board with a single register is unaffected. The documentation says why an LED on the first register's QH' cannot be a ninth LED (it repeats LED 8).

## [0.2.8] - The second select line of group B moves to GPIO 46

- **The base board's group B takes GPIO 46 by default** (0.2.7 said GPIO 4, which is a pin of the microSD socket of the Waveshare ESP32-S3-ETH and does not reach its header, so a board with both footprints could not wire it). GPIO 46 is on the header of both boards and reads low at start, so with a 100 kohm to ground the multiplexer sits on its first channel until the firmware takes over. The documentation says so, and the header of the Ethernet board is no longer described as having the microSD pins.

## [0.2.7] - The base board with ten ports

- **The mux profile serves ten ports** in groups of 4, 4 and 2 (batteries 1 to 8 and inverters 9 and 10), the shape of version 1.0 of the base board: group B gets its second select line (GPIO 4 by default; GPIO 37, which that version of the board wires, belongs to the octal PSRAM of both boards and is refused), so its four ports are reachable. The ports of a group were already limited only by its channels; what changed is the defaults, the number of ports the settings allow in this profile and the words of the panel.
- **Docs:** `NODE_HARDWARE.md` describes the board - which MAX3232 serves which sockets and which TTL connectors, the rule that a converter and its two connectors are never used together, what the firmware does with each variant - and lists what was found reading its netlist (the pin, the unconnected 5 V, the open connectors, the floating select lines, the decoupling). Nothing has been tried on a board.

## [0.2.6] - A Pylontech battery that sleeps is woken, and the node can update itself from GitHub

- **The node can look for a newer firmware on GitHub and install it** (*Firmware and log -> Check GitHub for a new version*), next to - never instead of - the upload of a file: it asks the repository's latest release, offers it only when it is newer and the release carries the SHA-256 of the image, downloads **the image built for this board** (`armor_solar-<board>.bin`) straight into the other firmware slot with a progress bar, and installs it only when the hash matches; then it restarts into it. Administrators only; it needs the node to reach the Internet. The code is the one the radar node already had, now shared by every node (`main/github_update.*`, `core/semver.hpp`, `core/release_assets.hpp` in ARMOR-COMMON's firmware base).
- **The panel and its tests:** the *Firmware and log* page has the card in the seven languages, the stand-in node (`tools/panel_mock.mjs`) answers the three routes (`GET /api/v1/ota/check`, `POST /api/v1/ota/install`, `GET /api/v1/ota/progress`), and the host tests check the comparison of versions and, for a release with and without an image per board, that a board never picks an image that is not its own.
- **A Pylontech console that has been quiet is woken before the first question.** At the start, and after a silence, the poller sends a bare Enter before `pwr`: a console that has been idle may swallow its first command, which looked like a battery that does not answer. The host tests cover it (`test_pylontech_wake`) and every poller sample strips that leading Enter.
- **A node can subscribe to the broker** (`mqtt_link::subscribe`, shared with the other nodes) and the panel's stylesheet gained two small rules; nothing changes for the solar node itself.

## [0.2.5] - More from batteries and inverters, no silence without a clock, and a login over HTTP after one over HTTPS

- **More of what the equipment says now reaches the message.** A battery stack carries its own power reading, the status codes of the MOSFETs and whether a protection has switched one off (ANT-BMS), and the number of cells being balanced (Pylontech, when its console tells); an inverter carries its DC bus voltage. They are left out when the equipment does not say, never made up.
- **A node without a clock no longer falls silent.** It used to drop everything it read until a time server answered, so a node on a network without internet, or one that restarted after a power cut with the provider down, said nothing at all. It now publishes with the time since it started, and ARMOR-SERVER stamps such a message with the moment it receives it; as soon as the clock is set it sends real dates again.
- **The broker link no longer races with itself** when the node moves to another saved broker: the client is replaced under a lock and the old one is stopped outside it, so a message being published at that moment can no longer use a client that is being destroyed.
- **An ANT-BMS with many temperature sensors** (more than six, plus the MOSFET and the balancer) no longer makes the server refuse the whole battery message: the module lists at most eight temperatures, and the mean and the extremes still count all of them.
- **The panel's help** (*Firmware and log*) describes the firmware slots and their switch, the copy of the log, in the seven languages.
- **The node says what kind it is.** The public session answer (`/api/v1/session`) now carries `kind` (`radar`, `solar`, `electrical` or `hmi`) next to the node's id, version and board, so Studio's search for nodes in the network can tell a radar node from a solar one before it is added, even when its panel title cannot be read.
- **Copy the log** (*Firmware and log -> Log*): a button copies everything the log shows to the clipboard, in the seven languages. It also works when the panel is opened over plain HTTP, where the browser's own clipboard is not available, through a fallback.
- **A login over HTTP works after one over HTTPS.** The session cookie of the page served over HTTPS now has its own name (`armor_session_tls`); with one shared name, the browser kept the "Secure" cookie of the HTTPS login and refused to let a plain-HTTP page replace it, so the login over HTTP looked as if it did nothing (it worked in a private window).

## [0.2.4] - Switch between the two firmware slots from the panel

- **Firmware slots in the panel** (*Firmware and log -> Update*): a new card shows the two application slots (ota_0 and ota_1) with the version each one holds and which one runs, and a button boots the other one at the next restart - the way back to the version that ran before an update, or forward to the one just installed. It asks for confirmation, needs an administrator, refuses an empty slot or a firmware of another project, and the settings are kept. The same card exists in the radar, solar, electrical and touch-panel nodes, in the seven languages (`POST /api/v1/ota/switch`).

## [0.2.3] - Date and time, flash overview, hints everywhere and a node that can always be reached

- **Date and time:** a new *Date and time* card on the Overview page shows the node's local time and where it comes from (time server, set by hand, or not set yet). The Network page has a new clock card: the time zone (a list of common zones, with summer time handled by itself), the time server on or off, and, with the server off, a button that sets the node's clock from the browser. The clock now starts by itself as soon as the node has an address - before, it only started together with the broker connection, so a node with no broker never had a time. The time server moved out of the broker settings into a `time` section of the settings file; older files that still carry `mqtt.ntp` load as before.
- **Flash memory:** a new card on the Overview page with the flash size, how much the partitions reserve, what is left unassigned and, for each of the two firmware slots, how much of it the image uses, how much is free, which version it holds and which one is running or boots next.
- **A hint on everything:** pausing the pointer over a field, a button or a menu entry now shows a short explanation in the panel's language (all seven).
- **"Load a configuration file"** now lights up under the pointer like the other buttons.
- **Real bug, found on a bench:** a backup Wi-Fi network or broker lost its password the next time the settings were saved, because the form never holds a stored password and an entry that arrived without one replaced the stored one with an empty password. An entry sent without a password now keeps the one it had (same network name; same user on the same slot or address); a different name starts empty. A secured network tried with an empty password answers "no network with compatible security"; that reason is now shown as a wrong password instead of an anonymous failure.
- **Real bug, found on a bench:** the choice of language made in the panel was never written to the node's settings, so the exported file said English while the panel was in Spanish. Choosing a language now stores it at once, with no restart.
- **Real bug, found on a bench:** the node's rescue network disappeared right after signing in to the panel through it. While the node kept looking for the Wi-Fi network it could not find, every attempt scanned all channels, and the access point shares the radio, so the phone lost the network. While someone is connected to the rescue network the node now pauses those attempts and resumes them ten seconds after the last person leaves.
- **Real bug, found on a bench:** after the first set-up by Wi-Fi the node's own network was never seen again. The set-up code was erased the moment the administrator was created, and that code is the password of the node's own network, which could then not start. The code is now always kept (the one derived from the fleet secret, the one built into the image, or a random one stored in flash so it is the same at every start), and the set-up screen accepts a Wi-Fi network for the node to join on every board, also on those with a cable.

## [0.2.2] - About and Help pages; removing a backup actually removes it

- **About:** the node's own identity, firmware version, author and licence.
- **Help:** a tabbed page explaining each part of the menu in plain language, in all seven languages.
- **Real bug (found on ARMOR-NODE-RADAR, fixed here too):** saving only ever added the document's backup Wi-Fi networks or brokers to the ones already stored, never replacing the list - so removing one and saving brought it straight back. Saving now replaces the list exactly as sent. Code only in this release - there is no board of this kind to flash yet.

## [0.2.1] - Staying signed in

- **"Keep me signed in on this browser"** at login: unchecked, nothing changes (30 minutes idle still signs out); checked, the session survives closing the browser and lasts 30 days of actual use. Code only in this release - there is no board of this kind to flash yet.

## [0.2.0] - Chip, flash, PSRAM and bootloader version on the Overview page

- **A new "Hardware" card:** the chip and its revision, how many cores, the flash and PSRAM sizes, and the IDF version of both the running firmware and the bootloader (not the same thing - a mismatch there usually means the wrong board file was built). Read straight from the chip and the bootloader's own descriptor, not stored anywhere. Needed `spi_flash` added to the component's own dependencies; built clean with the real toolchain.

## [0.1.9] - An eye on the login and set-up passwords

- **Sign-in, the admin password and the Wi-Fi password at set-up** now have an eye button that shows what was typed - the one place a mistyped password locks someone out with no other field to check it against.

## [0.1.8] - Download and load a whole configuration

- **An admin can now download this node's whole configuration** (Network page, secrets included) as a .json file - the same document the flash keeps - and load one back into the form before saving. Built for setting up a batch of identical boards from a single bench node instead of retyping Wi-Fi, broker and the rest by hand. The node's own identity (its node ID) is never overwritten by an import: every board keeps its own. `GET /api/v1/config/export` is a new, admin-only route; loading a file changes nothing until the existing Save button is pressed.

## [0.1.7] - A periodic restart the panel can turn on

- **A new System setting:** "do not apply" or every 1, 2, 3, 4, 6, 12, 24 or 48 hours. It is the same clean restart as every other one (Wi-Fi told it is leaving before the radio powers down), just timed instead of triggered by a save or a firmware update - for a board nobody is going to touch for weeks. Settings-schema change only (`system.auto_restart_hours`, 0 by default): a node left at "do not apply" behaves exactly as before.

## [0.1.6] - Backup Wi-Fi networks and backup brokers

- **Up to 3 backup Wi-Fi networks and 2 backup brokers**, in the panel's Wi-Fi and Broker pages: tried in order, the same way a phone or laptop remembers more than one network, only after the one above has failed to connect for a while - never while it still works. Settings-schema change only (`sta.backup[]`, `mqtt.backup[]`, both empty by default): a node with none configured behaves exactly as before.

## [0.1.5] - A proper goodbye to the access point before restarting

- **The node never told the access point it was leaving before a restart:** `esp_restart()` just cuts the radio, with no deauthentication frame sent; some access points get stuck holding the old association and need restarting themselves before the node can rejoin. It now calls `esp_wifi_disconnect()` and gives it a moment before restarting, on every restart path (the panel, a firmware update, the factory reset held on BOOT).
- **The "Restarting..." screen never appeared after a firmware update:** `S.rebooting = true` was set without calling `render()` on that one path - the panel just sat on the old screen until the auto-reload kicked in on its own six seconds later. Now it shows immediately.
- **The default HTTP header limit (512 bytes) was too small for a real browser:** a session cookie plus a modern browser's own request headers can exceed it, which the panel refused outright - a blank page saying "Header fields are too long". Raised to 2048 bytes.

## [0.1.4] - The node finder can tell this is a solar node

- **The panel's own title now says what it is** ("A.R.M.O.R. solar" instead of a generic "A.R.M.O.R. node"). ARMOR-STUDIO's "find nodes on the network" reads this when it probes a candidate, so the Solar menu no longer lists a radar or an electrical node found on the network.
- Built with ESP-IDF 5.5.5.

## [0.1.3] - A login that actually leaves you signed in

- **The session cookie was garbage:** it was built in a function's own local variable, and the HTTP server only keeps a pointer to a header's text, not a copy of it; by the time the response was really sent, that memory had already been reused for something else. The login or the first-time set-up answered "ok", but no browser ever kept a real session - re-entering the panel always looked like a fresh sign-in. The cookie is now kept alive until the response goes out.
- Built with ESP-IDF 5.5.5.

## [0.1.2] - Saving the settings over Bluetooth no longer restarts the node

- **Bluetooth task stack:** the task that serves the Bluetooth channel (`ble-worker`) ran out of stack while saving a Wi-Fi network with a fixed address, and the node restarted in the middle of the set-up (the app said the connection was lost). Its stack is now 16 KB instead of 8 KB.
- **Bluetooth is on at every start while there is no address:** a node set up with a cable that is not plugged in, or whose Wi-Fi was not joined, did not advertise at all and could not be found by the app; it now advertises at every start and stops a couple of minutes after it has an address.
- Built with ESP-IDF 5.5.5.

## [0.1.1] - A node that has just been set up can always be reached

- **A rescue access point.** A node that is set up (it has a user) but has no address 90 seconds after it starts - no cable, no Wi-Fi network it can join, no access point of its own - opens the set-up Wi-Fi `ARMOR-SETUP-xxxxxx` again, protected with the set-up code. Before, it closed its set-up Wi-Fi when the first administrator was created and vanished.
- **Why it did not join the Wi-Fi.** The `hello` of the Bluetooth channel and the panel's status carry `sta_error` (`network_not_found`, `wrong_password` or `failed`).
- **Bluetooth stays on while there is no address.** A node set up to join a Wi-Fi network keeps advertising until it has an address, and stops two minutes after it has one, so the app can come back and read the outcome.
- **A fixed address on the Wi-Fi too.** DHCP or a fixed address is valid whichever connection the node uses (it was only honoured on the cable); the panel offers it for both.
- Same change as ARMOR-NODE-RADAR 0.3.2; built with ESP-IDF 5.5.5.

## [0.1.0] - A board that boots

- **Boot loop fixed (shared firmware base):** generating the TLS certificate overflowed the main task's stack on the first boot of a board; the buffer is on the heap and the stack is 16 KB. Both images (Wi-Fi and Ethernet) build.

## [0.0.9]

- A GitHub Actions CI baseline (`.github/workflows/ci.yml`): validates the manifest, the version, CHANGELOG.md's heading, the seven README translations' structure and its own local Markdown links, then runs this project's real build/test through `tools/armor_project_tool.py build-test .` (vendored from ARMOR-COMMON, alongside `tools/armor_ci_validate.py` and `tools/_armor_readme_parity.py`, which do the manifest/docs checking).

## [0.0.8] - The base board with multiplexers, the cells in rotation, a second PV input and parallel units

- **The mux profile** for the base board with eight RJ45 sockets: a 74HC4052 multiplexer in front of each of the three hardware UARTs (groups of 4, 2 and 2 ports), the pins of the groups and of the LEDs' 74HC595 in the settings and the panel, and a new setting *Ports of the node* (`profile`: `direct` as before, or `mux`). The ports of a group take turns on the line (`core/mux_group.hpp`): each gets the multiplexer, its own speed and polarity and an empty UART, and runs until its cycle is over; a silent port costs its own timeouts and nobody else's. One LED per port through the 74HC595: a pulse for a good reading, fast blinking for a failed one, dark when the port is off. **Signals arrive inverted** per port, for a TTL device wired through a MAX3232.
- **The cells of a tall stack in rotation** (`cells_per_cycle`): each cycle reads the cells of that many Pylontech modules, in turn, and the others keep those of their last turn.
- **The second PV input (`QPIGS2`) and the units of a parallel system (`QPGS0`...)** for an inverter in the standard dialect, both off by default and never at the cost of the reading: `pv2_*` in the message (and `pv_w` as the sum of both inputs), `units` and the totals of the whole system. The contract of ARMOR-COMMON, the server and Studio know them.
- **Not read: `pwrsys`.** Its output is not described in any public document that could be found, so no parser was written from a guess. Instead, the panel's **Ask the battery** sends one question to a Pylontech console from an allow-list of commands that only read (`pwrsys`, `pwr`, `help`, `bat`/`info`/`stat`/`soh`/`data` with a module) and shows the answer as it came, so that it can be captured from a real battery (`core/console_probe.hpp`, 96 checks; the text on the wire is rebuilt from the list, never copied from the input).
- **Tests:** 2,548 host checks in all: 1,630 of the mux profile (the settings, who has the line, the LEDs, the rotation), 57 of the two optional readings, 96 of the console question, and the messages with them are accepted by ARMOR-COMMON; the panel was exercised in a real browser against a stand-in node (`tools/panel_browser_test.mjs`, 16 checks).
- **Not done:** none of it has run on a board.
- The hardware notes say why the eight-port base board has no isolation (the Pylontech ports and the inverter's RS232 are isolated at the equipment) and what would still need it (a port that shares the pack's negative, like the ANT-BMS one).

## [0.0.7] - Configuration from a phone over Bluetooth

- **Bluetooth Low Energy (NimBLE), the same channel as ARMOR-NODE-RADAR's node:** a phone running the ARMOR app finds the node as `ARMOR-XXXXXX` and sets up its name, Wi-Fi station, address, broker and the rest, with the same set-up code and the same users as the panel (`docs/BLE_PROVISIONING.md`). The link asks for encryption before anything is written; every operation but `hello` needs the set-up code or a login, changing anything needs an administrator, and wrong passwords are throttled.
- **A new setting, `ble.mode`:** `setup` (the default: the node listens only while it has no user), `always` or `off` (the Bluetooth stack is not even started). It is in the panel's Network page, in seven languages.
- **Tests:** 68 more host checks (the framing, the requests, who may ask what, and the setting).
- **Not done:** the radio side has never run on a board and no phone has talked to it.

## [0.0.6] - More inverters and batteries: three inverter dialects and the real Pylontech console formats

- **Three dialects for the inverters, read only.** The port of an inverter now has a *Protocol* setting: **auto** (the default: the node asks in PI30, then in PI18, and keeps to the one that answers; it looks again after three silences), **PI30** (Axpert, PIP, MKS and clones: what the port always spoke), **REVO** (the same frames with another arrangement of `QPIGS`, no warning list, and replies that end with a one-byte checksum instead of the CRC: a reply that fits only the checksum makes an *auto* port REVO) and **PI18** (InfiniSolar V, LV5048, SunGoldPower and clones: `^P005GS`, `^P006MOD`, `^P005FWS`, replies `^D...` with the length in front and a CRC). A whole reply of the other dialect (even a refusal) makes a searching port change at once. The port says which one it found; a dialect chosen by hand is never given up.
- **PI18 readings:** the 28 fields of the general status (grid, output, battery, both PV inputs added up, temperatures, the charger states, the direction of the battery's power), the working mode (power on, standby, bypass, battery, fault, hybrid) and the fault code and 16 warnings by name. **Only reading requests exist**: the frames that set something (`^S...`, `PCP`, `POP`, `MUCHGC`...) are refused by the builder and built nowhere.
- The warning list of the older dialect now names the bits the reference projects list past the 32nd (PV loss, battery derating, the three *battery weak* bits, battery equalisation), and accepts 32 to 36 flags.
- **Pylontech consoles are read by the names in their header.** The newer firmwares (the US5000 V2.3 among them) put an *Id* column after Tlow, Thigh, Vlow and Vhigh, add MosTempr, M.T.St and SysAlarm.St and can leave out others: the columns are found by name, the Time column is read as its two words, and the older layout is assumed when there is no header. The MOSFET temperature joins the module's temperatures and every state column (cells, temperature, MOSFET, system alarm) can raise the alarm.
- `bat <n>` gives the remaining charge (the *Coulomb* column in mAH) and the cells being balanced; `info <n>` gives the model and the rated capacity from its *Specification* line (`48V/74AH`); the cycles come from `stat <n>`. The identity and the history hardly change, so they are asked once and kept for half an hour: a cycle asks only `pwr` and `bat <n>`.
- **Tests:** 684 host checks (121 + 480 of the node + 83 of the Ethernet profile). The new ones take the console text real batteries printed (`info` and `stat` of a US2000C of four firmware versions, an US2KBPL, an US3000C and a 24 V unit, captured with the maker's own program), the samples the samples the mpp-solar and esphome-pipsolar projects publish, build the frames with their CRC or checksum, and run the whole exchange against stand-in inverters that answer in one dialect, in another, with a refusal, garbled, or not at all; and a US5000 V2.3 console with its Id columns.
- **Not done:** any of this against a real inverter or battery; the parallel and second-PV requests of the PI30 family (`QPGS`, `QPIGS2`); the rating commands (`QPIRI`, `PIRI`); the stack's own summary of the newer Pylontech firmwares (`pwrsys`); anything that writes.
- **What real batteries print, from the maker's own program's logs** (`info` and `stat` of more than twenty units: US2000C with four firmware versions, US2KBPL, US3000C and a 24 V one; every console speed is 115200): the console echoes every line followed by an empty one, some fields are empty, the prompt is `pylon>` or `pylon_debug>`, and a battery calls what is sold as US2000B *US2KBPL*. All of it is now a test.
- **An answer is whole when the console's prompt comes back** (`$$` and `pylon>` or `pylon_debug>`), not when the text says "Command completed": a command the battery does not know (`Invalid command or fail to excute.`) is over at once instead of after the timeout, and nothing of one answer is left to be taken for the next.
- **Battery health.** `stat`'s *Pwr Coulomb* is the capacity the battery has learned, in milliampere-seconds (a new 50 Ah module says 180000000); against the rated capacity of its *Specification* it is the module's `health_percent`, and the stack's is their mean. In the logs, some modules were at 56 % and 69 %.
- `bat <n>` rows with fewer columns (index, voltage, temperature and two states) are read too.
- **Tests:** 697 host checks (129 + 485 of the node + 83 of the Ethernet profile).

## [0.0.5] - Reading the settings of an ANT-BMS (read only)

- **The node can read a BMS's settings, and only read them.** On the *Serial ports* page an ANT-BMS port has a button *Read the BMS settings* (an administrator's): the node asks the BMS for its **model and software version** and for its **56 protection, warning, balancing and capacity registers** (cell and pack over- and under-voltage with their recoveries and second levels, currents and delays, short-circuit, state-of-charge warnings, balancing voltages and currents, number of cells, capacities), one read request at a time, and shows them as a table with their units. The port's ordinary readings wait meanwhile; a register that does not answer is counted and left out; three silences in a row end it.
- **Nothing is ever written.** The only request built is function 0x02 (read); the write frames of the BMS and its password are not built anywhere. Changing a BMS's or an inverter's settings is a separate step for later, to be studied with the equipment and tried in isolation. Only a BMS of the newer protocol answers; the older one has no settings request here (the panel says so).
- It runs as an operation of the port's own task (`POST /api/v1/ports/ant-settings?port=N` starts it, `GET` says how it goes and gives the result), so the web server is never blocked. Seven languages.
- **Tests:** 600 host checks (100 + 417 of the node + 83 of the Ethernet profile). The 56 read requests are compared byte for byte with the ones a real app sent (captured by the esphome-ant-bms project), the documented answer's CRC is reproduced, the device information decodes from a real capture (a 48-byte frame whose length byte says 32: it is read by position, without its CRC), and the reader is run against stand-in BMSs that answer everything, only some registers, nothing, no device information, and answers for another register.
- **Not done:** any of this against a real BMS; the old protocol's settings; anything that writes.

## [0.0.4] - One firmware for two boards: Wi-Fi and Ethernet

- **The Waveshare ESP32-S3-ETH is supported** next to the ESP32-S3-WROOM-1 N16R8. The firmware is one code base with two **board profiles** chosen when the image is built: `tools/build_node.sh generic s3-wifi` (the default) writes `dist/generic-s3-wifi.bin`, `tools/build_node.sh generic s3-eth` writes `dist/generic-s3-eth.bin`. An image is for ONE board: flash it only to that one.
- **The `s3-eth` profile** brings the W5500 Ethernet driver (the same code as ARMOR-NODE-RADAR's, on the same board), the Ethernet way in with DHCP or a fixed address (checked as a whole: address, mask, gateway, DNS), the layouts *ethernet* and *ethernet + access point* (the access point is a network of its own for reaching the node from a phone, not bridged to the cable), and the pin table of that board: GPIO 9 to 14 (the W5500) and 8 (the camera connector) are never offered, GPIO 4 to 7 are the microSD socket. The ports' default pins move to free pins; the tenth port has no free pair left and sits on strapping pins (the panel warns).
- **The panel** shows the connection card (cable or Wi-Fi, DHCP or a fixed address) and the board's name only on the board that has a cable; the set-up needs no Wi-Fi there. The `s3-wifi` image is unchanged for the user and refuses an Ethernet setting (`not_available`).
- **Tests:** 349 host checks (100 of the protocol library, 166 of the node on the `s3-wifi` profile, 83 of the `s3-eth` profile: its pin table, that every default pin of the ten ports is usable and none is shared, the address checks, the settings document, the layouts).
- **Not done:** a run on either board. The Ethernet path (W5500 on SPI, the link, DHCP and a fixed address, the interrupt service shared with the emulated UARTs, whose edges may be delayed during a flash write on this board) is untried. `docs/NODE_FIRMWARE.md` has the bench checklist for both boards.

## [0.0.3] - The firmware of the solar node

- **The firmware of an ESP32-S3-WROOM-1 N16R8 (Wi-Fi only), with its web panel.** It reads equipment through **ten serial ports**: three hardware UARTs (any pins) and seven emulated ones (an interrupt on every edge of the RX pin; one hardware timer shared by all of them shifts out the TX bits, one port sending at a time; up to 19200 baud), each independent and each an *inverter* (Voltronic / MPP Solar), a *battery* (Pylontech console), an *ANT-BMS* battery or a *raw monitor* that only listens and shows what arrives, for a protocol that is not decoded. A node may read only inverters, only batteries or a mix; RS485 through a driver-enable pin.
- **A reading is made only when whole:** an inverter needs `QPIGS`, `QMOD` and `QPIWS` all answered (a missing warning list must not look like no warnings), a battery is `pwr` plus, for each module that is there, `bat` and `info` (the extras may go missing without losing the reading). A silent port, a refused request (NAK), a wrong checksum and an answer that is not the expected one are counted and named, and the port says *waiting*, *reporting*, *silent* or *garbled*.
- **ANT-BMS batteries, over the cable at 19200 baud (3.3 V UART).** Both protocols of its firmware are decoded (`core/ant_bms.hpp`): the old 140-byte frame with its sum and the new `7E A1` frame with its CRC-16; the port asks in the old one and, when nothing answers, in the new one, keeps to the one that answered and looks again after three misses. The reading has the cells, the temperature sensors, the state of charge, the current, the capacities (only when the BMS knows them), an estimate of the cycles, and an alarm while a MOSFET is off for a protection; the ports page shows the protocol found and the MOSFET and balancer codes. Tested with frames captured on four real models by the AntBms-Arduino and esphome-ant-bms projects; nothing has been read from a battery by this project. Reading only: the write commands are left out on purpose. The battery message gains `temperatures_c` per module (already in the shared contract).
- **The panel of the radar nodes, with two pages of its own:** set-up with a code shown on the USB console (and the Wi-Fi network the node is to join; it keeps its own network `ARMOR-NODE-SOLAR-xxxxxx` with the setup code as password so it can always be reached), users, Wi-Fi station and access point, broker, update over the air with rollback, log, HTTPS; **Serial ports** (ten of them: each port's equipment, speed, pins, live counters and *what it hears* as hexadecimal) and **Readings** (the last message of every port, with the cells and the capacities). Seven languages.
- **Tests:** 254 host checks (100 of the protocol library, 154 of the node: settings and their checks, the ten ports, the network plan, the emulated UART with jittered edges at four speeds, the exchange with stand-in equipment including every fault above, the ANT-BMS decoder against frames captured on four real models of both protocols) and what the ports make is accepted by ARMOR-COMMON. `tools/panel_mock.mjs` is a stand-in node for the panel; `tools/build_node.sh generic` builds the image.
- **Not done:** a run on a board, any real inverter or battery, the Wi-Fi, the TLS panel and the update on the chip, the timing of the emulated UARTs under Wi-Fi load and with several of them receiving at once; the ANT-BMS's Bluetooth link (the node uses its cable, the more stable one) and its write commands; the Android solar tab. `docs/NODE_FIRMWARE.md` has the bench checklist.

## [0.0.2] - Cells, capacities and the messages in the shared contract

- **Each cell of each module.** `bat <n>` is decoded into the voltage and temperature of every cell, and `info <n>` into the model, the remaining and the full capacity (mAh), the cycles and the state; a stack reports the sum of its capacities, its energy in kWh and its cycles.
- **The messages are now part of ARMOR-COMMON** (schemas and conformance vectors), and the samples this library prints are validated against them.
- Tests: 100 checks. Still not run against a real battery or inverter, and ANT-BMS is still missing (it arrives in 0.0.3).

## [0.0.1] - The protocols and the design

- **Voltronic / MPP Solar inverters** (Axpert, PIP, InfiniSolar and clones), RS232 at 2400 baud: the frames with their CRC-16/XMODEM (and the rule for a CRC byte that would be a delimiter), a framer that finds replies in a byte stream, and the readings `QPIGS` (grid, output, battery, PV, status bits), `QMOD` (mode), `QPIWS` (warnings and faults by name) and `QPIRI` (ratings). Only reading commands can be built.
- **Pylontech batteries** (US2000, US3000, US5000): the table of the console's `pwr` command, summarised as one stack, and the RS485 frame with its length check and checksum.
- **The messages of a gateway node**, contract version 0 (`armor/solar/<node>/<device>/state`), serialised and checked by `tests/check_samples.py`.
- **The design of the rest**, in `docs/`: the protocols and their limits, the boards and the safe wiring of RS232 and RS485, the messages, and the plan for Studio's solar menus.
- **Tests:** 83 checks, including the CRCs of the reading commands and of `(NAK` that the protocol document lists on the wire.
- **Honest limits:** nothing has been connected to a real inverter or battery; there is no node firmware, the server does not read these messages and Studio has no solar menu; ANT-BMS is not decoded because its protocol document is not in the project.
