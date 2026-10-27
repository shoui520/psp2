# os0:sm/encdec_w_portability_sm.self (SceSblEncdecWPortabilitySm), a secure module. See ../Makefile.
MODULE := encdec_w_portability_sm
SOURCES := \
	encdec_w_portability_sm.c \
	../sm_runtime.c \
	bigmac.c \
	../sm_syscall.c \
	../muldiv.c
EXTRA_CFLAGS := -fno-inline -fno-ipa-cp -fno-ipa-sra -fno-merge-constants \
	-fno-zero-initialized-in-bss -mtiny=0 -mdc -fno-common -fno-toplevel-reorder

include ../sm.mk
