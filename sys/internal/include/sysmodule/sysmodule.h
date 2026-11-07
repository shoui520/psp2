/*
 * SceSysmodule's exports: the syscalls a process loads its system libraries
 * with, by the module IDs of sysmodule_ids.h.
 */
#pragma once

#include <scetypes.h>
#include <sysmodule/sysmodule_ids.h>

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
