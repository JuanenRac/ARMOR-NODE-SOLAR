# Bench tests

The software of these checks is done and tested on a computer; what is left needs a real board, a real phone or the CM5. Nothing here has been run on hardware yet.

## Solar node

| Check | Connect | Run | Expected |
|---|---|---|---|
| Serial bytes while flash is written | a serial transmitter at 9600 baud (and 2400 and 19200) | write the settings in a loop from the panel | no lost bytes; measure the interrupt jitter with Wi-Fi active |
| Lateness of the edge interrupt | a jumper from the TX pin of an emulated port to the RX pin of another, and Wi-Fi in use | send a stream of 0x55 at each speed while the node uploads and the settings are saved; for every edge note the time the transmitter made it and the time the interrupt saw it (the difference is the lateness) | the spread of the lateness (the latest minus the earliest) is under 30 % of a bit at the speed in use; `core/isr_jitter.hpp` (`judge`, `fastest_safe_baud`) gives the verdict from the numbers and `docs/UART_JITTER_LIMITS.json` holds the limits; the host test shows a line within the limit is read byte for byte |
| Compatibility table | each inverter and battery model | read the model, the firmware and which commands answer | one row per model in the compatibility table: write the three in the row's `bench` member of `docs/compatibility.json`, set its status to `done` and its level to `bench`, and run `python tools/make_compatibility.py` (see [COMPATIBILITY.md](COMPATIBILITY.md)) |

## Checks every node needs on a real board

Connect the board by USB, open the console at 115200 baud and note the result of each line (OK or FAIL, with the log).

| Check | Connect | Run | Expected |
|---|---|---|---|
| Bluetooth session isolation | the node and a phone with the app | connect and disconnect over Bluetooth 50 times in a row (open "Set up a node", close it) | no restart (the uptime in the node state never drops); the next session never receives an answer meant for the previous one |
| Settings survive an update | a node with the previous firmware | save settings, then update over the air to a firmware that adds a field or makes a value invalid | it starts, keeps the valid settings, drops only the bad field and says so in the log; it does not fall back to factory settings |
| Update for another board | a node of one board (for example Ethernet) | upload through the web panel the image of the other board of the same project | the upload is refused with 422 `wrong_board` (the image carries a mark of its board that the node reads while it arrives, `core/image_board.hpp`); an image from before the marks is refused as `no_board_mark` unless `security.ota_require_board_mark` is off, and the check as a whole is `security.ota_check_board`. The update from GitHub is refused too (the signature covers the board). Needs an image built with the marks on both boards |
| Backup broker | a node with two brokers configured | stop the main broker while the backup one is active, then bring it back | no lost readings, no 30 s without data every 30 minutes, and the node returns to the main broker |
| Router that does not answer ping | a node behind such a router | leave it running 10 minutes | the network watchdog does not disconnect the Wi-Fi |
| Network scan on Ethernet + access point | an Ethernet board in the Ethernet + access point mode | start a scan from the panel | the log shows the empty station is not reconnected and the Ethernet link does not drop |
| Log in a small task | any board | write logs in a loop from a task with a 3 KiB stack (panel, Log) | no stack overflow (high-water mark above 512 bytes) |
| Pins during a scan | a pulse generator on a spare pin | pulse the pin while the network scan runs | no blocking and no watchdog restart |
| Set-up from the browser | a new board and ESP Web Tools | install and give the Wi-Fi credentials | the page says "provisioned" only after the node joined the network and shows the panel address; with a wrong password it says it could not connect after 45 s |
| Session MAC | a wired board | open `/api/v1/session` | the MAC is the one printed with the set-up code on the console |
| Sealed secrets | a node with the option | turn `security.seal_secrets` on and restart | the secrets keep working; without eFuses the key stays in the same flash, which the panel says plainly |
