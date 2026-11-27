# Writing to Pylontech batteries

The node can do, through the console of a Pylontech battery, most of what the maker's own tool (BatteryView) does: set its clock, clear its counters, restart it, write its capacity, change its
limits and protections, switch its outputs, write its serial number, erase its external memory and send it new firmware. It is **off** unless the owner turns it on, in three independent places, and
every operation is a *typed* one: there is no free command, except the expert console, which has a block list.

**Nothing in this document has been tried on a real battery.** Everything is tested against a simulated console (`tests/battery_bench.hpp`) that answers the way the real console's logs show; a battery
may answer differently, and the first thing a bench test finds is where. The staged plan to try it on a spare battery is in [BATTERY_BENCH_TEST_PLAN.md](BATTERY_BENCH_TEST_PLAN.md). Changing what a
battery keeps can void the maker's warranty, and some of it cannot be undone.

## The three locks

1. **The node.** The setting `battery.write_enabled` (Network page of the node's panel, administrators) and the battery's **service password**, which the console asks for before it accepts a change.
   The password is stored only in the node, like the other secrets of its settings; the panel only learns that there is one, and it appears in no answer, no log, no message and no transcript.
2. **The server.** `ARMOR_BATTERY_WRITE=1`. Without it the server sends nothing and every route says so.
3. **The broker.** Its access list has to let the server publish `armor/solar/<node>/maintenance/command` and let the node publish `.../result` (see ARMOR-COMMON's `docs/CONTRACTS.md`).

## How an operation goes

```
Studio / phone -> server -> armor/solar/<node>/maintenance/command -> node -> console of the battery
   (administrator,   (checks, audits,      arm                          reads the battery, checks every
    fresh proof,      keeps the record)     execute + token             precondition, answers with a token
    serial typed)                                                       and the exact operation; then writes,
                                                                        reads back and answers
```

* **Arm.** The server sends `arm` with the operation, its values and the serial number the person typed. The node logs in to the console, reads `info`, `stat`, `time` and `totalcoul`, checks the
  preconditions and answers with a one-time token, the operation in words (`canonical`), its risk and the battery's identity. Nothing has been written.
* **Execute.** Within two minutes the server sends `execute` with the same operation, the serial number again and the token. The node rebuilds the operation from the identity it read when it armed,
  checks that it is the same one, spends the token (a wrong one withdraws the arm and counts towards a lock) and runs the plan.
* **The plan** is a list of console lines, each with the prompt it waits for and what its answer must contain (`Command completed successfully`, `Test result:pass`). A line the battery refuses
  (`Invalid command or fail to excute.`, `Unknown command`) ends the run. Before the writes the plan reads the battery; after them it reads back what it wrote and compares.
* **The result** is one of: `ok` (written and read back as written), `failed` (a step failed before anything was written, or the read-back differs: the answer carries the real state), `partial`
  (a sequence that writes stopped midway: the transcript says where), `retry` (the battery did not answer and nothing was written).

## What is checked before anything is written

| Check | Refusal |
|---|---|
| The port is set up for a Pylontech battery and a module answers on it | `not_pylontech`, `no_battery` |
| The model is one the node knows and the battery says its firmware | `unknown_model`, `unknown_firmware` |
| The serial number typed is the one the battery reports (a battery with none: `NOSERIAL`, only for the operations that allow it) | `serial_mismatch`, `serial_unreadable` |
| The current is near zero (1 A), for the operations that change how the battery measures or protects | `not_idle` |
| No protection is active, except for the recoveries (restart, shutdown, clearing a fuse fault, switching an output **off**) | `critical_alarm` |
| One operation at a time per port, and 30 seconds after a write | `busy`, `cooldown` |
| Three wrong tokens in ten minutes lock every order for five | `locked` |
| The node knows the time (for the operations that write it) | `clock_not_set` |
| For firmware: the model, the chip and the image agree (below). Nothing forces it | `chip_unknown`, `image_mismatch`, `unsupported_model`, `image_missing` |
| The battery is not sitting in its bootloader | `preflight_failed` with the code `bootloader` or `nothing_to_resume` |

An operation that is **atomic** (the reset of the charge level, writing the serial number, erasing the memory, firmware, the board-replacement procedure) cannot be cancelled once its first line has
been sent; the others can be stopped between two lines.

## The operations

| Operation | Console lines | Notes |
|---|---|---|
| `set_clock` | `time YY MM DD hh mm ss` | The time is the node's own, never a value from the order. The line is the help text's form; the read-back (`time`) decides whether the battery took it |
| `clear_stats`, `clear_log`, `clear_data event|history` | `statc`, `logc`, `datac <which>` | The old values are kept in the record of the operation (the read before) |
| `restart`, `shutdown` | `trst`, `shut` | After `trst` the console is back in the user mode and the run logs in again. After `shut` the battery stays silent, which is the expected answer |
| `soc_capacity` | `totalcoul <Ah>`, read back with `totalcoul` | The maker's capacity for the model: US2000C 50, US3000C 74, UP5000 100. Models the maker's procedure names no value for are refused |
| `mosfail_test` | `ctrl mostest on`, read back with `ctrl mostest` | A step of the maker's procedure after a board replacement |
| `soc_reset` | `setlowcfg`, `eepe 0`, `trst`, `login`, `totalcoul <Ah>`, `tbar <serial>`, then reads | The sequence the maker's tool runs to reset the charge level. The serial number is read first and written back |
| `pcb_setup` | `totalcoul`, `ctrl mostest on`, `time ...`, `tbar <serial>` | The steps the maker gives after a board replacement; the serial number is the one on the label |
| `limits_set` | `config <name> <value>` for every value of a group, then `config save`, read back with `config` | A group is sent **whole** (module voltages, cell voltages or temperatures), in milli-volts or milli-degrees, inside the ranges the maker's tool states and in the order it states (the over-voltage protection above the high-voltage alarm ...). The current limits are not offered: the maker's file gives two of them the same console name |
| `limits_default`, `protections_default` | `config default` + `config save`, `prot default` | |
| `protections_set` | `prot <name> en|dis` | Disabling one needs `accept_risk` |
| `output_set` | `ctrl cfet|dfet|buzz|heat on|off`, read back with `ctrl <name>` | Can be put back from the state read before |
| `fault_clear` | `ctrl afuse off` | |
| `write_barcode` | `tbar <serial>` | Changes which firmware image is considered valid for the battery: the serial number tells the chip, and a wrong one can let a wrong image through and destroy it. To read the serial number nothing has to be written: the readings carry it |
| `eeprom_erase` | `eepe 0`, `trst`, `login`, `tbar <serial>` | By default the serial number is written back |
| `firmware_update` | `updata`, XMODEM-1K, `tbar <serial>`, optionally `eepe 0`, then `totalcoul <Ah>` and `ctrl mostest on` | See below |
| `expert_command` | one line, at most 48 characters | The words `updata`, `eepe`, `eepw`, `eepr`, `tbar`, `tmbar`, `flaw`, `flae`, `mm`, `login`, `logout`, `re`, `m2s`, `save`, `rtcw`, `socw`, `soccfg`, `socclr`, `urates`, `setprt` and a few more are blocked: they have a typed operation or must never be sent. With the default `battery.expert_mode` = `blocklist` the words `setlowcfg`, `shut`, `trst`, `statc`, `logc`, `datac`, `totalcoul`, `config`, `prot` and the outputs `cfet` and `dfet` are blocked too, and any line that is not a plain reading (`pwr`, `bat`, `info`, `soh`, `stat`, `help`, `datalist`) needs `accept_risk`; `strict` takes the plain readings only; `off` refuses only the first list |

The lines marked in `core/battery_ops.hpp` as *seen* (`login`, `logout`, `info`, `stat`, `setlowcfg`, `eepe 0`, `trst`, `tbar <serial>`, `totalcoul`, `ctrl mostest on`, `updata`) come from the logs of
the maker's tool on real batteries. The others come from the console's own help text and the tool's configuration files: their exact form is confirmed by the read-back on the first try.

## Firmware

The maker's update tool describes the transfer, and the node does the same: the `updata` command restarts the battery into its bootloader (`BootLoader version ...`, `Program start`), which asks for
**XMODEM with a 16-bit CRC** (the character `C`); the image goes in packets of 1024 bytes (the last padded with `0x1A`), the bootloader answers ACK or NAK, a packet is repeated at most 20 times,
the bootloader may NAK the end-of-transfer once, and it prints `Updata OK` when it has accepted the image and restarted. Anything that is not exactly that ends the transfer: a receiver that asks for
the old checksum mode (nothing has been sent yet), one that never asks, one that cancels, one that stays silent, a message from the battery that says it failed.

**A wrong image destroys the battery, so nothing is sent unless everything agrees:**

* The image is kept first in the server's memory for an hour and sent to the node in 256-byte pieces, each acknowledged and the whole checked against its size and CRC-32.
* The node reads the battery's model, its **Board** (`NF4.E2` or `NF4.E3` on the batteries with the newer chip, nothing on the older) and its serial number. For the US2000C, US3000C and UP5000,
  characters 8 and 9 of the serial number are `E2` or `C3` on the newer chip; for the US5000 they are `C5`. The two must tell the same story; when they do not, the chip is unknown and nothing is sent.
  The Board `ST1` counts as the older chip on the US2000C only, as in the maker's table.
* The US2000 Plus (`US2KBPL`), the US3000A and the other models with no rule in the maker's table are refused (`unsupported_model`), whatever their image says.
* The image carries the name of the board it is for (11 bytes at offset `0x418`) and its version (minor at `0x400`, major at `0x401`). The right image is `US2000C` for the older chip of the first
  three models, `US2000C_NF4` for the newer, `US5000` for the older US5000 and, for the newer US5000, `E32000C_NF4` below version 2.0 and `US2000C_NF4` from 2.0.
* The image comes as a `.bin`; if the maker ships it inside a `.zip`, take the `.bin` out first. The server does not unpack archives.
* After the transfer the node waits for the battery to restart, logs in, writes the serial number again (the update leaves it blank) and reads the battery.

## One module at a time, with the cable moved

The maker's tool updates a stack one battery at a time: the cable is moved to the console of each one. Its logs show batteries whose `info` says a device address other than 1 (8 and 16)
taking `login`, `info`, `updata` and `tbar` on their own console, so the node does the same. The A.R.M.O.R. server keeps the list of a stack (a *campaign*) and Studio walks it module by module.
On a console that answers `pwr` with the sheet of one battery (*slave*) the node accepts only `firmware_update` and `write_barcode`, and takes the current and the `Basic Status` from that
sheet (a status it has not seen counts as an alarm). Before anything is sent `info` has to name the module that was asked for: the serial number typed must be the one the console gives.

After a transfer the node writes the serial number back, then the capacity (`totalcoul`) and `ctrl mostest on`, as the maker's tool does, and reads the battery: the serial number must be the
same, the version the one of the image and the capacity the maker's (`verify:version_differs`, `verify:barcode_differs`, `verify:capacity_zero`, `verify:capacity_differs`). If writing the serial
number fails the battery is left without one: write it again with `write_barcode` on the same console before anything else (a word such as `Specification` is not a serial number).

## A battery left in its bootloader

A refused transfer can leave the battery in its bootloader: it prints `Updata error!`, `Please updata again!` and `Updata is ready!` again and again, answers no command and cannot say who it
is. The node says so (refusal `preflight_failed`, code `bootloader`) and sends no image to it as to a battery just read. The only thing it accepts is **the same try again**: `firmware_update` with `resume: true`, the same image
and the same serial number as the try that failed on that port, which the node keeps in memory. It listens for the bootloader (code `nothing_to_resume` when it is not there), sends no `updata`,
waits up to 90 s for `Program start`, transfers, and never erases the external memory. It can be insisted on up to 10 times (a real battery took its image after several tries). A node that
restarted has forgotten the failed try (`nothing_to_resume`): the maker's tool is then the way. **Do not switch the battery off and do not send it a different image.**

## The node's own panel: not done yet

The panel offers neither the writes nor the stack campaign. What it needs, with the same locks as the server's way (nothing less):

1. Off unless `battery.write_enabled` is on and the service password is set; administrators only, with the password asked again for every step.
2. Routes of the panel (HTTPS only): keep an image (the `.bin`, into external memory, with the same checks of size and header), arm, execute, cancel and the state of the operation. They call the
   same code as the broker's orders (`main/battery_maintenance.cpp`), with the answers going to the request instead of the broker.
3. Two steps (arm and execute with the one-time token, never shown) and the serial number typed at both.
4. A record of who did what, kept across restarts, without the service password.
5. The campaign: the list of modules read from the first battery, one module at a time with the cable moved, a stop at the first failure, the state kept across restarts, and the cable returned.
6. A doubtful chip or an image that does not fit blocks, with no way to force it. The same retry for a battery left in its bootloader.
7. Every text in the eight languages, the route check (`tests/check_routes.py`), the panel text check and a stand-in test of the routes.

## What is left, and what is not done

* **A real battery.** All of the above is proven only against the simulated console. See the bench plan.
* **Reading the limits back.** The format of the console's `config` answer is not known, so the verification only checks that every value written appears in it, and **restoring** previous limits or
  protections is done by running the operation again with the old values (they are in the record of the operation), not by a button. Only the outputs have a *put it back* button.
* **The current limits** (charge and discharge over-current) are not offered, and neither are `emptyvolt` and `blocksoc`: the maker's files give no range for them.
* **A battery group.** The console of a stack answers for the module it is plugged into; the operations apply to that module. The panel of the node does not offer the writes or the campaign: Studio does.
* **The service password** is a setting of the node. There is no way to read it back, and a node that loses its settings has to be given it again.
