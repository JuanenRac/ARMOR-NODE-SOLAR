// The answers of a real Pylontech US2000 (board US2000CV30R07, firmware V1.5; its prompt is pylon>), copied unchanged from a capture of its console: the fixtures the parsers are tested with.
#pragma once

static const char* const kUs2000Help =
    R"RAW(@
Local command:
bat      Battery data show - bat [pwr][index]
cmdquit  Quit the Console Mode
data     History data load - data [event/history][item]
datalist Show recorded data - datalist [event/history][item/bat][batnun][volt/curr/temp/coul][item]
disp     Display Info at regular intervals - disp [(pwrs pwrNo)/val]/[(bats batNo)/volt/curr/temp]
getpwr   Get power Info - getpwr
help     Help [cmd]
info     Device infomation - info
log      Log information show - log
login    Login Admin mode - login [password]
logout   user mode  - logout
pwr      Power data show - pwr [index]
shut     Shut down - shut
soh      State of health - soh [addr]
stat     Statistic data show - stat
time     Time - time [year] [month] [day] [hour] [minute] [second]
trst     Test Soft Reset - trst
updata   updata system - updata
**********************************************************
Remote command:
data     History data load - data [event/history][item]
info     Device infomation - info
login    Login Admin mode - login [password]
logout   user mode  - logout
soh      State of health - soh [addr]
stat     Statistic data show - stat

pylon>
)RAW";

static const char* const kUs2000Pwr =
    R"RAW(@
Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  LC. St   Time                 B.V.St   B.T.St
1     51688  0      36000  36000  36000  3347   3495   Charge   OV       Normal   Normal   52%      OFF      2025-03-09 19:11:40  Normal   Normal
2     55084  -15    35000  35000  35000  3357   3974   Dischg   Normal   Normal   Normal   29%      OFF      2025-03-09 18:45:30  Normal   Normal
3     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
4     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
5     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
6     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
7     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
8     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
9     -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
10    -      -      -      -      -      -      -      Absent   -        -        -        -        -        -                    -        -
Command completed successfully
$$

pylon>
)RAW";

static const char* const kUs2000Bat =
    R"RAW(@
Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  Coulomb
0        3423     0        36000    Charge       Normal       Normal       Normal        60%      30056 mAH
1        3453     0        36000    Charge       Normal       Normal       Normal        60%      30385 mAH
2        3398     0        36000    Charge       Normal       Normal       Normal        54%      27275 mAH
3        3480     0        36000    Charge       Normal       Normal       Normal        60%      30482 mAH
4        3445     0        36000    Charge       Normal       Normal       Normal        60%      30366 mAH
5        3471     0        36000    Charge       Normal       Normal       Normal        60%      30463 mAH
6        3346     0        36000    Charge       Normal       Normal       Normal        52%      26039 mAH
7        3470     0        36000    Charge       Normal       Normal       Normal        60%      30444 mAH
8        3477     0        36000    Charge       Normal       Normal       Normal        60%      30482 mAH
9        3482     0        36000    Charge       Normal       Normal       Normal        61%      30502 mAH
10       3498     0        36000    Charge       Normal       Normal       Normal        61%      30541 mAH
11       3398     0        36000    Charge       Normal       Normal       Normal        54%      27275 mAH
12       3489     0        36000    Charge       Normal       Normal       Normal        61%      30541 mAH
13       3485     0        36000    Charge       Normal       Normal       Normal        61%      30521 mAH
14       3400     0        36000    Charge       Normal       Normal       Normal        56%      28384 mAH
Command completed successfully
$$

pylon>
)RAW";

static const char* const kUs2000Data =
    R"RAW(@
-----------------------------------------------
Item Index      : 754
Time            : 25-03-09 19:01:42
Voltage         : 51753       mV
Current         : 0           mA
Temperature     : 36000       mC
Percent         : 52          %
Total Coulomb   : 50000       mAH
Max Voltage     : 54000       mV
Base State      : Charge
Volt. State     : OV
Curr. State     : Normal
Tempr. State    : Normal
Coul. Status    : Normal
Power Events    : 0x0
Bat Events      : 0x4        BVNOR
Protect ENA     : BOV BHV BLV BUV POV PHV PLV PUV BOT BHT BLT BUT POT PHT PLT PUT COC COCA DOCA DOC SC
System Fault    : 0x0
-----------------------------------------------

Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  Coulomb
0        3425     0        36000    Charge       Normal       Normal       Normal       60%
1        3456     0        36000    Charge       Normal       Normal       Normal       60%
2        3398     0        36000    Charge       Normal       Normal       Normal       54%
3        3483     0        36000    Charge       Normal       Normal       Normal       60%
4        3448     0        36000    Charge       Normal       Normal       Normal       60%
5        3474     0        36000    Charge       Normal       Normal       Normal       60%
6        3346     0        36000    Charge       Normal       Normal       Normal       52%
7        3473     0        36000    Charge       Normal       Normal       Normal       60%
8        3481     0        36000    Charge       Normal       Normal       Normal       60%
9        3487     0        36000    Charge       Normal       Normal       Normal       61%
10       3500     0        36000    Charge       Normal       Normal       Normal       61%
11       3398     0        36000    Charge       Normal       Normal       Normal       54%
12       3493     0        36000    Charge       Normal       Normal       Normal       61%
13       3488     0        36000    Charge       Normal       Normal       Normal       61%
14       3403     0        36000    Charge       Normal       Normal       Normal       56%
Command completed successfully
$$

pylon>
)RAW";

static const char* const kUs2000GetPwr =
    R"RAW(@
51701#      0#  36000# 26039# Charge# OV# Normal# Normal#
3422#  36000# Normal# Normal#
3454#  36000# Normal# Normal#
3396#  36000# Normal# Normal#
3478#  36000# Normal# Normal#
3444#  36000# Normal# Normal#
3471#  36000# Normal# Normal#
3345#  36000# Normal# Normal#
3468#  36000# Normal# Normal#
3475#  36000# Normal# Normal#
3482#  36000# Normal# Normal#
3496#  36000# Normal# Normal#
3397#  36000# Normal# Normal#
3488#  36000# Normal# Normal#
3484#  36000# Normal# Normal#
3401#  36000# Normal# Normal#
0#
1#
Command completed successfully
$$

pylon>
)RAW";

static const char* const kUs2000Info =
    R"RAW(@
Device address      : 1
Manufacturer        : Pylon
Device name         : US2000
Board version       : US2000CV30R07
Main Soft version   : B51
Soft  version       : V1.5
Boot  version       : V1.2
Comm version        : V2.0
Release Date        : 15-6-02
Barcode             : PPTAB05155301250

Specification       : 48V/50AH
Cell Number         : 15
Max Dischg Curr     : -55000mA
Max Charge Curr     : 53000mA
EPONPort rate       : 1200
Console Port rate   : 19200
Command completed successfully
$$

pylon>
)RAW";

static const char* const kUs2000Soh =
    R"RAW(@
Power   1
Battery    Voltage    SOHCount   SOHStatus
0          3421       0          Normal
1          3452       0          Normal
2          3397       0          Normal
3          3477       0          Normal
4          3443       0          Normal
5          3469       0          Normal
6          3346       13         Abnormal
7          3467       0          Normal
8          3476       0          Normal
9          3481       0          Normal
10         3495       12         Normal
11         3397       0          Normal
12         3486       0          Normal
13         3482       0          Normal
14         3399       0          Normal
Command completed successfully
$$

pylon>
)RAW";
