TARGET := pops_01g
SRCS   := \
	pops_jit_asm.S \
	pops_vfpu_asm.S \
	pops_data.c \
	pops_jit.c \
	pops_dma_io.c \
	pops_sio_pad.c \
	pops_cdrom.c \
	pops_range.c \
	pops_mdec.c \
	pops_gpu.c \
	pops_main.c \
	pops_init.c \
	pops_vmc_savedata.c \
	pops_mcfs.c \
	pops_atrac.c
EXTRA_CFLAGS = -O2 -fno-strict-aliasing -mno-check-zero-division -I$(CURDIR) -I$(O)

include ../../build.mak

BIOS_O         := $(O)/bios
PSX_BIOS_BIN   := $(BIOS_O)/psx_bios.bin
PSX_SHELL_PAL  := $(BIOS_O)/psx_shell_pal.bin

$(PSX_BIOS_BIN): FORCE
	$(MAKE) -C bios O=$(abspath $(BIOS_O))

$(PSX_SHELL_PAL): $(PSX_BIOS_BIN)
	@:

$(O)/psx_bios_rom.h: $(PSX_BIOS_BIN) $(TOOLS_DIR)/bin2c.py | $(O)
	$(PYTHON) $(TOOLS_DIR)/bin2c.py $< -o $@ --symbol g_psxBiosRom --type u8

$(O)/psx_bios_patch.h: $(PSX_SHELL_PAL) $(TOOLS_DIR)/bin2c.py | $(O)
	$(PYTHON) $(TOOLS_DIR)/bin2c.py $< -o $@ --symbol g_psxBiosPatch --type u8 --const

$(O)/pops_data.o: $(O)/psx_bios_rom.h $(O)/psx_bios_patch.h

.PHONY: FORCE
FORCE:
