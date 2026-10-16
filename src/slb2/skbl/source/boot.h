/*
 * What boot.c shares with the rest of SKBL and NSKBL.
 */
#pragma once

#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu/corelock.h>

/* reset.S: the boot save area */
typedef struct BootSaveArea {
	SceUInt32 state;		/* 0: cold boot, 0x10: restored from the backup */
	SceCorelockContext corelock;	/* the CPUs' boot steps */
	SceCorelockContext kblCorelock;	/* the kernel boot loader's steps */
	SceUInt32 reserved[23];
} BootSaveArea;

_Static_assert(sizeof(BootSaveArea) == 0x70, "boot save area");

extern BootSaveArea bootSaveArea;

/* name unknown: the KBL Param's QA flag "Allow Kernel Debug" */
int IsAllowKernelDebug(void);
