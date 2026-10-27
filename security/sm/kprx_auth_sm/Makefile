# PlayStation Vita KPRX Authentication Secure Module (kprx_auth_sm.self)
# Toshiba MeP C5 standalone build for SLB2 / SceSblAuthMgr.
#
#   tools/mepbuild.sh security/sm/kprx_auth_sm [FW=4.00-DEBUGGER] [O=<build-dir>]

FW       ?= 4.00-DEBUGGER
GCC      ?= mep-elf-
BINUTILS ?= mep-elf-
O        ?= build

include fw/$(FW).mk

CC      := $(GCC)gcc
AS      := $(BINUTILS)as
LD      := $(BINUTILS)ld
OBJCOPY := $(BINUTILS)objcopy

CFLAGS  := -mel -O2 -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector \
           -fno-merge-constants -fno-zero-initialized-in-bss -mtiny=0 -mdc \
           -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-overlength-strings \
           -fno-strict-aliasing -ffreestanding -fno-builtin -fno-common \
           -fno-toplevel-reorder \
           -DFW_VERSION=$(FW_VERSION) \
           -I. -I../include -I../../../cmake/include -I../../../sys/sdk/target/include -I../../../sys/internal/include
ASFLAGS := -mel

BUILD_DIR := $(O)/$(FW)/kprx_auth_sm
OUT_ELF   := $(BUILD_DIR)/kprx_auth_sm.elf
OUT_BIN   := $(BUILD_DIR)/kprx_auth_sm.bin

ASM_SRCS := start.s

C_SRCS   := \
	self_loader.c \
	auth_kprx.c \
	sm_runtime.c \
	self_loader_wrapper.c \
	rsa_srvk.c \
	aimgr.c \
	string.c

OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS)) \
        $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))

.PHONY: all clean

all: $(OUT_BIN)

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.c kprx_auth_sm.h fw/$(FW).mk
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_ELF): $(OBJS) kprx_auth_sm.ld
	$(LD) -EL -T kprx_auth_sm.ld -o $@ $(OBJS)

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
