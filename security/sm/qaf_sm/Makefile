MODULE := qaf_sm
SOURCES := \
	qaf_sm.c \
	../aimgr.c \
	../sm_heap.c \
	../sm_runtime.c \
	../rsa_srvk.c \
	../sm_syscall.c \
	../string.c \
	../muldiv.c
EXTRA_CFLAGS := -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector -fno-merge-constants -fno-zero-initialized-in-bss -mtiny=0 -mdc -fno-common -fno-toplevel-reorder

include ../sm.mk
