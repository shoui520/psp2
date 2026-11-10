/* The CORE class: the root of a render, its output buss and its racks (as_core.c). */
#ifndef SCE_SAS_AS_CORE_H
#define SCE_SAS_AS_CORE_H

#include "as.h"

/* the parameter blocks a CORE locks */
#define SCE_SAS_AS_CORE_BLOCK_OUTPUT	0
#define SCE_SAS_AS_CORE_BLOCK_RACKS	1

typedef struct SceSasAsCore SceSasAsCore;

/* the racks a CORE renders, in order (their handles, as the API passes them) */
typedef struct SceSasAsCoreRacks {
	SceUIntPtr *racks;
	SceUInt16 count;
	SceUInt16 capacity;
} SceSasAsCoreRacks;

extern const SceSasAsClass sceSasAsCoreClass;

int sceSasAsCoreClassSetGranularity(SceSasAsCore *core, SceUInt32 granularity);
SceUInt32 sceSasAsCoreClassGetGranularity(const SceSasAsCore *core);
int sceSasAsCoreClassRender(SceSasAsCore *core, void *output, void *jobs);
int sceSasAsCoreClassLockUpdate(SceSasAsCore *core);
int sceSasAsCoreClassUnlockUpdate(SceSasAsCore *core);

int sceSasAsCoreSetOutput(SceSasAsHandle *core, SceSasAsHandle *buss);
int sceSasAsCoreSetRacks(SceSasAsHandle *core, const SceUIntPtr *racks, int count);
int sceSasAsCoreSetGranularity(SceSasAsHandle *core, SceUInt32 granularity);
int sceSasAsCoreGetGranularity(SceSasAsHandle *core, SceUInt32 *granularity);
int sceSasAsCoreRender(SceSasAsHandle *core, void *output, void *jobs);
int sceSasAsCoreLockUpdate(SceSasAsHandle *core);
int sceSasAsCoreUnlockUpdate(SceSasAsHandle *core);

#endif
