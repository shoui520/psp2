# Shared build rules for ePSP 6.61 kernel PRX modules (sys661/kernel/src/<module>).
#
#   make [O=<build dir>]

KERNEL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
TOOLS_DIR  := $(abspath $(KERNEL_DIR)/../../tools)
PSPDEV     ?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,$(KERNEL_DIR)/../../toolchains/install/pspdev-v20261001)
PSP_GCC336 ?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/psp-gcc-3.3.6-allegrex,$(KERNEL_DIR)/../../toolchains/install/psp-gcc-3.3.6-allegrex)
PSP_TOOLCHAIN ?= $(if $(wildcard $(PSP_GCC336)/bin/psp-gcc),gcc336,pspdev)
PRXGEN     := $(PSPDEV)/bin/psp-prxgen
PYTHON     ?= python3
O          ?= $(if $(MEP_OUT),$(MEP_OUT)/sys661/$(TARGET),build)

ifeq ($(PSP_TOOLCHAIN),gcc336)
CROSS      := $(PSP_GCC336)/bin/psp-
CC         := $(CROSS)gcc
CFLAGS     := -std=gnu99 -O2 -G0 -march=allegrex -mabi=eabi -mgp32 -mfp32 -msingle-float -mbranch-likely \
              -ffreestanding -fno-builtin -fno-common -fno-pic -mno-abicalls \
              -Wall -W -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-sign-compare \
              -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include \
              -include common/gcc336_compat.h \
              $(filter-out -fno-reorder-blocks -fno-inline -fno-inline-small-functions -fno-ipa-cp -fno-optimize-sibling-calls -fno-toplevel-reorder -fno-ipa-icf -fno-ipa-ra -fno-shrink-wrap -fno-inline-functions-called-once -fno-tree-ch -fno-ipa-sra -fno-tree-dominator-opts -fno-tree-vrp -fno-code-hoisting,$(EXTRA_CFLAGS))
ASFLAGS    := -G0 -march=allegrex -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include
else
CROSS      := $(PSPDEV)/bin/psp-
CC         := $(CROSS)gcc
CFLAGS     := -std=gnu17 -O2 -G0 -march=allegrex -ffreestanding -fno-builtin -fno-common \
              -fno-pic -mno-abicalls -fno-toplevel-reorder -fno-inline-small-functions \
              -fno-ipa-icf -fno-optimize-sibling-calls -fno-ipa-ra \
              -Wall -Wextra \
              -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
              -Wno-unused-but-set-variable -Wno-sign-compare -Wno-implicit-fallthrough \
              -Wno-attribute-alias -Wno-incompatible-pointer-types -Wno-cast-function-type \
              -Wno-maybe-uninitialized -Wno-array-bounds -Wno-implicit-function-declaration \
              -Wno-int-conversion -Wno-tautological-compare -Wno-pragmas \
              -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include \
              $(EXTRA_CFLAGS)
ASFLAGS    := -G0 -march=allegrex -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include
endif
LDFLAGS    := -nostdlib -Wl,-q,-T,$(KERNEL_DIR)/prx.ld -Wl,-Map,$(O)/$(TARGET).map

OBJS       := $(addprefix $(O)/,$(addsuffix .o,$(basename $(SRCS))))
ifneq ($(wildcard module.yml),)
OBJS       += $(O)/scetables.o
endif

all: $(O)/$(TARGET).prx

$(O)/scetables.S: module.yml $(TOOLS_DIR)/mkprx_psp.py | $(O)
	$(PYTHON) $(TOOLS_DIR)/mkprx_psp.py $< -o $@

$(O)/scetables.o: $(O)/scetables.S | $(O)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/%.o: %.c $(wildcard *.h $(KERNEL_DIR)/include/*.h) | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: %.S $(wildcard *.h $(KERNEL_DIR)/include/*.h) | $(O)
	$(CC) $(ASFLAGS) -c $< -o $@

$(O)/$(TARGET).elf: $(OBJS) $(KERNEL_DIR)/prx.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ -lgcc

$(O)/$(TARGET).prx: $(O)/$(TARGET).elf
	$(PRXGEN) $< $@

$(O):
	mkdir -p $@

clean:
	rm -rf $(O)

.PHONY: all clean
