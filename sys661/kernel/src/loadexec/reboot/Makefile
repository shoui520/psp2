# ePSP 6.61 reboot.bin (Allegrex), embedded in loadexec_01g.prx.
#
#   make [O=<build dir>]
#
# Builds reboot.elf and flat reboot.bin linked at 0x88600000.

PSPDEV  ?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,../../../../../toolchains/install/pspdev-v20261001)
CROSS   := $(PSPDEV)/bin/psp-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
IPL_DIR := ../../../../ipl
LC_DIR  := ../../loadcore
O       ?= $(if $(MEP_OUT),$(MEP_OUT)/sys661/reboot,build)

CFLAGS  := -Os -G0 -march=allegrex -ffreestanding -fno-builtin -fno-common \
           -fno-pic -mno-abicalls -Wall -Wno-unused-parameter \
           -I$(IPL_DIR)/include -I$(PSPDEV)/psp/sdk/include
ASFLAGS := -G0 -march=allegrex -I$(IPL_DIR)/include
LDFLAGS := -nostdlib -Wl,-T,reboot.ld -Wl,--gc-sections -Wl,-Map,$(O)/reboot.map

SRCS    := start.S halt.c cache.S memlmd.c sysreg.c kl4e.c boot_handoff.S \
           reboot_main.c reboot_config.c reboot_data.c loadelf_boot.c libc.c

vpath %.S $(IPL_DIR)
vpath %.c $(IPL_DIR) $(LC_DIR)

OBJS    := $(addprefix $(O)/,$(addsuffix .o,$(basename $(SRCS))))

all: $(O)/reboot.bin

$(O)/%.o: %.c $(wildcard $(IPL_DIR)/include/*.h) | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: %.S $(wildcard $(IPL_DIR)/include/*.h) | $(O)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/reboot.elf: $(OBJS) reboot.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ -lgcc

$(O)/reboot.bin: $(O)/reboot.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data $< $@

$(O):
	mkdir -p $@

clean:
	rm -rf $(O)

.PHONY: all clean
