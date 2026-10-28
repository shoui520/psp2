MODULE := compat_sm
SOURCES := \
	compat_sm.c \
	../sm_runtime.c \
	../sm_syscall.c \
	bigmac.c \
	../aimgr.c \
	../string.c \
	../muldiv.c
EXTRA_CFLAGS := -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector -fno-merge-constants -fno-zero-initialized-in-bss -mtiny=0 -mdc -fno-common -fno-toplevel-reorder

include ../sm.mk

CFLAGS += -I$(OBJ)

PSP_PREFIX ?= $(if $(filter /%,$(GCC)),$(abspath $(dir $(GCC))../..)/pspdev-v20261001/bin/psp-,psp-)
BOOTROM_DIR := $(SM)../../sys661/bootrom

$(OBJ)/bootrom.inc: $(wildcard $(BOOTROM_DIR)/* $(SM)../../sys661/ipl/sysreg.c)
	$(MAKE) -C $(BOOTROM_DIR) O=$(abspath $(OBJ)) FW=$(FW) PSP_PREFIX=$(PSP_PREFIX) $(abspath $@)

$(OBJ)/compat_sm.o: $(OBJ)/bootrom.inc
