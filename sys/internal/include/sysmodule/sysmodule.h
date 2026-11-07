/*
 * SceSysmodule's exports: the syscalls a process loads its system libraries
 * with, by the module IDs of sysmodule_ids.h.
 */
#pragma once

#include <scetypes.h>
#include <sysmodule/sysmodule_ids.h>

/* SceSysmodule's errors (Sony's libsysmodule.h) */
#define SCE_SYSMODULE_ERROR_INVALID_VALUE	-2141581312	/* 0x805A1000 */
#define SCE_SYSMODULE_ERROR_UNLOADED		-2141581311	/* 0x805A1001 */
#define SCE_SYSMODULE_ERROR_FATAL		-2141581057	/* 0x805A10FF */

#ifdef __cplusplus
extern "C" {
#endif

/* sceSysmodule*InternalWithArg option (user) */
typedef struct SysmoduleUserOpt {
	SceUInt32 flags;
	int *status;				/* user: receives the module's status */
	SceUInt32 reserved[2];
} SysmoduleUserOpt;

int sceSysmoduleLoadModule(SceUInt32 id);
int sceSysmoduleUnloadModule(SceUInt32 id);
int sceSysmoduleIsLoaded(SceUInt32 id);
int sceSysmoduleLoadModuleInternal(SceUInt32 id);
int sceSysmoduleUnloadModuleInternal(SceUInt32 id);
int sceSysmoduleIsLoadedInternal(SceUInt32 id);
int sceSysmoduleLoadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp,
					  const SysmoduleUserOpt *uopt);
int sceSysmoduleUnloadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp,
					    const SysmoduleUserOpt *uopt);

#ifdef __cplusplus
}
#endif
