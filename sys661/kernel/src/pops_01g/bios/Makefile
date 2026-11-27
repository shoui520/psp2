# PlayStation 1 System ROM (512 KB, 0xBFC00000) & PAL Shell Overlay (0x16120 B, 0x80030000)
# Compiled from clean C using psp-gcc targeting pure MIPS R3000A O32 soft-float.

PSPDEV  ?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,../../../../../toolchains/install/pspdev-v20261001)
TOOLS   := ../../../../../tools
PYTHON  ?= python3
CROSS   := $(PSPDEV)/bin/psp-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
O       ?= $(if $(MEP_OUT),$(MEP_OUT)/sys661/pops_bios,build)

R3000_CFLAGS  := -Os -G0 -march=r3000 -mabi=32 -msoft-float -ffreestanding -fno-builtin \
                 -fno-common -fno-pic -mno-abicalls -Wall -Wno-unused-parameter
R3000_ASFLAGS := -G0 -march=r3000 -mabi=32 -msoft-float -fno-pic -mno-abicalls

SHELL_SRCS := shell/shell_main.c shell/shell_gpu.c shell/shell_intr.c
SHELL_NTSC_OBJS := $(addprefix $(O)/shell_ntsc_,$(addsuffix .o,$(notdir $(basename $(SHELL_SRCS)))))
SHELL_PAL_OBJS  := $(addprefix $(O)/shell_pal_,$(addsuffix .o,$(notdir $(basename $(SHELL_SRCS)))))

BIOS_SRCS := start.S kernel_init.c exception.c events_threads.c fs_devices.c \
             cdrom_bios.c pad_card.c gpu_bios.c libc_bios.c syscall_tables.c font_bios.c
BIOS_OBJS := $(addprefix $(O)/,$(addsuffix .o,$(basename $(BIOS_SRCS)))) $(O)/shell_ntsc_embed.o

all: $(O)/psx_bios.bin $(O)/psx_shell_pal.bin

# 1. Build NTSC & PAL shell binaries (linked at 0x80030000, exact 0x16120 bytes)
$(O)/shell_ntsc_%.o: shell/%.c shell/shell_internal.h | $(O)
	$(CC) $(R3000_CFLAGS) -Ishell -c $< -o $@

$(O)/shell_pal_%.o: shell/%.c shell/shell_internal.h | $(O)
	$(CC) $(R3000_CFLAGS) -DPSX_SHELL_PAL=1 -Ishell -c $< -o $@

$(O)/psx_shell_ntsc.elf: $(SHELL_NTSC_OBJS) shell/shell.ld
	$(CC) $(R3000_CFLAGS) -nostdlib -Wl,-T,shell/shell.ld -Wl,--gc-sections $(SHELL_NTSC_OBJS) -o $@

$(O)/psx_shell_ntsc.bin: $(O)/psx_shell_ntsc.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data -j .rodata.shellend --gap-fill=0x00 $< $@

$(O)/psx_shell_pal.elf: $(SHELL_PAL_OBJS) shell/shell.ld
	$(CC) $(R3000_CFLAGS) -nostdlib -Wl,-T,shell/shell.ld -Wl,--gc-sections $(SHELL_PAL_OBJS) -o $@

$(O)/psx_shell_pal.bin: $(O)/psx_shell_pal.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data -j .rodata.shellend --gap-fill=0x00 $< $@

# 2. Embed compiled NTSC shell into .psx_shell of psx_bios.elf (at 0xBFC18000)
$(O)/shell_ntsc_embed.c: $(O)/psx_shell_ntsc.bin $(TOOLS)/bin2c.py | $(O)
	$(PYTHON) $(TOOLS)/bin2c.py $< -o $@ --symbol g_psxShellNtscImage --section .psx_shell --const

$(O)/shell_ntsc_embed.o: $(O)/shell_ntsc_embed.c | $(O)
	$(CC) $(R3000_CFLAGS) -c $< -o $@

# 3. Build 512 KB PS1 BIOS System ROM (0xBFC00000..0xBFC7FFFF)
$(O)/%.o: %.c bios_internal.h | $(O)
	$(CC) $(R3000_CFLAGS) -I. -c $< -o $@

$(O)/%.o: %.S | $(O)
	$(CC) $(R3000_ASFLAGS) -I. -c $< -o $@

$(O)/psx_bios.elf: $(BIOS_OBJS) bios_rom.ld
	$(CC) $(R3000_CFLAGS) -nostdlib -Wl,-T,bios_rom.ld -Wl,--gc-sections $(BIOS_OBJS) -o $@

$(O)/psx_bios.bin: $(O)/psx_bios.elf
	$(OBJCOPY) -O binary -j .text.boot -j .rodata.romid -j .text -j .rodata \
	    -j .psx_shell -j .psx_romfont -j .psx_romver -j .psx_romend \
	    --gap-fill=0xFF $< $@

$(O):
	mkdir -p $@

clean:
	rm -rf $(O)

.PHONY: all clean
