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
| For firmware: the model, the chip and the image agree (below) | `chip_unknown`, `image_mismatch`, `unsupported_model`, `image_missing` |

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
| `write_barcode` | `tbar <serial>` | The serial number tells the chip, so a wrong one can lead to a wrong firmware later |
| `eeprom_erase` | `eepe 0`, `trst`, `login`, `tbar <serial>` | By default the serial number is written back |
| `firmware_update` | `updata`, XMODEM-1K, `tbar <serial>`, optionally `eepe 0` | See below |
| `expert_command` | one line, at most 48 characters | The words `updata`, `eepe`, `eepw`, `eepr`, `tbar`, `tmbar`, `flaw`, `flae`, `mm`, `login`, `logout`, `re`, `m2s`, `save`, `rtcw`, `socw`, `soccfg`, `socclr`, `urates`, `setprt` and a few more are blocked: they have a typed operation or must never be sent |

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
* The image carries the name of the board it is for (11 bytes at offset `0x418`) and its version (minor at `0x400`, major at `0x401`). The right image is `US2000C` for the older chip of the first
  three models, `US2000C_NF4` for the newer, `US5000` for the older US5000 and, for the newer US5000, `E32000C_NF4` below version 2.0 and `US2000C_NF4` from 2.0.
* The image comes as a `.bin`; if the maker ships it inside a `.zip`, take the `.bin` out first. The server does not unpack archives.
* After the transfer the node waits for the battery to restart, logs in, writes the serial number again (the update leaves it blank) and reads the battery.

## What is left, and what is not done

* **A real battery.** All of the above is proven only against the simulated console. See the bench plan.
* **Reading the limits back.** The format of the console's `config` answer is not known, so the verification only checks that every value written appears in it, and **restoring** previous limits or
  protections is done by running the operation again with the old values (they are in the record of the operation), not by a button. Only the outputs have a *put it back* button.
* **The current limits** (charge and discharge over-current) are not offered, and neither are `emptyvolt` and `blocksoc`: the maker's files give no range for them.
* **A battery group.** The console of a stack answers for the module it is plugged into (the master); the operations apply to that module.
* **The service password** is a setting of the node. There is no way to read it back, and a node that loses its settings has to be given it again.
