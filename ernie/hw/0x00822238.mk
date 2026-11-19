# Board 0x00822238: PCH-20XX (USS-1002), supports FW 3.500-3.740
# Firmware versions with an update patch: 0x01050031 0x01050034
HW_ID := 0x00822238
# USS-1001's chip (the same option bytes) and drivers, on the 0x0104 line (DOL-1002's)
BOARD := 0x00406000
CHIP := f1zcrk
KERNEL := 0x00411000
LINKFILE_app := app/uss1001.lk
