// The answers of a real Pylontech battery's console (module 1), copied unchanged from the maker's console documentation: the fixtures the parsers are tested with.
#pragma once

static const char* const kRealBat =
    R"RAW(@
Battery - Volt   - Curr - Tempr   - Base State - Volt. State - Curr. State - Temp.     - State - Coulomb
0           - 3303 - 236  - 18000 - Dischg         - Normal       - Normal       - Normal   - 68%  - 33424 mAH
1 3303 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
2 3303 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
3 3303 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
4 3302 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
5 3303 -236 19000 Dischg Normal Normal Normal 68% 33424 mAH
6 3303 -236 19000 Dischg Normal Normal Normal 68% 33424 mAH
7 3303 -236 19000 Dischg Normal Normal Normal 68% 33424 mAH
8 3303 -236 19000 Dischg Normal Normal Normal 68% 33424 mAH
9 3303 -236 19000 Dischg Normal Normal Normal 68% 33424 mAH
10 3304 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
11 3304 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
12 3305 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
13 3304 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
14 3304 -236 18000 Dischg Normal Normal Normal 68% 33424 mAH
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealSoh =
    R"RAW(@
Power 1
Battery - Voltage - SOHCount - SOHStatus
0           - 3302     - 0                  - Normal
1 3303 0 Normal
2 3303 0 Normal
3 3302 0 Normal
4 3303 0 Normal
5 3302 0 Normal
6 3303 0 Normal
7 3302 0 Normal
8 3302 0 Normal
9 3302 0 Normal
10 3304 0 Normal
11 3304 0 Normal
12 3306 0 Normal
13 3304 0 Normal
14 3304 0 Normal
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealStat =
    R"RAW(@
Device address 1
Data Items : 628
HisData Items : 2044
Charge Cnt. : 0
Discharge Cnt. : 761
Charge Times : 35859
Status Cnt. : 760
Idle Times : 38704
COC Times : 0
DOC Times : 0
COCA Times : 0
DOCA Times : 0
SC Times : 0
Bat OV Times : 44
Bat HV Times : 83
Bat LV Times : 8
Bat UV Times : 0
Bat SLP Times : 0
Pwr OV Times : 220
Pwr HV Times : 278
Pwr LV Times : 8
Pwr UV Times : 1
Pwr SLP Times : 0
COT Times : 0
CUT Times : 0
DOT Times : 0
DUT Times : 0
CHT Times : 0
CLT Times : 0
DHT Times : 0
DLT Times : 0
Shut Times : 7
Reset Times : 22
RV Times : 0
Input OV Times : 0
SOH Times : 31
BMICERR Times : 0
CYCLE Times : 11
Pwr Percent : 67
Pwr Coulomb : 120297600
Dsg Cap : 4542819
HT@0.5C Cnt : 0
LT@0.5C Cnt : 0
HT Cnt : 0
LT Cnt : 0
LV Cnt : 17416
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealDataEvent =
    R"RAW(@
-----------------------------------------------
Item Index : 1
Time : 17-05-13 20:52:58
Voltage : 54015 mV
Current : 10648 mA
Temperature : 29000 mC
Percent : 100 %
Total Coulomb : 50000 mAH
Max Voltage : 54000 mV
Base State : Charge
Volt. State : High
Curr. State : Normal
Tempr. State : Normal
Coul. Status : Full
Power Events : 0x2 PHV
Bat Events : 0x0
Bat Protect ENA : BOV BHV BLV BUV BSLP CBOT CBHT CBLT CBUT DBOT DBHT DBLT DBUT
Pwr Protect ENA : POV PHV PLV PUV PSLP POT PHT PLT PUT COC2 COC COCA DOCA DOC DOC2 SC LCOUL
System Fault : 0x0
-----------------------------------------------
Battery - Volt  - Curr  -   Tempr Base   - State      - Volt. State - Curr. State - Temp. State  - Coulomb
0        -   3559 - 10648 - 28000            -  Charge  - Normal     - Normal       - Normal           - 100%
1 3588 10648 28000 Charge Normal Normal Normal 100%
2 3588 10648 28000 Charge Normal Normal Normal 100%
3 3610 10648 28000 Charge Normal Normal Normal 100%
4 3589 10648 28000 Charge Normal Normal Normal 100%
5 3688 10648 29000 Charge Normal Normal Normal 100%
6 3615 10648 29000 Charge Normal Normal Normal 100%
7 3590 10648 29000 Charge Normal Normal Normal 100%
8 3716 10648 29000 Charge High Normal Normal 100%
9 3666 10648 29000 Charge Normal Normal Normal 100%
10 3535 10648 28000 Charge Normal Normal Normal 100%
11 3625 10648 28000 Charge Normal Normal Normal 100%
12 3449 10648 28000 Charge Normal Normal Normal 100%
13 3610 10648 28000 Charge Normal Normal Normal 100%
14 3587 10648 28000 Charge Normal Normal Normal 100%
Command completed successfully
$$
pylon_debug>
)RAW";
