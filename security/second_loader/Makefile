# second_loader: the PS Vita's second boot loader (SLSK), loaded by first_loader at
# 0x40000 (aliased to Cmep SRAM 0x800000) and entered at 0x40180.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> FW=<firmware>
#   tools/mepbuild.sh security/second_loader FW=4.00-DEBUGGER
#
# Output:
#   $(O)/$(FW)/second_loader.elf
#   $(O)/$(FW)/second_loader.bin

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build
FW ?= 4.00-DEBUGGER

include fw/$(FW).mk

CC := $(GCC)gcc
AS := $(BINUTILS)as
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS := -mel -O2 -mno-opts -fstack-protector -fno-zero-initialized-in-bss -mtiny=0 -mdc \
	-std=gnu99 -Wall -fno-strict-aliasing -ffreestanding -fno-builtin -fno-toplevel-reorder \
	-DSL_FW_VERSION=$(SL_FW_VERSION) \
	-I../../sys/internal/include -I../../sys/internal/secure/sbl_smsched \
	-I../../sys/internal/driver/src/sdif -I../../sys/sdk/target/include \
	-I../../sys/internal/kernel/src/update_mgr
ASFLAGS := -mel

# Platform hardware driver units compiled with GP-relative small data (-mtiny=4 -mdc)
$(O)/$(FW)/second_loader/syscon.c.o \
$(O)/$(FW)/second_loader/blockdev.c.o \
$(O)/$(FW)/second_loader/sdif_emmc.c.o \
$(O)/$(FW)/second_loader/ddrc.c.o \
$(O)/$(FW)/second_loader/pervasive_clk.c.o: CFLAGS += -mtiny=4 -mdc

CSRCS := \
	boot.c \
	loader.c \
	kbl_param.c \
	qaf.c \
	keyring.c \
	resume.c \
	snvs.c \
	syscon.c \
	bigmac.c \
	timer.c \
	rsa_srvk.c \
	partition.c \
	slb2.c \
	blockdev.c \
	sdif_emmc.c \
	ddrc.c \
	pervasive_clk.c \
	uart_debug.c \
	handshake.c \
	gpio.c \
	self_auth.c \
	string.c

OBJS := $(O)/$(FW)/second_loader/start.s.o $(patsubst %.c,$(O)/$(FW)/second_loader/%.c.o,$(CSRCS))

.PHONY: all image clean
all: image
image: $(O)/$(FW)/second_loader.bin

$(O)/$(FW)/second_loader/%.s.o: %.s fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/$(FW)/second_loader/%.c.o: %.c second_loader.h fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

LIBGCC := $(shell $(CC) -print-file-name=el/libgcc.a)

$(O)/$(FW)/second_loader.elf: $(OBJS) second_loader.ld
	$(LD) -EL -T second_loader.ld -o $@ $(OBJS) $(LIBGCC)

$(O)/$(FW)/second_loader.bin: $(O)/$(FW)/second_loader.elf
	$(OBJCOPY) -O binary --gap-fill 0 $< $@

clean:
	rm -rf $(O)/$(FW)/second_loader $(O)/$(FW)/second_loader.elf $(O)/$(FW)/second_loader.bin
