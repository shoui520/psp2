# Ernie: the PS Vita's system controller (syscon), an NEC 78K0R (the IRT boards) or a
# Renesas RL78 microcontroller.
# Two programs share its flash:
#   app/      boot cluster 0 (vectors at 0x0000, config block "ConfZZ" at 0x800) and the
#             code 0xc000-0x353ff: the firmware version that updates replace
#   updater/  boot cluster 1 (vectors at 0x1000), code to 0xbfff and 0x35400-0x3ffff: the resident
#             updater ("Ernie Downloader"), never rewritten by an update
#
#   make HW=<hardware ID> FW=<version> [PROGRAM=app|updater]
#     e.g. make HW=0x00415000 FW=0x01000306 PROGRAM=updater   (DEM-3000L, logging build)
# targets.list lists every board/version with an update patch; hw/<id>.mk describes the
# boards. Originals: ${PSP2_FW_DIR}/ernie/<hw>/<version>/ (dump.bin, patch.bin).
#
# Compiler: Renesas CA78K0R (cc78k0r, ra78k0r) for the 78K0R boards, through
# tools/ca78k0r.py, which places every segment itself (the free edition's linker stops
# at 64 KiB); CHIP, the device option, comes from hw/<id>.mk. Windows programs, run like
# psp2snc.exe (tools/host.py). Skeleton: nothing builds yet.

HW ?= 0x00415000
FW ?= 0x01000306
PROGRAM ?= app

include hw/$(HW).mk

CFLAGS := -DERNIE_HW=$(HW) -DERNIE_FW=$(FW) -Iinclude
# cc78k0r's options beside the chip: the medium model (its default) and -qx2, its standard
# optimization. Sony's void functions mostly end in an explicit return, which keeps the last
# call a call (only a call the function falls off after becomes a branch)
CAFLAGS := -qx2

all:
	@echo "ernie is a skeleton: no CA78K0R toolchain yet" >&2; exit 1

.PHONY: all
