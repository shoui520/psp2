/* The CORE class: the root of a render, its output buss and its racks (as_core.c). */
#ifndef SCE_SAS_AS_CORE_H
#define SCE_SAS_AS_CORE_H

#include "as.h"
#include "as_task.h"

/* the parameter blocks a CORE locks */
#define SCE_SAS_AS_CORE_BLOCK_OUTPUT	0
#define SCE_SAS_AS_CORE_BLOCK_RACKS	1

/* the racks a CORE renders, in order (their handles, as the API passes them) */
typedef struct SceSasAsCoreRacks {
	SceUIntPtr *racks;
	SceUInt16 count;
	SceUInt16 capacity;
} SceSasAsCoreRacks;

typedef struct SceSasAsCore {
	SceSasAsHandle handle;
	SceUInt32 bussSampleSize;		/* 4: the busses hold 32-bit samples */
	SceUInt32 granularity;
	SceUInt32 maxGranularity;
	SceUInt32 minGranularity;
	SceUInt32 numChannels;
	SceUInt32 numProcessors;
	SceUInt32 outputSampleSize;
	SceSasAsTaskList *tasks;
	SceSasAsLockedBlock outputLocked;
	struct SceSasAsBuss *output;
	SceSasAsLockedBlock racksLocked;
	SceSasAsCoreRacks racks;
	SceSasAsMutex renderMutex;
	SceSasAsMutex updateMutex;
	SceUInt32 reserved0;			/* nothing reads or writes it */
	SceInt32 updateLockCount;
	SceUInt8 reserved[0x400];
} SceSasAsCore;

extern const SceSasAsClass sceSasAsCoreClass;

int sceSasAsCoreClassSetGranularity(SceSasAsCore *core, SceUInt32 granularity);
SceUInt32 sceSasAsCoreClassGetMinGranularity(const SceSasAsCore *core);
SceUInt32 sceSasAsCoreClassGetMaxGranularity(const SceSasAsCore *core);
SceUInt32 sceSasAsCoreClassGetGranularity(const SceSasAsCore *core);
SceUInt32 sceSasAsCoreClassGetNumChannels(const SceSasAsCore *core);
SceUInt32 sceSasAsCoreClassGetNumProcessors(const SceSasAsCore *core);
int sceSasAsCoreClassRender(SceSasAsCore *core, void *output, SceUInt32 numSamples);
int sceSasAsCoreClassLockUpdate(SceSasAsCore *core);
int sceSasAsCoreClassUnlockUpdate(SceSasAsCore *core);

int sceSasAsCoreSetOutput(SceSasAsHandle *core, SceSasAsHandle *buss);
int sceSasAsCoreSetRacks(SceSasAsHandle *core, const SceUIntPtr *racks, int count);
int sceSasAsCoreSetGranularity(SceSasAsHandle *core, SceUInt32 granularity);
int sceSasAsCoreGetGranularity(SceSasAsHandle *core, SceUInt32 *granularity);
int sceSasAsCoreRender(SceSasAsHandle *core, void *output, SceUInt32 numSamples);
int sceSasAsCoreLockUpdate(SceSasAsHandle *core);
int sceSasAsCoreUnlockUpdate(SceSasAsHandle *core);

#endif
