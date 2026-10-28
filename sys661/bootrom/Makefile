# ePSP BootROM (Allegrex), the 4,096-byte Stage 0/Stage 2 payload embedded in
# os0:sm/compat_sm.self (security/sm/compat_sm) and loaded at 0xBFC00000.
#
#   make [O=<build dir>] [FW=<firmware>]
#
# Builds bootrom.elf, a flat 4,096-byte bootrom.bin, and bootrom.inc.

PSPDEV		?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,../../toolchains/install/pspdev-v20261001)
PSP_PREFIX	?= $(PSPDEV)/bin/psp-
CC		:= $(PSP_PREFIX)gcc
OBJCOPY		:= $(PSP_PREFIX)objcopy
FW		?= 4.00-DEBUGGER
O		?= build/$(FW)

ifeq ($(FW),4.00-DEBUGGER)
FW_DEFINES := -DFW_VERSION=0x04000000 -DFW_DEBUG=1 -DFW_CEX=0 -DFW_TOOL=0
else ifeq ($(FW),3.630.011-DEBUGGER)
FW_DEFINES := -DFW_VERSION=0x03630011 -DFW_DEBUG=1 -DFW_CEX=0 -DFW_TOOL=0
else ifeq ($(FW),3.650.011-TOOL)
FW_DEFINES := -DFW_VERSION=0x03650011 -DFW_DEBUG=0 -DFW_CEX=0 -DFW_TOOL=1
else
FW_DEFINES := -DFW_VERSION=0x03650000 -DFW_DEBUG=0 -DFW_CEX=1 -DFW_TOOL=0
endif

CFLAGS := -O2 -G0 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fno-tree-loop-distribute-patterns -fno-toplevel-reorder -fno-zero-initialized-in-bss \
	-mno-abicalls -mgp32 -mlong32 $(FW_DEFINES) -I. -I../ipl/include -I../../cmake/include

OBJS := $(O)/bootrom_start.o $(O)/bootrom_main.o $(O)/bootrom_sysreg.o

.PHONY: all clean
all: $(O)/bootrom.bin $(O)/bootrom.inc

$(O)/bootrom_start.o: start.S bootrom.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/bootrom_main.o: bootrom.c bootrom.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/bootrom_sysreg.o: ../ipl/sysreg.c ../ipl/include/ipl.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/bootrom.elf: $(OBJS) bootrom.ld
	$(CC) -nostdlib -Wl,-T,bootrom.ld -o $@ $(OBJS)

$(O)/bootrom.bin: $(O)/bootrom.elf
	$(OBJCOPY) -O binary -j .text.reset -j .text -j .data $< $@

$(O)/bootrom.inc: $(O)/bootrom.bin
	od -An -tx1 -v $< | sed 's/[0-9a-f][0-9a-f]/0x&u,/g' > $@

clean:
	rm -rf $(O)
