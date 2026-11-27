TARGET := pops_01g
SRCS   := \
	pops_jit_asm.S \
	pops_vfpu_asm.S \
	pops_data.c \
	pops_jit.c \
	pops_dma_io.c \
	pops_sio_pad.c \
	pops_cdrom.c \
	pops_range.c \
	pops_mdec.c \
	pops_gpu.c \
	pops_main.c \
	pops_init.c \
	pops_vmc_savedata.c \
	pops_mcfs.c \
	pops_atrac.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing -mno-check-zero-division -I$(CURDIR)

include ../../build.mak
