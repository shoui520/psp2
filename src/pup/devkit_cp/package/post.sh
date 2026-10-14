#!/bin/busybox sh
rm -f /work/settings.bin
rm -f /work/settings.xml
CID=`cat /sys/class/mmc_host/mmc0/mmc0:0001/cid`
/confidential/sony/bin/nblenv get dfcpm_cp_id_3 2> /dev/null
if [ $? != 0 ]; then
    CID_0=`echo $CID | sed -e 's/\(........\).*/\1/'`
    /confidential/sony/bin/nblenv set dfcpm_cp_id_0 $CID_0

    CID_1=`echo $CID | sed -e 's/........\(........\).*/\1/'`
    /confidential/sony/bin/nblenv set dfcpm_cp_id_1 $CID_1

    CID_2=`echo $CID | sed -e 's/................\(........\).*/\1/'`
    /confidential/sony/bin/nblenv set dfcpm_cp_id_2 $CID_2

    CID_3=`echo $CID | sed -e 's/........................\(........\).*/\1/'`
    /confidential/sony/bin/nblenv set dfcpm_cp_id_3 $CID_3
fi
exit 0
