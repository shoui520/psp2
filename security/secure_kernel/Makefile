# secure_kernel: the PS Vita's Toshiba MeP Secure Kernel (enp.bin), loaded by
# second_loader into Cmep SRAM at 0x800000 and entered at 0x800100.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> FW=<firmware>
#   tools/mepbuild.sh security/secure_kernel FW=4.00-DEBUGGER
#
# Output:
#   $(O)/$(FW)/secure_kernel.elf
#   $(O)/$(FW)/secure_kernel.bin

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build
FW ?= 4.00-DEBUGGER

include fw/$(FW).mk

CC := $(GCC)gcc
AS := $(BINUTILS)as
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS := -mel -O2 -mno-opts -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector -fno-merge-constants \
	-fno-zero-initialized-in-bss -mtiny=0 -mdc \
	-std=gnu99 -Wall -fno-strict-aliasing -ffreestanding -fno-builtin -fno-toplevel-reorder \
	-DSK_FW_VERSION=$(SK_FW_VERSION) \
	-I../../sys/internal/include -I../../sys/internal/secure/sbl_smsched \
	-I../../sys/sdk/target/include
ASFLAGS := -mel

# Units compiled with GP-relative small data (-mtiny=4 -mdc) and .sbss/.bss
$(O)/$(FW)/secure_kernel/handler.c.o \
$(O)/$(FW)/secure_kernel/uart_debug.c.o \
$(O)/$(FW)/secure_kernel/mem_region.c.o \
$(O)/$(FW)/secure_kernel/sm_state.c.o: CFLAGS += -mtiny=4 -mdc -fzero-initialized-in-bss

# Units compiled with .bss for zero-initialized globals (-mtiny=0 -mdc)
$(O)/$(FW)/secure_kernel/suspend.c.o \
$(O)/$(FW)/secure_kernel/intr.c.o: CFLAGS += -fzero-initialized-in-bss

OBJS := \
	$(O)/$(FW)/secure_kernel/start.s.o \
	$(O)/$(FW)/secure_kernel/exception.c.o \
	$(O)/$(FW)/secure_kernel/intr_asm.s.o \
	$(O)/$(FW)/secure_kernel/main.c.o \
	$(O)/$(FW)/secure_kernel/handler.c.o \
	$(O)/$(FW)/secure_kernel/uart_debug.c.o \
	$(O)/$(FW)/secure_kernel/mem_region.c.o \
	$(O)/$(FW)/secure_kernel/suspend.c.o \
	$(O)/$(FW)/secure_kernel/context_asm.s.o \
	$(O)/$(FW)/secure_kernel/intr.c.o \
	$(O)/$(FW)/secure_kernel/self_loader.c.o \
	$(O)/$(FW)/secure_kernel/aclib.c.o \
	$(O)/$(FW)/secure_kernel/srvk.c.o \
	$(O)/$(FW)/secure_kernel/sm_state.c.o \
	$(O)/$(FW)/secure_kernel/dmac_rng.c.o \
	$(O)/$(FW)/secure_kernel/aimgr.c.o \
	$(O)/$(FW)/secure_kernel/rsa_srvk.c.o \
	$(O)/$(FW)/secure_kernel/string.c.o

.PHONY: all image clean
all: image
image: $(O)/$(FW)/secure_kernel.bin

$(O)/$(FW)/secure_kernel/%.s.o: %.s fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/$(FW)/secure_kernel/%.c.o: %.c secure_kernel.h fw/$(FW).mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

LIBGCC := $(shell $(CC) -print-file-name=el/libgcc.a)

$(O)/$(FW)/secure_kernel.elf: $(OBJS) secure_kernel.ld
	$(LD) -EL -T secure_kernel.ld -o $@ $(OBJS) $(LIBGCC)

$(O)/$(FW)/secure_kernel.bin: $(O)/$(FW)/secure_kernel.elf
	$(OBJCOPY) -O binary --gap-fill 0 --pad-to 0x807e00 $< $@

clean:
	rm -rf $(O)/$(FW)/secure_kernel $(O)/$(FW)/secure_kernel.elf $(O)/$(FW)/secure_kernel.bin
