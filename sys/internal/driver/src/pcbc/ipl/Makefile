# ePSP IPL (Allegrex), the payload of os0:kd/pcbc.skprx.
#
#   make [O=<build dir>] [FW=<firmware>]
#
# Builds ipl.elf and a flat ipl.bin at 0x88600000. The payload is incomplete;
# the container remains PARTIAL until the source rebuild and packing are verified.
# Needs the PSP toolchain ($PSPDEV/bin/psp-gcc).

PSPDEV	?= $(HOME)/pspdev
CROSS	:= $(PSPDEV)/bin/psp-
CC	:= $(CROSS)gcc
OBJCOPY	:= $(CROSS)objcopy
FW	?= 4.00-DEBUGGER
O	?= build/$(FW)

FW_VERSION_4.00-DEBUGGER := 0x04000000
FW_VERSION_3.650.011-TOOL := 0x03650011
FW_VERSION_3.630.011-DEBUGGER := 0x03630011
FW_VERSION_3.65-CEX := 0x03650000

CFLAGS	:= -O2 -G0 -march=allegrex -ffreestanding -fno-builtin -fno-common \
	   -fno-pic -mno-abicalls -Wall -Wno-unused-parameter -Iinclude \
	   -I$(PSPDEV)/psp/sdk/include \
	   -DFW_ID=\"$(FW)\" -DFW_VERSION=$(FW_VERSION_$(FW))
ASFLAGS	:= -G0 -march=allegrex -Iinclude
SCRAMBLE_SEED_4.00-DEBUGGER := scramble_seed_debug.inc
SCRAMBLE_SEED_3.630.011-DEBUGGER := scramble_seed_debug.inc
SCRAMBLE_SEED_3.650.011-TOOL := scramble_seed_365.inc
SCRAMBLE_SEED_3.65-CEX := scramble_seed_365.inc
LDFLAGS	:= -nostdlib -Wl,-T,ipl.ld -Wl,--gc-sections -Wl,-Map,$(O)/ipl.map

SRCS	:= start.S halt.c data.S cache.S memlmd.c sysreg.c kl4e.c boot.c boot_services.c boot_handoff.S boot_main.c boot_config.c loadelf_boot.c libc.c
OBJS	:= $(addprefix $(O)/,$(addsuffix .o,$(basename $(SRCS))))

all: $(O)/ipl.bin

$(O)/data.o: ASFLAGS += -DIPL_SCRAMBLE_SEED_FILE=\"data/$(SCRAMBLE_SEED_$(FW))\"
$(O)/data.o: $(wildcard data/*.inc)

$(O)/%.o: %.c $(wildcard include/*.h) | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: %.S $(wildcard include/*.h) | $(O)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/ipl.elf: $(OBJS) ipl.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ -lgcc

$(O)/ipl.bin: $(O)/ipl.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data $< $@

$(O):
	mkdir -p $@

clean:
	rm -rf $(O)

.PHONY: all clean
