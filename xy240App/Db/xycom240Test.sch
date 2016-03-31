[schematic2]
uniq 7
[tools]
[detail]
w 27 667 100 0 n#1 hwout.hwout#51.outp 24 664 24 664 ebos.s0bo.OUT
w 27 435 100 0 n#2 hwout.hwout#59.outp 24 432 24 432 ebos.s1bo.OUT
w 596 611 100 0 n#3 hwin.hwin#63.in 512 608 680 608 embbis.s24mbbi.INP
w 523 1107 100 0 n#4 hwin.hwin#65.in 520 1104 520 1104 ebis.s24bi.INP
w 11 955 100 0 n#5 hwout.hwout#67.outp 8 952 8 952 embbos.s0mbbo.OUT
w 523 915 100 0 n#6 hwin.hwin#71.in 520 912 520 912 ebis.s25bi.INP
[cell use]
use ebos -104 606 100 0 s0bo
xform 0 -104 696
p -179 782 100 0 1 DTYP:XYCOM-240
p -132 606 100 0 -1 PV:x:
use hwout 24 648 100 0 hwout#51
xform 0 120 664
p 120 655 100 0 -1 val(outp):#C0 S0
use ebos -109 371 100 0 s1bo
xform 0 -104 464
p -179 550 100 0 1 DTYP:XYCOM-240
p -137 371 100 0 -1 PV:x:
use hwout 24 416 100 0 hwout#59
xform 0 120 432
p 120 423 100 0 -1 val(outp):#C0 S1
use embbis 792 497 100 0 s24mbbi
xform 0 808 576
p 749 554 100 0 1 SCAN:Passive
p 730 647 100 0 1 DTYP:XYCOM-240
p 1015 610 100 0 1 ZRVL:0
p 1015 577 100 0 1 ONVL:1
p 1016 542 100 0 1 TWVL:2
p 1016 513 100 0 1 THVL:3
p 1103 611 100 0 1 ZRST:FAULT(HIGH) (0,0)
p 1103 579 100 0 1 ONST:DISASSERTED (0,1)
p 1106 546 100 0 1 TWST:ASSERTED (1,0)
p 1105 515 100 0 1 THST:FAULT(LOW) (1,1)
p 764 497 100 0 -1 PV:x:
p 1093 652 100 0 1 NOBT:2
p 766 536 100 0 1 PINI:YES
use hwin 320 592 100 0 hwin#63
xform 0 416 608
p 332 601 100 0 -1 val(in):#C0 S24
use ebis 659 985 100 0 s24bi
xform 0 648 1072
p 631 985 100 0 -1 PV:x:
p 296 974 100 0 0 PINI:YES
p 555 1137 100 0 1 DTYP:XYCOM-240
p 799 1064 100 0 1 SCAN:I/O Intr
p 296 910 100 0 0 ONAM:disabled (1)
p 296 942 100 0 0 ZNAM:enabled (0)
use hwin 328 1088 100 0 hwin#65
xform 0 424 1104
p 331 1096 100 0 -1 val(in):#C0 S24
use embbos -119 863 100 0 s0mbbo
xform 0 -120 952
p -147 863 100 0 -1 PV:x:
p -498 1020 100 0 1 NOBT:2
p -462 963 100 0 1 ONST:(0,1)
p -538 965 100 0 1 ONVL:1
p -458 912 100 0 1 THST:(1,1)
p -534 912 100 0 1 THVL:3
p -459 939 100 0 1 TWST:(1,0)
p -534 939 100 0 1 TWVL:2
p -458 988 100 0 1 ZRST:(0,0)
p -539 987 100 0 1 ZRVL:0
p -206 1027 100 0 1 DTYP:XYCOM-240
use hwout 8 936 100 0 hwout#67
xform 0 104 952
p 104 943 100 0 -1 val(outp):#C0 S0
use ebis 659 793 100 0 s25bi
xform 0 648 880
p 631 793 100 0 -1 PV:x:
p 296 782 100 0 0 PINI:YES
p 555 945 100 0 1 DTYP:XYCOM-240
p 805 870 100 0 1 SCAN:I/O Intr
p 296 718 100 0 0 ONAM:disabled (1)
p 296 750 100 0 0 ZNAM:enabled (0)
use hwin 328 896 100 0 hwin#71
xform 0 424 912
p 331 904 100 0 -1 val(in):#C0 S25
[comments]
