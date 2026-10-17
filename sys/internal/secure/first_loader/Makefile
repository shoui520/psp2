# first_loader: the PS Vita's first boot code, a 16 KiB ROM for the Cmep (little-endian
# Toshiba MeP) at its vector base 0x40000. Not a boot ROM in the strict sense (see the
# henkaku wiki); it starts second_loader.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> FW=<build date>
#   tools/mepbuild.sh sys/internal/secure/first_loader FW=20110616 O=<build directory>
#
# FW selects fw/<date>.mk: 20110616 (retail, PCH-1000), 20110117 (CEM-3000ND1).
# Sony's file names (a prototype firmware's first_loader folder: boot.c, boot.s, boot.ld,
# Makefile, beside second_loader):
#   boot.s     the vector table, reset (copies the program to 0x5c000), and the
#              copyright notice and build date in the last 0x80 bytes
#   boot.c     the program run at 0x5c000 (skeleton)
#   boot.ld    the ROM's layout
#
# Sony's Cmep compiler is not identified yet; the ROM part is assembly and builds with
# binutils 2.27 for mep-elf. Until the program is written its copy length (reset's r3,
# ROM 0xcc) is 0; everything else in 0x0-0x180 and 0x3f80-0x4000 matches the originals.

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build
FW ?= 20110616

include fw/$(FW).mk

CC := $(GCC)gcc
AS := $(BINUTILS)as
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS := -mel -O2 -std=gnu99 -Wall -ffreestanding -fno-builtin -DFL_BUILD_DATE=$(FL_BUILD_DATE)
ASFLAGS := -mel -Wa,--defsym,FL_BUILD_DATE=$(FL_BUILD_DATE)

OBJS := $(O)/$(FW)/boot.s.o $(O)/$(FW)/boot.c.o

.PHONY: image clean
image: $(O)/$(FW)/first_loader.bin

$(O)/$(FW)/%.s.o: %.s fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/$(FW)/%.c.o: %.c fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/$(FW)/first_loader.elf: $(OBJS) boot.ld
	$(LD) -T boot.ld -o $@ $(OBJS)

# the ROM image: 16 KiB from 0x40000, gaps zero
$(O)/$(FW)/first_loader.bin: $(O)/$(FW)/first_loader.elf
	$(OBJCOPY) -O binary --gap-fill 0 $< $@

clean:
	rm -rf $(O)/$(FW)
