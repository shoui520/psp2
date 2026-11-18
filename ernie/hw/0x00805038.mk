# Board 0x00805038: PCH-20XX / PTEL-20XX (USS-1001), supports FW 2.500-3.740
# Firmware versions with an update patch: 0x01030505 0x01030511
HW_ID := 0x00805038
# Renesas R5F1ZCRK, an RL78/G13 with the RL78-S3 core (DR5F1ZCRK.DVF, which
# toolchains/unpack-ca78k0r.py makes from the R5F100SK's: 384 KiB flash, 24 KiB RAM).
# Its code above the drivers is PCH-1000's (0x00406000)
BOARD := 0x00406000
CHIP := f1zcrk
