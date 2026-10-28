# os0:sm/update_service_sm.self (SceSblUpdateServiceSm), a secure module. See ../Makefile.
MODULE := update_service_sm
SOURCES := \
	update_service_sm.c \
	../aimgr.c \
	../sm_heap.c \
	../sm_runtime.c \
	../cf_loader.c \
	../rsa_srvk.c \
	../sm_syscall.c \
	../string.c \
	../muldiv.c
EXTRA_CFLAGS := -fno-inline -fno-ipa-cp -fno-ipa-sra -fstack-protector -fno-merge-constants -fno-zero-initialized-in-bss -mtiny=0 -mdc -fno-common -fno-toplevel-reorder

include ../sm.mk
