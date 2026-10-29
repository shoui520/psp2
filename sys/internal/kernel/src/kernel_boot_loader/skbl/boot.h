/*
 * What boot.c shares with the rest of SKBL and NSKBL.
 */
#pragma once

#include <scetypes.h>
#include <sysmem/cpu.h>

/*
 * The boot save area: the start of SKBL's data (boot.c), which reset.S backs up on a cold
 * boot and restores on a resume, up to its backup (skbl.ld)
 */
typedef struct BootSaveArea {
	SceUInt32 state;		/* 0: cold boot, 0x10: restored from the backup */
	SceCorelockContext corelock;	/* the CPUs' boot steps */
	SceCorelockContext kblCorelock;	/* the kernel boot loader's steps */
} BootSaveArea;

extern BootSaveArea bootSaveArea;

/* name unknown: the KBL Param's QA flag "Allow Kernel Debug" */
int IsAllowKernelDebug(void);

/* the secure world's level 1 table */
#define KBL_L1(_param)		((uint32_t *)(_param)->ttbr0.vbase)

/* archive_tzs.c */
int KblArchive(SceKernelBootParam *param);

/* memory_test.c */
void KblSimpleMemoryTest(uintptr_t start, SceSize size, uint32_t seed, SceSize step,
			 SceSize blockSize);
