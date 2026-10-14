# Venezia firmware of os0:kd/vnzimg.skprx: little-endian Toshiba MeP c5 with
# IVC2, built with GCC 4.5 and binutils 2.27 for mep-elf.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> [objects]
#
# The image runs at 0x800000 on all eight cores: the kernel ("VK0"), the
# drivers ("VK2") and the application ("VK1"), with the C library's pieces
# (lib) and libgcc.

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build

CC := $(GCC)gcc
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS := -mel -mivc2 -O2 -g -std=gnu99 -Wall -ffreestanding -fno-builtin -mtiny=0 \
	-Iinclude -Ikernel -Idrivers -Iapplication
ASFLAGS := -mel -mivc2 -g -Iinclude -Ikernel -Idrivers -Iapplication

kernel := $(wildcard kernel/*.c kernel/*.S)
drivers := $(wildcard drivers/*.c drivers/*.S)
application := $(wildcard application/*.c application/*.S application/*/*.c application/*/*.S)
lib := $(wildcard lib/*.c)

PARTS := kernel drivers application lib
HEADERS := $(wildcard include/*.h kernel/*.h drivers/*.h application/*.h application/*/*.h)
objs = $(patsubst %,$(O)/%.o,$(basename $($(1))))

.PHONY: objects clean
objects: $(foreach p,$(PARTS),$(call objs,$(p)))

$(O)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: %.S $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(O)
