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

ROOT := $(abspath ../../../../../..)
# The project's GCC 4.8 through tools/gcc48.sh, which runs it as this host needs
# (natively, in Lima or in WSL2); GCC=<prefix> uses another compiler directly.
# The linker is the project's binutils 2.27, the GCC 4.8 build's (toolchains/build-binutils.sh),
# run through tools/host.py like the compiler; BINUTILS=<prefix> uses another directly. Its
# Thumb-to-ROM veneers are the original's 8-byte ldr pc ones (vitasdk's ld makes 12-byte PIC ones).
ifneq ($(origin GCC),undefined)
CC := $(GCC)gcc
else
CC := $(ROOT)/tools/gcc48.sh
endif
PSP2_TOOLCHAINS ?= $(ROOT)/toolchains/install
GCC48_TOOLCHAIN ?= csl-4.8-2014.05-strong-strd
ifneq ($(origin BINUTILS),undefined)
LD := $(BINUTILS)ld
else
LD := python3 -I $(ROOT)/tools/host.py linux -- $(PSP2_TOOLCHAINS)/$(GCC48_TOOLCHAIN)/bin/arm-none-eabi-ld
endif

O ?= build

ARCH_FLAGS := -march=armv5te -mthumb-interwork -mfloat-abi=soft
# The image fills the chip's ITCM, DTCM and SQRAM as Marvell built it (the
# heap base is 0x5f800 in the original, 0x63800 here), so it is built for size.
OPT ?= -Os
CFLAGS := $(ARCH_FLAGS) -mthumb $(OPT) -g -std=gnu99 -Wall -Wextra -Wno-unused-parameter -ffreestanding -fno-builtin \
	-Iinclude -I$(ROOT)/sys/internal/include -I$(O)/radio
ASFLAGS := $(ARCH_FLAGS) -g -Iinclude

BOOT_SRCS := $(wildcard boot/*.S)
SYS_SRCS := $(wildcard sys/*.c)
OSA_SRCS := $(wildcard osa/*.c)
WLAN_SRCS := $(wildcard wlan/*.c)
BT_SRCS := $(wildcard bt/*.c)
AMP_SRCS := $(wildcard amp/*.c)
RUNTIME_SRCS := $(wildcard runtime/*.c)
SQRAM_SRCS := $(wildcard sqram/*.c)
ESA_SRCS := $(wildcard esa/*.c)

SRCS := $(BOOT_SRCS) $(SYS_SRCS) $(OSA_SRCS) $(WLAN_SRCS) $(BT_SRCS) $(AMP_SRCS) $(RUNTIME_SRCS) $(SQRAM_SRCS) $(ESA_SRCS)
HEADERS := $(wildcard include/*.h include/robin/*.h)
SCRIPTS := robin.ld boot/rom_syms.ld
OBJS := $(patsubst %,$(O)/%.o,$(basename $(SRCS)))

# GCC's Thumb-1 switch helpers (the original's switches call the ROM's __rt_switch8),
# from the pinned GCC's lib1funcs.S. Only these two: libgcc's other members would
# take the place of the ROM's run-time functions.
LIB1FUNCS := $(ROOT)/contrib/gcc-4.8-2014.05/libgcc/config/arm/lib1funcs.S
LIBGCC_OBJS := $(O)/libgcc/thumb1_case_uqi.o $(O)/libgcc/thumb1_case_uhi.o
OBJS += $(LIBGCC_OBJS)

.PHONY: image objects clean
image: $(O)/wlanbt_robin_img_ax.bin
objects: $(OBJS)

# The original unaligned-access runtime family is ARM, unlike driver C.
$(O)/runtime/unaligned.o: runtime/unaligned.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mthumb,$(CFLAGS)) -marm -c $< -o $@

# The library's memmove, strncpy, strcmp and errno code are ARM too.
$(O)/runtime/rtlibarm.o: runtime/rtlibarm.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mthumb,$(CFLAGS)) -marm -c $< -o $@

$(O)/runtime/stringarm.o: runtime/stringarm.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mthumb,$(CFLAGS)) -marm -c $< -o $@

# The concealment's DSP loops are ARM in the original, like the runtime above.
$(O)/bt/plcdsp.o: bt/plcdsp.c $(HEADERS)
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

$(O)/libgcc/thumb1_case_%.o: $(LIB1FUNCS)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -mthumb -DL_thumb1_case_$* -c $< -o $@

# The radio controllers' 8051 programs, built with SDCC (4.6.0) and embedded as the
# images the ARM side loads (radio/radioimage.py: length, code, byte sum).
SDCC ?= sdcc
SDCCFLAGS := -mmcs51 --model-small --std-c99 --iram-size 128 -Iradio/include

$(O)/radio/wlan/wlanradio.ihx: radio/wlan/wlanradio.c radio/include/radio8051.h
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) $< -o $(dir $@)

$(O)/radio/wlan_radio.inc: radio/radioimage.py $(O)/radio/wlan/wlanradio.ihx
	python3 -I radio/radioimage.py $@ $(O)/radio/wlan/wlanradio.ihx 0

$(O)/wlan/wlaninit.o: $(O)/radio/wlan_radio.inc

# The Bluetooth radio's program runs from 0x100 (its 8051 vectors are there) with
# the stack from 0x74, and fills the 8051's whole internal RAM.
$(O)/radio/bt/btradio.ihx: radio/bt/btradio.c radio/bt/btradio_hw.h
	@mkdir -p $(dir $@)
	$(SDCC) -mmcs51 --model-small --std-c99 --iram-size 256 --code-loc 0x100 -Iradio/bt $< -o $(dir $@)

$(O)/radio/bt_radio.inc: radio/radioimage.py $(O)/radio/bt/btradio.ihx
	python3 -I radio/radioimage.py $@ $(O)/radio/bt/btradio.ihx 0x100

$(O)/bt/lcpool.o: $(O)/radio/bt_radio.inc

$(O)/robin.elf: $(OBJS) $(SCRIPTS)
	$(LD) -EL -n --no-warn-mismatch --use-blx -T robin.ld -o $@ $(OBJS)

$(O)/wlanbt_robin_img_ax.bin: pack.py $(O)/robin.elf
	python3 pack.py $@ $(O)/robin.elf

clean:
	rm -rf $(O)
