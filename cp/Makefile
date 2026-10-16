# devkit_cp: the Communication Processor update (PUP package type 0xb, TOOL/DEBUGGER PUPs).
# The CP is the devkit's GCP board (EMMA Mobile1-S, ARM1176JZF-S) running Linux; it bridges
# the host PC (USB) and the Vita (SDIO). CP 1.3.0.1, used by 3.10-3.68.
#
# Package (CpUp: header, AES-128-CBC encrypted tar.gz, RSA key blob; package/spec):
#   nbl.nui        miniboot/ + nbl/ (NBL 0.9.7.5)
#   vmlinux.trf    Linux 2.6.23.17-alp_nl-grover (Sony CE Linux): vendored, not reversed
#   fsimage0.trf   rootfs/ (AXFS 1.9.1): BusyBox 1.4.2, Sony's toolchain runtime,
#                  OpenSSL 0.9.8, zlib 1.2.3 (vendored) and Sony's /sbin/init
#   fsimage1.trf   confidential/ (AXFS 1.9.1, encrypted), mounted at /confidential:
#                  Sony's programs and kernel modules
#   normal0.txt normal1.txt pre.sh post.sh spec   package/, verbatim
#
# Skeleton: stub sources only, nothing builds yet. Toolchain to come: FSF GCC 4.1.2 for
# arm-linux (ARM1176, the compiler Sony CE Linux 5.0.2.8 is based on), Linux 2.6.23.17
# headers for the modules, mkfs.axfs for AXFS 1.9.1. The .trf and CpUp signatures and
# encryption are deferred.

PROGRAMS := confidential/scei/bin/dfcpm confidential/scei/bin/i2c \
	confidential/scei/bin/battery_command confidential/sony/bin/bootprof \
	confidential/sony/bin/cuiutil confidential/sony/bin/nblconfig \
	confidential/sony/bin/nblenv confidential/sony/bin/nblupdate
MODULES := confidential/scei/lib/sce_gpio confidential/scei/lib/sce_sdio \
	confidential/scei/lib/sce_usb confidential/sony/lib/sdmdrv

all:
	@echo "devkit_cp is a skeleton: no toolchain yet" >&2; exit 1

.PHONY: all
