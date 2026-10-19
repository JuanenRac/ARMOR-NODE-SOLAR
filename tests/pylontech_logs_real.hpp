// Answers of real Pylontech batteries (US2000C and US2KBPL, prompt pylon_debug>), copied from captures of their console with the serial number removed: the fixtures the parsers are tested with.
#pragma once

static const char* const kRealInfoUs2000c =
    R"RAW(@
Device address      : 1
Manufacturer        : Pylon
Device name         : US2000C
Board version       : V10R04
Main Soft version   : B68.2.0.0
Soft  version       : V2.6
Boot  version       : V2.1
Comm version        : V2.0
Release Date        : 21-09-26
Barcode             : XXXXXXXXXXXXXXXXX
Specification       : 48V/50AH
Cell Number         : 15
Max Dischg Curr     : -90000mA
Max Charge Curr     : 90000mA
EPONPort rate       : 1200
Console Port rate   : 115200
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealStatWorn =
    R"RAW(@
Device address           1
Data Items      :     1919
HisData Items   :     2045
MiscData Items  :      123
Charge Cnt.     :        0
Discharge Cnt.  :        0
Charge Times    :    43146
Status Cnt.     :     2019
Idle Times      :    42936
COC Times       :        0
DOC Times       :        0
COCA Times      :        3
DOCA Times      :        0
SC Times        :        0
Bat OV Times    :     2428
Bat HV Times    :      530
Bat LV Times    :        1
Bat UV Times    :        1
Bat SLP Times   :        0
Pwr OV Times    :        0
Pwr HV Times    :        0
Pwr LV Times    :        0
Pwr UV Times    :        0
Pwr SLP Times   :        0
COT Times       :        0
CUT Times       :        0
DOT Times       :        0
DUT Times       :        0
CHT Times       :        0
CLT Times       :        0
DHT Times       :        0
DLT Times       :        0
Shut Times      :        5
Reset Times     :       18
RV Times        :        0
Input OV Times  :        0
SOH Times       :      237
BMICERR Times   :        0
CYCLE Times     :       29
Pwr Percent     :       99
Pwr Coulomb     : 179294400
Dsg Cap         :  1459677
HT@0.5C Cnt     :        0
LT@0.5C Cnt     :        0
HT Cnt          :        0
LT Cnt          :        0
LV Cnt          :        0
LifeWarn Times  :        0
LifeAlarm Times :        0
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealDatalistEvent =
    R"RAW(@
Item  Time               Vo(mV) Cu(mA) Tempr   Tlow    Thigh   Vlowest  Vhighest Base.St   Volt.St   Curr.St   Temp.St Per% Events
0     25-10-28 16:27:27  50909  0      30000   29000   29000   3378     3495     Charge    Normal    Normal    Normal   89% BHV 
1     25-10-28 16:27:41  51549  13683  31000   29000   29000   3407     3655     Charge    Normal    Normal    Normal   89% BOV 
2     25-10-28 16:27:49  50907  0      31000   29000   29000   3376     3497     Charge    Normal    Normal    Normal   89% BHV 
3     25-10-28 16:27:59  51551  13759  31000   29000   29000   3405     3654     Charge    Normal    Normal    Normal   89% BOV 
4     25-10-28 16:28:07  50902  0      31000   29000   29000   3375     3497     Charge    Normal    Normal    Normal   89% BHV 
5     25-10-28 16:28:17  51548  13915  31000   29000   29000   3406     3653     Charge    Normal    Normal    Normal   89% BOV 
6     25-10-28 16:28:25  50914  0      31000   29000   29000   3377     3498     Charge    Normal    Normal    Normal   89% BHV 
7     25-10-28 16:28:35  51571  13848  31000   29000   29000   3406     3660     Charge    Normal    Normal    Normal   89% BOV 
8     25-10-28 16:28:45  50889  0      31000   29000   29000   3376     3492     Charge    Normal    Normal    Normal   89% BHV 
9     25-10-28 16:28:49  51569  20686  31000   29000   29000   3405     3651     Charge    Normal    Normal    Normal   89% BOV 
10    25-10-28 16:28:57  50898  0      31000   29000   30000   3374     3496     Charge    Normal    Normal    Normal   89% BHV 
11    25-10-28 16:28:59  51642  16593  31000   29000   29000   3409     3663     Charge    Normal    Normal    Normal   89% BOV 
12    25-10-28 16:29:07  50882  0      31000   29000   30000   3374     3494     Charge    Normal    Normal    Normal   89% BHV 
13    25-10-28 16:29:09  51932  19226  31000   29000   30000   3423     3703     Charge    Normal    Normal    Normal   89% BOV 
14    25-10-28 16:29:17  50891  0      31000   29000   30000   3374     3497     Charge    Normal    Normal    Normal   89% BHV 
Press [Enter] to be continued,other key to exit
15    25-10-28 16:29:19  51655  17934  31000   29000   30000   3409     3669     Charge    Normal    Normal    Normal   89% BOV 
16    25-10-28 16:29:27  50890  0      31000   29000   30000   3375     3497     Charge    Normal    Normal    Normal   89% BHV 
17    25-10-28 16:29:35  51540  14569  31000   29000   30000   3404     3652     Charge    Normal    Normal    Normal   89% BOV 
18    25-10-28 16:29:45  50880  0      31000   29000   30000   3373     3492     Charge    Normal    Normal    Normal   89% BHV 
19    25-10-28 16:29:51  51585  14745  31000   29000   30000   3406     3661     Charge    Normal    Normal    Normal   89% BOV 
Command completed successfully
$$
pylon_debug>
)RAW";

static const char* const kRealDatalistHistory =
    R"RAW(@
