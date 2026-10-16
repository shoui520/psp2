# Ernie: the PS Vita's system controller (syscon), a Renesas RL78 microcontroller.
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
# Compiler: Renesas CA78K0R (cc78k0r, ra78k0r, lk78k0r): the startup is CA78K0R's
# cstart.asm. Windows programs, run like psp2snc.exe (tools/host.py). Device option and
# compiler version not identified yet. Skeleton: nothing builds yet.

HW ?= 0x00415000
FW ?= 0x01000306
PROGRAM ?= app

include hw/$(HW).mk

CFLAGS := -DERNIE_HW=$(HW) -DERNIE_FW=$(FW) -Iinclude

all:
	@echo "ernie is a skeleton: no CA78K0R toolchain yet" >&2; exit 1

.PHONY: all
