# prog_rvk: the PS Vita's Program Revocation List (slb2:prog_rvk.srvk),
# authenticated and loaded by secure_kernel into SRAM at 0x809000..0x809380
# and by kprx_auth_sm during kernel module authentication.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> FW=<firmware>
#   tools/mepbuild.sh security/prog_rvk FW=4.00-DEBUGGER
#
# Output:
#   $(O)/$(FW)/prog_rvk.elf
#   $(O)/$(FW)/prog_rvk.bin        (0x380 bytes: 0x20-byte header + 0x360-byte body)
#   $(O)/$(FW)/prog_rvk.srvk.seg01 (0x360 bytes: 27 revocation entries)

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build
FW ?= 4.00-DEBUGGER

include fw/$(FW).mk

CC := $(GCC)gcc
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS := -mel -O2 -std=gnu99 -Wall -ffreestanding -fno-builtin \
	-DSRVK_FW_VERSION=$(SRVK_FW_VERSION) \
	-I../../sys/sdk/target/include

OBJ := $(O)/$(FW)/prog_rvk/prog_rvk.c.o
ELF := $(O)/$(FW)/prog_rvk.elf
BIN := $(O)/$(FW)/prog_rvk.bin
SEG01 := $(O)/$(FW)/prog_rvk.srvk.seg01

.PHONY: all clean

all: $(BIN) $(SEG01)

$(OBJ): prog_rvk.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJ) prog_rvk.ld
	$(LD) -EL -T prog_rvk.ld -o $@ $(OBJ)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(SEG01): $(ELF)
	$(OBJCOPY) -O binary -j .srvk_body $< $@

clean:
	rm -rf $(O)/$(FW)/prog_rvk $(ELF) $(BIN) $(SEG01)
