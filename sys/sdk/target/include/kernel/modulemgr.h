/*
 * kernel/modulemgr.h: the module manager's constants and option types, under Sony's names and with
 * Sony's definitions (SDK Release 03.570.011, target/include/kernel/modulemgr.h).
 */
#ifndef _SCE_KERNEL_MODULEMGR_H
#define _SCE_KERNEL_MODULEMGR_H

#include <scetypes.h>

/* what module_start and module_stop return */
#define SCE_KERNEL_START_SUCCESS	(0)	/* started, stays resident */
#define SCE_KERNEL_START_RESIDENT	SCE_KERNEL_START_SUCCESS
#define SCE_KERNEL_START_NO_RESIDENT	(1)	/* started, unloaded again */
#define SCE_KERNEL_START_FAILED		(2)

#define SCE_KERNEL_STOP_SUCCESS		(0)
#define SCE_KERNEL_STOP_FAIL		(1)	/* stays loaded */
#define SCE_KERNEL_STOP_CANCEL		SCE_KERNEL_STOP_FAIL

#if !defined(_LANGUAGE_ASSEMBLY)

typedef struct SceKernelLoadModuleOpt {
	SceSize		size;
} SceKernelLoadModuleOpt;

typedef struct SceKernelStartModuleOpt {
	SceSize		size;
	SceUInt32	flags;		/* 0 */
	SceUInt32	prologue;	/* 0 */
	SceUInt32	start;		/* 0 */
} SceKernelStartModuleOpt;

typedef struct SceKernelStopModuleOpt {
	SceSize		size;
	SceUInt32	flags;		/* 0 */
	SceUInt32	epilogue;	/* 0 */
	SceUInt32	stop;		/* 0 */
} SceKernelStopModuleOpt;

typedef struct SceKernelUnloadModuleOpt {
	SceSize		size;
} SceKernelUnloadModuleOpt;

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_KERNEL_MODULEMGR_H */
