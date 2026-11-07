/*
 * SceKernelModulemgr (os0:kd/modulemgr.skprx): its user syscalls (the SceModulemgr library),
 * apart from the kernel's exports so that user code can include them with the SDK's headers.
 */

#pragma once

#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceModulemgr: the user syscalls */
SceUID _sceKernelLoadModule(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *uopt);
int _sceKernelUnloadModule(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *uopt);
SceUInt32 sceKernelGetAllowedSdkVersionOnSystem(void);
SceUID sceKernelGetModuleIdByAddrForUser(const void *addr);
int sceKernelGetModuleInfoForUser(SceUID modid, SceKernelModuleInfo *uinfo);
int sceKernelInhibitLoadingModule(SceUInt32 flags);
int sceKernelIsCalledFromSysModule(const void *addr);

#ifdef __cplusplus
}
#endif
