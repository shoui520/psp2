# VIP firmware of os0:kd/vipimg.skprx: big-endian Toshiba MeP, built with GCC 4.5
# and binutils 2.27 for mep-elf.
#
#   make GCC=<gcc prefix> BINUTILS=<binutils prefix> O=<build directory> [objects]
#
# The image holds two firmwares, fw1 (boot block, PU0 and PU1 programs) and fw2
# (boot block and runtime, two stream parsers, the codec, the program records),
# each linked at 0x800000. Each program is linked on its own (its globals and
# local RAM are its own); a link that needs another's addresses takes them from
# it (-R): fw1's boot block from the two programs, FW2's programs from the
# runtime, FW2's records from the programs. pack.py puts the links together into
# $(O)/vipimg.bin ("image", the default).

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build

CC := $(GCC)gcc
LD := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

# The cores have the core instruction set and the repeat loops, nothing else of
# MeP's options: multiplication and division are library calls (fw1/common).
CORE := -mno-mult -mno-div -mno-bitops -mno-leadz -mno-minmax -mno-absdiff \
	-mno-average -mno-clip -mno-satur -mrepeat
CFLAGS := -meb -O2 -g -std=gnu99 -Wall -ffreestanding -fno-builtin -mtiny=0 $(CORE) \
	-Iinclude -I../../../../../../sys/internal/include
ASFLAGS := -meb -g -Iinclude

