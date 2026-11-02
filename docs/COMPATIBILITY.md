# Compatibility: which inverters and batteries this node reads, and how far each is verified

Generated from [compatibility.json](compatibility.json) by `tools/make_compatibility.py`; edit the table, not this page.

The node reads these devices; the setting commands of the inverters are never built, and writing to a battery's console is a separate capability that is off by default. A row has one of three levels:

- **Bench**: read by this node on a bench: the model, the firmware and the commands that answered are written in the row.
- **Capture**: decoded from a capture made on a real unit of that model (the tests replay it); the node itself has not been connected to one.
- **Protocol**: written from the public description of the protocol and checked against sample replies only; no unit of it has been met.

**No inverter and no battery has been connected to this node yet**, so every row below is still pending on the bench. At the bench, for each model: connect it, read the model and the firmware it
reports, note which of the commands in the *Reads* column answered, and write the three in the row's `bench` member (`model`, `firmware`, `answers`) with `status` set to `done` and the level set to
`bench`. The test that checks this table refuses a `bench` row that has no numbers, and a pending row that claims the level.

## Inverters

| Model | Port | Reads | Level | Evidence | Bench |
| --- | --- | --- | --- | --- | --- |
| Voltronic / MPP Solar: Axpert, PIP, InfiniSolar and clones (PI30) | RS232, 2400 baud 8N1 | QPIGS, QMOD, QPIWS, QPIRI | Protocol | typical replies written by hand; the CRCs of QPIGS, QPIRI, QMOD and QPIWS | pending |
| Voltronic PI30 with a second PV input | RS232, 2400 baud 8N1 | QPIGS2 (optional, off by default) | Protocol | the sample replies of the mpp-solar project; no inverter with two inputs has been met | pending |
| Voltronic PI30 parallel system | RS232, 2400 baud 8N1 | QPGS0 to QPGS9 (optional, off by default) | Protocol | the sample replies of the mpp-solar project; no parallel system has been met | pending |
| Revo VM III and the inverters esphome-pipsolar marks as REVO | RS232, 2400 baud 8N1 | QPIGS (REVO fields), QMOD | Protocol | the frames and sample replies of the esphome-pipsolar project | pending |
| InfiniSolar V, LV5048, SunGoldPower and clones (PI18) | RS232, 2400 baud 8N1 | GS, MOD, FWS | Protocol | the sample replies of the mpp-solar and esphome-pipsolar projects | pending |

## Batteries

| Model | Port | Reads | Level | Evidence | Bench |
| --- | --- | --- | --- | --- | --- |
| Pylontech US2000C (four firmware versions) | console RS232, 115200 baud 8N1 | pwr, info, stat, soh, data event | Capture | captures of the console of real units (prompt pylon_debug>); the layouts of pwr and bat are those of the maker's console documentation, they were not in the captures | pending |
| Pylontech US2KBPL (sold as US2000B) | console RS232, 115200 baud 8N1 | pwr, info, stat, soh | Capture | a capture of a real unit | pending |
| Pylontech US3000C | console RS232, 115200 baud 8N1 | pwr, info, stat, soh | Capture | a capture of a real unit | pending |
| Pylontech 24 V unit | console RS232, 115200 baud 8N1 | pwr, info, stat | Capture | a capture of a real unit (its model name was not recorded) | pending |
| Pylontech US2000 (board US2000CV30R07, firmware V1.5) | console RS232, 19200 baud 8N1 (recent units say 115200) | help, pwr, info, stat | Capture | a capture of the console of a real unit, replayed unchanged by the tests | pending |
| Pylontech US2000B, US2000B Plus, US3000, US5000, UP2500, UP5000, Force L1, Force L2 | console RS232, 115200 baud 8N1 | pwr, info, stat | Protocol | the names these firmwares are known under; the columns of pwr are found by the names of the header line, so the layouts of the different firmwares are tolerated | pending |
| Pytes E-Box 48100R | console RS232, 115200 baud 8N1 | pwr | Protocol | its console is a relative of Pylontech's; no unit has been met | pending |
| Pylontech RS485 protocol | RS485, 9600 baud (DIP 1 on) or 115200 baud (DIP 1 off) | the frame and its checks; INFO is not decoded | Protocol | the public description of the frame; the meaning of INFO differs between protocol versions and no capture exists | pending |
| ANT-BMS 16ZMB-TB-7-16S-300A and 24AHA-TB-24S-200A (old protocol) | 3.3 V TTL UART, 19200 baud 8N1 | the status frame | Capture | frames captured on real boards | pending |
| ANT-BMS ANT-BLE16ZMUB and ANT-BLE24BHUB (new protocol) | 3.3 V TTL UART, 19200 baud 8N1 | the status frame | Capture | frames captured on real boards | pending |
| ANT-BMS 16S / 24S 100 to 200 A of 2020 (old protocol) | 3.3 V TTL UART, 19200 baud 8N1 | the status frame | Protocol | the old protocol is the same frame as the captured boards; these models were not captured | pending |
| ANT-BMS boards after the ANT-BLE24BHUB, the 32S included (new protocol) | 3.3 V TTL UART, 19200 baud 8N1 | the status frame | Protocol | the new protocol is the same frame as the captured boards; these models were not captured | pending |
