# Venezia firmware of os0:kd/vnzimg.skprx: little-endian Toshiba MeP c5 with
# IVC2, built with GCC 4.5 and binutils 2.27 for mep-elf.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> [objects]
#
# The image runs at 0x800000 on all eight cores: the kernel ("VK0"), the
# drivers ("VK2") and the application ("VK1"), with the C library's pieces
# (lib) and libgcc. The SILK codec is Skype's SILK SDK 1.0.8 from
# contrib/silk-1.0.8; the files Sony changed (sources and headers) are in
# application/silk. Both are copied into one tree in the build directory, Sony's
# over upstream's, so that every SILK file includes Sony's headers.

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build

CC := $(GCC)gcc
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

SILK := ../../../../../../contrib/silk-1.0.8

CFLAGS := -mel -mivc2 -O2 -g -std=gnu99 -Wall -ffreestanding -fno-builtin -mtiny=0 \
	-Iinclude -Ikernel -Idrivers -Iapplication -I$(O)/silk/src
ASFLAGS := -mel -mivc2 -g -Iinclude -Ikernel -Idrivers -Iapplication

kernel := $(wildcard kernel/*.c kernel/*.S)
drivers := $(wildcard drivers/*.c drivers/*.S)
application := $(filter-out application/silk/%,$(wildcard application/*.c application/*.S \
	application/*/*.c application/*/*.S))
lib := $(wildcard lib/*.c)
silk := $(sort $(notdir $(wildcard $(SILK)/src/*.c application/silk/*.c)))
silktree := $(O)/silk/src/.tree

PARTS := kernel drivers application lib silk
HEADERS := $(wildcard include/*.h kernel/*.h drivers/*.h application/*.h application/*/*.h) \
	$(silktree)
objs = $(patsubst %,$(O)/%.o,$(basename $(if $(filter silk,$(1)),$(addprefix silk/,$(silk)),$($(1)))))

.PHONY: objects clean
objects: $(foreach p,$(PARTS),$(call objs,$(p)))

$(O)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(silktree): $(wildcard $(SILK)/src/* $(SILK)/interface/* application/silk/*)
	@rm -rf $(dir $@) && mkdir -p $(dir $@)
	cp $(SILK)/src/*.[ch] $(SILK)/interface/*.h $(dir $@)
	cp application/silk/*.[ch] $(dir $@)
	@touch $@

$(O)/silk/%.o: $(silktree) $(HEADERS)
	$(CC) $(CFLAGS) -c $(O)/silk/src/$*.c -o $@

$(O)/%.o: %.S $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(O)
