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
PSP_CC := $(PSP_PREFIX)gcc
PSP_OBJCOPY := $(PSP_PREFIX)objcopy
PSP_CFLAGS := -O2 -G0 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fno-tree-loop-distribute-patterns -fno-toplevel-reorder -fno-zero-initialized-in-bss \
	-mno-abicalls -mgp32 -mlong32 $(FW_DEFINES) -I$(SM)../../cmake/include

$(OBJ)/bootrom_start.o: bootrom/start.S bootrom/bootrom.h
	@mkdir -p $(dir $@)
	$(PSP_CC) $(PSP_CFLAGS) -c $< -o $@

$(OBJ)/bootrom_main.o: bootrom/bootrom.c bootrom/bootrom.h
	@mkdir -p $(dir $@)
	$(PSP_CC) $(PSP_CFLAGS) -c $< -o $@

$(OBJ)/bootrom.elf: $(OBJ)/bootrom_start.o $(OBJ)/bootrom_main.o bootrom/bootrom.ld
	$(PSP_CC) -nostdlib -Wl,-T,bootrom/bootrom.ld -o $@ $(OBJ)/bootrom_start.o $(OBJ)/bootrom_main.o

$(OBJ)/bootrom.bin: $(OBJ)/bootrom.elf
	$(PSP_OBJCOPY) -O binary -j .text.reset -j .text -j .data $< $@

$(OBJ)/bootrom.inc: $(OBJ)/bootrom.bin
	od -An -tx1 -v $< | sed 's/[0-9a-f][0-9a-f]/0x&u,/g' > $@

$(OBJ)/compat_sm.o: $(OBJ)/bootrom.inc

