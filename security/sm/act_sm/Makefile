MODULE := act_sm
SOURCES := \
	act_sm.c \
	../sm_runtime.c \
	crypto.c \
	../aimgr.c \
	../sm_syscall.c \
	../string.c \
	../muldiv.c
EXTRA_CFLAGS := -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector -fno-merge-constants -fno-zero-initialized-in-bss -mtiny=0 -mdc -fno-common -fno-toplevel-reorder

include ../sm.mk
