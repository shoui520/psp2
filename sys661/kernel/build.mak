# Shared build rules for ePSP 6.61 kernel PRX modules (sys661/kernel/src/<module>).
#
#   make [O=<build dir>]

KERNEL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PSPDEV     ?= $(if $(PSP2_TOOLCHAINS),$(PSP2_TOOLCHAINS)/pspdev-v20261001,$(KERNEL_DIR)/../../toolchains/install/pspdev-v20261001)
CROSS      := $(PSPDEV)/bin/psp-
CC         := $(CROSS)gcc
PRXGEN     := $(CROSS)prxgen
O          ?= $(if $(MEP_OUT),$(MEP_OUT)/sys661/$(TARGET),build)

CFLAGS     := -O2 -G0 -march=allegrex -ffreestanding -fno-builtin -fno-common \
              -fno-pic -mno-abicalls -fno-toplevel-reorder -fno-inline-small-functions -fno-ipa-icf \
              -Wall -Wextra \
              -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
              -Wno-unused-but-set-variable -Wno-sign-compare -Wno-implicit-fallthrough \
              -Wno-attribute-alias -Wno-incompatible-pointer-types \
              -Wno-cast-function-type -Wno-maybe-uninitialized -Wno-array-bounds \
              -Wno-implicit-function-declaration -Wno-int-conversion \
              -Wno-tautological-compare -Wno-pragmas \
              -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include \
              $(EXTRA_CFLAGS)
ASFLAGS    := -G0 -march=allegrex -I. -I$(KERNEL_DIR)/include -I$(PSPDEV)/psp/sdk/include
LDFLAGS    := -nostdlib -Wl,-q,-T,$(KERNEL_DIR)/prx.ld -Wl,-Map,$(O)/$(TARGET).map

OBJS       := $(addprefix $(O)/,$(addsuffix .o,$(basename $(SRCS))))

all: $(O)/$(TARGET).prx

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
