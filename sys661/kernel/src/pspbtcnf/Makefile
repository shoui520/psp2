# ePSP 6.61 boot configuration table (pspbtcnf.bin, 2,724 bytes plaintext)
#
#   make [O=<build dir>]

PSPDEV		?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,../../../../toolchains/install/pspdev-v20261001)
PSP_PREFIX	?= $(PSPDEV)/bin/psp-
CC		:= $(PSP_PREFIX)gcc
OBJCOPY		:= $(PSP_PREFIX)objcopy
O		?= build

CFLAGS := -O2 -G0 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-mno-abicalls -mgp32 -mlong32 -I../../../ipl/include

.PHONY: all clean
all: $(O)/pspbtcnf.bin

$(O)/pspbtcnf.o: pspbtcnf.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/pspbtcnf.bin: $(O)/pspbtcnf.o
	$(OBJCOPY) -O binary -j .rodata.pspbtcnf $< $@

clean:
	rm -rf $(O)
