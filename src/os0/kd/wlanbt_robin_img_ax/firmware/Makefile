# Marvell 88W8787 ("Robin") WLAN/Bluetooth combo firmware of
# os0:kd/wlanbt_robin_img_ax.skprx: little-endian ARMv5TE (Marvell Feroceon),
# Thumb-1 with ARM exception/CP15 glue and on-chip Mask
# ROM calls (0x03f00000 .. 0x03f48000).
#
#   make [GCC=<gcc prefix>] [BINUTILS=<binutils prefix>] [O=<build dir>] [image|objects]
#
# The firmware links into $(O)/robin.elf (robin.ld + boot/rom_syms.ld), and
# pack.py compresses the four RW data segments with RVCT LZ77 and wraps the four
# contiguous download blocks into $(O)/wlanbt_robin_img_ax.bin ("image", default).

ROOT := $(abspath ../../../../..)
# The project's GCC 4.8 through src/tools/gcc48.sh, which runs it as this host needs
# (natively, in Lima or in WSL2); GCC=<prefix> uses another compiler directly.
# The linker is vitasdk's (VITASDK must be set).
ifneq ($(origin GCC),undefined)
CC := $(GCC)gcc
else
CC := $(ROOT)/src/tools/gcc48.sh
endif
BINUTILS ?= $(VITASDK)/bin/arm-vita-eabi-
LD := $(BINUTILS)ld

O ?= build

ARCH_FLAGS := -march=armv5te -mthumb-interwork -mfloat-abi=soft
CFLAGS := $(ARCH_FLAGS) -mthumb -O2 -g -std=gnu99 -Wall -Wextra -Wno-unused-parameter -ffreestanding -fno-builtin \
	-Iinclude -I$(ROOT)/src/common/include
ASFLAGS := $(ARCH_FLAGS) -g -Iinclude

BOOT_SRCS := $(wildcard boot/*.S)
SYS_SRCS := $(wildcard sys/*.c)
OSA_SRCS := $(wildcard osa/*.c)
WLAN_SRCS := $(wildcard wlan/*.c)
BT_SRCS := $(wildcard bt/*.c)
AMP_SRCS := $(wildcard amp/*.c)
RUNTIME_SRCS := $(wildcard runtime/*.c)
SQRAM_SRCS := $(wildcard sqram/*.c)

SRCS := $(BOOT_SRCS) $(SYS_SRCS) $(OSA_SRCS) $(WLAN_SRCS) $(BT_SRCS) $(AMP_SRCS) $(RUNTIME_SRCS) $(SQRAM_SRCS)
HEADERS := $(wildcard include/*.h include/robin/*.h)
SCRIPTS := robin.ld boot/rom_syms.ld
OBJS := $(patsubst %,$(O)/%.o,$(basename $(SRCS)))

.PHONY: image objects clean
image: $(O)/wlanbt_robin_img_ax.bin
objects: $(OBJS)

# The original unaligned-access runtime family is ARM, unlike driver C.
$(O)/runtime/unaligned.o: runtime/unaligned.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mthumb,$(CFLAGS)) -marm -c $< -o $@

# SQRAM code compiles into .sqram1.ro so the linker places it in 0xc0007200..0xc000faa8
$(O)/sqram/%.o: sqram/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c $< -o $@

$(O)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: %.S $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/robin.elf: $(OBJS) $(SCRIPTS)
	$(LD) -EL -n --no-warn-mismatch -T robin.ld -o $@ $(OBJS)

$(O)/wlanbt_robin_img_ax.bin: pack.py $(O)/robin.elf
	python3 pack.py $@ $(O)/robin.elf

clean:
	rm -rf $(O)