# programs: their sources, and the shared code each one builds in
fw1-boot := $(wildcard fw1/boot/*.c fw1/boot/*.S)
fw1-common := $(wildcard fw1/common/*.c fw1/common/*.S)
fw1-pu0 := $(wildcard fw1/pu0/*.c fw1/pu0/*.S) $(fw1-common)
fw1-pu1 := $(wildcard fw1/pu1/*.c fw1/pu1/*.S) $(fw1-common)

# FW2: its boot block, and the runtime both cores run, which builds in fw1's
# DMA code
fw2-boot := fw2/boot/boot.S
fw2-runtime := $(wildcard fw2/common/*.c fw2/common/*.S) fw1/common/dma.c \
	fw1/common/lram.c fw1/common/divide.c
fw2-records := fw2/boot/programs.S

# FW2's stream parsers: the library both build in, and each one's own code
fw2-parser := $(wildcard fw2/parser/*.c fw2/parser/*.S)
fw2-parser0 := $(wildcard fw2/parser0/*.c) $(fw2-parser)
fw2-parser1 := $(wildcard fw2/parser1/*.c) $(fw2-parser)

# FW2's codec, fw1's PU1 decoder made to serve contexts, and the PU1 code they share
fw2-codec := $(wildcard fw2/codec/*.c fw2/codec/*.S) \
	fw1/pu1/memory.c fw1/pu1/parameters.c fw1/pu1/scaling.c fw1/pu1/order.c \
	fw1/pu1/lists.c fw1/pu1/dpb.c fw1/pu1/direct.c fw1/pu1/loop.c fw1/pu1/sei.c fw1/pu1/timing.c fw1/pu1/stream.c

PROGRAMS := fw1-boot fw1-pu0 fw1-pu1 fw2-boot fw2-runtime fw2-parser0 \
	fw2-parser1 fw2-codec fw2-records
HEADERS := $(wildcard include/*.h fw1/*/*.h fw2/*/*.h)
SCRIPTS := $(wildcard fw1/*/*.ld fw2/*/*.ld)
objs = $(patsubst %,$(O)/$(1)/%.o,$(basename $($(1))))

.PHONY: image objects clean
image: $(O)/vipimg.bin
objects: $(foreach p,$(PROGRAMS),$(call objs,$(p)))

# what a program's firmware adds to the flags
flags = $(if $(filter fw2-%,$(1)),-DVIP_FW2)

define program
$(O)/$(1)/%.o: %.c $(HEADERS)
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $(call flags,$(1)) -DVIP_PROGRAM_$(subst -,_,$(1)) -c $$< -o $$@

$(O)/$(1)/%.o: %.S $(HEADERS)
	@mkdir -p $$(dir $$@)
	$$(CC) $$(ASFLAGS) $(call flags,$(1)) -DVIP_PROGRAM_$(subst -,_,$(1)) -c $$< -o $$@
endef
$(foreach p,$(PROGRAMS),$(eval $(call program,$(p))))

# the links: each one's objects, script, and the links it takes addresses from
fw1-pu0-link := $(call objs,fw1-pu0)
fw1-pu0-script := fw1/pu0/pu0.ld
fw1-pu1-link := $(call objs,fw1-pu1)
fw1-pu1-script := fw1/pu1/pu1.ld
fw1-boot-link := $(call objs,fw1-boot)
fw1-boot-script := fw1/boot/boot.ld
fw1-boot-uses := $(O)/fw1-pu0.sym $(O)/fw1-pu1.sym
fw2-base-link := $(call objs,fw2-boot) $(call objs,fw2-runtime)
fw2-base-script := fw2/boot/boot.ld
fw2-parser0-link := $(call objs,fw2-parser0)
fw2-parser0-script := fw2/parser0/parser0.ld
fw2-parser0-uses := $(O)/fw2-base.elf
fw2-parser1-link := $(call objs,fw2-parser1)
fw2-parser1-script := fw2/parser1/parser1.ld
fw2-parser1-uses := $(O)/fw2-base.elf
fw2-codec-link := $(call objs,fw2-codec)
fw2-codec-script := fw2/codec/codec.ld
fw2-codec-uses := $(O)/fw2-base.elf
fw2-records-link := $(call objs,fw2-records)
fw2-records-script := fw2/boot/programs.ld
fw2-records-uses := $(O)/fw2-parser0.sym $(O)/fw2-parser1.sym $(O)/fw2-codec.sym
LINKS := fw1-pu0 fw1-pu1 fw1-boot fw2-base fw2-parser0 fw2-parser1 fw2-codec fw2-records

# what a link gives the links that take addresses from it
record = __$(1)_code_load __$(1)_code_size __$(1)_local_load __$(1)_local_size \
	__$(1)_globals_load __$(1)_globals_size __$(1)_zero_size
regions = __$(1)_lram_image_end __$(1)_lram_image_gap __$(1)_bss_start __$(1)_bss_size
fw1-pu0-gives := pu0Start $(call regions,pu0)
fw1-pu1-gives := pu1Start $(call regions,pu1)
fw2-parser0-gives := parser0Entry $(call record,parser0)
fw2-parser1-gives := parser1Entry parser1ResetHardware $(call record,parser1)
fw2-codec-gives := codecEntry codecResetHardware $(call record,codec)

# fw1's loop overlays at -Os, so that PU1's code overlays fit its 4 KB of code RAM
$(O)/fw1-pu1/fw1/pu1/loop.o: CFLAGS += -Os

define link
$(O)/$(1).elf: $($(1)-link) $($(1)-uses) $(SCRIPTS)
	$$(LD) -EB -n -T $($(1)-script) -o $$@ $(addprefix -R ,$($(1)-uses)) $($(1)-link)

$(O)/$(1).sym: $(O)/$(1).elf
	$$(OBJCOPY) -S $(addprefix -K ,$($(1)-gives)) $$< $$@
endef
$(foreach l,$(LINKS),$(eval $(call link,$(l))))

$(O)/vipimg.bin: pack.py $(foreach l,$(LINKS),$(O)/$(l).elf)
	python3 pack.py $@ --fw1 $(O)/fw1-boot.elf $(O)/fw1-pu0.elf $(O)/fw1-pu1.elf \
		--fw2 $(O)/fw2-base.elf $(O)/fw2-parser0.elf $(O)/fw2-parser1.elf \
		$(O)/fw2-codec.elf $(O)/fw2-records.elf

clean:
	rm -rf $(O)
