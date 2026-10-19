# Trying the battery operations on a real battery

Everything that writes to a Pylontech battery has been tested only against a simulated console. This is the order to try it on a **spare battery that can be lost**, from the least risky to the most,
with what to expect and what to keep. Do not go on to a stage until the one before gave exactly the expected result. Stop at the first surprise: a refusal that makes no sense, a read-back that
differs, a transcript with a line you did not expect.

## Before the first stage

* A spare battery (ideally the model you mean to work on), on its own, not in a bank that carries a load, with a charger or a bench supply you can switch off. Wear what you would wear to work on a
  battery of 48 V: it can deliver hundreds of amperes.
* The console cable (RJ45 `pin 3` is the battery's TX, `pin 6` its RX, `pin 8` ground; the RJ11 models differ: check the manual of the model), 115200 baud 8N1, on a port of the node set as Pylontech.
  Not every USB-serial converter works with the maker's tool; the node uses its own UART.
* The node's panel: *Battery maintenance* **on**, and the battery's service password typed in (it is the one the maker's tool uses; the node never shows it again). The server with
  `ARMOR_BATTERY_WRITE=1` and the broker's access list allowing the two topics.
* Studio: Configuration > Battery maintenance, signed in as an administrator. The wizard asks for your password again at the first step, whatever the origin.
* Read the battery first from the node's panel (the one-question console: `info`, `stat`, `pwr`) and write down the serial number exactly as it prints it. Keep these outputs: they are the best test
  fixtures this project can have.

## Stage 1: read, and arm without executing

Arm *Set the clock* and **do not execute** (withdraw it). What to see: the operation goes from *Checking* to *Ready to do* within a few seconds, with the battery's model, serial number and firmware
and `canonical` = `set_clock`. What it proves: the login, `info`, `stat`, `time` and `totalcoul` parse; the serial number matches; the preconditions are right.

If it is refused: `login_failed` (the service password), `no_answer` (cable, speed, the port's kind), `serial_mismatch` (the serial number as the battery prints it), `unknown_model` (the model name
is not in `core/battery_ops.hpp`), `not_idle` (a load is on it).

## Stage 2: the clock

Execute *Set the clock*. Expect `ok`, with `time` before and after in *What changed*, the after being the node's time.

The line sent is `time YY MM DD hh mm ss` (two-digit year, two-digit fields). If the battery answers `Invalid command` or the read-back differs (`clock_differs`), the form is not the one the battery
wants: **keep the transcript** and send it back; the likely alternatives are a four-digit year or fields without leading zeros, and the change is one line in `core/battery_ops.hpp`.

## Stage 3: outputs

On a battery with no load: *Switch an output* for the buzzer (on, then off), then the heater if the model has one, then the charge MOSFET off and on again, then the discharge MOSFET. Each ends with
the read of `ctrl <name>`. Expect `ok` and the *Put it back as it was* button after each. Check with a multimeter at the terminals that a MOSFET really cut the output. If `ctrl <name>` answers in a
form the read-back does not recognise, the result is `failed` with the real text: keep it.

## Stage 4: counters and logs

*Clear the statistics counters* and *Clear the log*, after saving `stat` and `log` from the panel's console. Expect `ok`; `stat` afterwards shows the counters at zero.

## Stage 5: the charge level

Read `totalcoul` first. Then *Write the total capacity* (the model's value). Expect `Test result:pass` and a read-back of `<Ah>000 mAH`. Then *Switch the MOSFET-failure test on* (a step of the maker's board
replacement procedure; read `ctrl mostest` afterwards). Only then, on a battery you do not mind resetting, *Reset the charge level*: it erases the external memory, restarts, logs in again, writes the
capacity and the serial number back; expect `ok`, `Barcode` equal to the serial number you typed and the capacity written. If it stops (`partial`), the transcript says at which line, and the battery may
be left with no serial number: write it with *Write the serial number*.

## Stage 6: limits and protections

1. From the panel's console run `config` and keep the whole answer: its form (volts or millivolts, the labels) decides how the read-back is checked and how a "put it back" could work.
2. Send the *module voltages* group with every value **equal to the one it has now** (read from that output) and see that the read-back finds them. Then change one threshold by the smallest step and
   change it back.
3. Do the same with the *temperatures* and the *cell voltages*.
4. Enable a protection that is already enabled; then *Put the protections back to factory settings*. Do **not** disable a protection on a battery that has cells, and never on one that is charging.
5. If the battery refuses the `config` lines, or the read-back never finds the values, send the transcript back: the unit (mV against V) is the first suspect.

## Stage 7: the serial number and the memory

On a battery with no serial number or one you can rewrite: *Write the serial number* (the one on its label), then read `info`. Then *Erase the external memory* with the serial number written back. The
barcode tells the chip, so check the characters 8 and 9 against the label before and after.

## Stage 8: firmware, last

* Only on a battery you can lose, only with an image from the maker for **exactly** this model and chip, as a `.bin`.
* First *keep the image*: Studio shows its board name and version. They must be the ones you expect (`US2000C`, `US2000C_NF4`, `US5000`, `E32000C_NF4`).
* Try the refusals first, on purpose, with nothing at risk: the image of the other chip must be refused with `image_mismatch` (nothing is sent); a battery whose Board and serial number disagree must give
  `chip_unknown`.
* Then the **same version** the battery already has. Watch the transcript: `Current Soft version`, `Restart the BMS system`, `BootLoader ... version`, `Program start`, the `C` characters, the packets, `Updata OK`,
  the restart, the login and `tbar`. Do not remove power until it ends. Expect `ok` and the serial number back.
* If the bootloader asks for the old checksum mode (it sends NAK instead of `C`), the node stops before sending anything (`firmware:checksum_mode_requested`) and the battery restarts into its old firmware
  after its timeout; send the transcript back.
* Only then a different version.

## What to send back

For every stage: the operation's transcript (Studio shows it; it never contains the service password), the outputs of `info`, `stat`, `time`, `totalcoul`, `config` and `prot` from the panel's console,
and anything the battery printed that the transcript does not explain. A line the battery refused is as useful as one it accepted.