Save data every 1800 S
ID    Day       Time      Volt   Curr     Tempr    Tlow     Thigh    Vlowest  Vhighest Base.St   Volt.St   Curr.St   Temp.St   ErrCode      MosT.St     SOC TotalCoul  ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC ID BVolt  BTempr  BVolt.St  BTemp.St    SOC
0     25-05-28  18:18:04  50524  3956     30200    27000    27600    3362     3406     Charge    Normal    Normal    Normal    0x0          Normal      89% 50000      1  3373   27200   Normal    Normal      89% 2  3363   27200   Normal    Normal      89% 3  3363   27200   Normal    Normal      89% 4  3367   27200   Normal    Normal      89% 5  3363   27200   Normal    Normal      89% 6  3364   27600   Normal    Normal      89% 7  3362   27600   Normal    Normal      89% 8  3406   27600   Normal    Normal      96% 9  3368   27600   Normal    Normal      89% 10 3363   27600   Normal    Normal      89% 11 3364   27000   Normal    Normal      89% 12 3365   27000   Normal    Normal      89% 13 3367   27000   Normal    Normal      89% 14 3371   27000   Normal    Normal      89% 15 3365   27000   Normal    Normal      89%
1     25-05-28  18:48:08  50615  3380     30700    27200    27800    3363     3486     Charge    Normal    Normal    Normal    0x0          Normal      89% 50000      1  3373   27300   Normal    Normal      89% 2  3363   27300   Normal    Normal      89% 3  3363   27300   Normal    Normal      89% 4  3368   27300   Normal    Normal      89% 5  3363   27300   Normal    Normal      89% 6  3364   27800   Normal    Normal      89% 7  3363   27800   Normal    Normal      89% 8  3486   27800   Normal    Normal      99% 9  3369   27800   Normal    Normal      89% 10 3363   27800   Normal    Normal      89% 11 3365   27200   Normal    Normal      89% 12 3366   27200   Normal    Normal      89% 13 3369   27200   Normal    Normal      89% 14 3374   27200   Normal    Normal      89% 15 3366   27200   Normal    Normal      89%
2     25-05-28  19:18:11  50821  2834     31400    27400    28100    3366     3535     Charge    Normal    Normal    Normal    0x0          Normal      89% 50000      1  3386   27500   Normal    Normal      93% 2  3366   27500   Normal    Normal      89% 3  3366   27500   Normal    Normal      89% 4  3379   27500   Normal    Normal      89% 5  3366   27500   Normal    Normal      89% 6  3367   28100   Normal    Normal      89% 7  3367   28100   Normal    Normal      89% 8  3535   28100   Normal    Normal     100% 9  3386   28100   Normal    Normal      92% 10 3366   28100   Normal    Normal      89% 11 3368   27400   Normal    Normal      89% 12 3372   27400   Normal    Normal      89% 13 3387   27400   Normal    Normal      89% 14 3439   27400   Normal    Normal      97% 15 3371   27400   Normal    Normal      89%
3     25-05-28  19:48:15  51047  3354     34000    27500    28100    3365     3533     Charge    Normal    Normal    Normal    0x0          Normal      89% 50000      1  3533   27700   Normal    Normal      99% 2  3366   27700   Normal    Normal      89% 3  3366   27700   Normal    Normal      89% 4  3404   27700   Normal    Normal      96% 5  3367   27700   Normal    Normal      89% 6  3369   28100   Normal    Normal      89% 7  3366   28100   Normal    Normal      89% 8  3515   28100   Normal    Normal      99% 9  3404   28100   Normal    Normal      96% 10 3365   28100   Normal    Normal      89% 11 3367   27500   Normal    Normal      89% 12 3376   27500   Normal    Normal      89% 13 3413   27500   Normal    Normal      97% 14 3460   27500   Normal    Normal      98% 15 3376   27500   Normal    Normal      89%
4     25-05-28  20:18:18  51071  -1806    35200    27600    28200    3366     3524     Dischg    Normal    Normal    Normal    0x0          Normal      89% 50000      1  3524   27800   Normal    Normal     100% 2  3367   27800   Normal    Normal      89% 3  3368   27800   Normal    Normal      89% 4  3430   27800   Normal    Normal      98% 5  3368   27800   Normal    Normal      89% 6  3372   28200   Normal    Normal      89% 7  3368   28200   Normal    Normal      89% 8  3441   28200   Normal    Normal      99% 9  3413   28200   Normal    Normal      97% 10 3366   28200   Normal    Normal      89% 11 3368   27600   Normal    Normal      89% 12 3397   27600   Normal    Normal      96% 13 3433   27600   Normal    Normal      98% 14 3459   27600   Normal    Normal      99% 15 3397   27600   Normal    Normal      96%
5     25-05-28  20:48:22  51695  928      37000    27700    28300    3419     3514     Charge    Normal    Normal    Normal    0x0          Normal      97% 50000      1  3514   28000   Normal    Normal     100% 2  3419   28000   Normal    Normal      97% 3  3423   28000   Normal    Normal      97% 4  3477   28000   Normal    Normal      99% 5  3427   28000   Normal    Normal      97% 6  3432   28300   Normal    Normal      98% 7  3419   28300   Normal    Normal      97% 8  3447   28300   Normal    Normal      99% 9  3452   28300   Normal    Normal      98% 10 3424   28300   Normal    Normal      97% 11 3422   27700   Normal    Normal      97% 12 3451   27700   Normal    Normal      98% 13 3465   27700   Normal    Normal      98% 14 3479   27700   Normal    Normal      99% 15 3444   27700   Normal    Normal      98%
Command completed successfully
$$
pylon_debug>
)RAW";
