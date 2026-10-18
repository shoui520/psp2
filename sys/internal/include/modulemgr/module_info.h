/*
 * SceKernelModulemgr: the record its sceKernelGetModuleInfo syscall fills, and the syscalls user
 * code finds its modules with (LibKernel calls them). The state names are ours.
 */

#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceKernelModuleInfo.state */
#define SCE_KERNEL_MODULE_STATE_LOADED		2	/* loaded, or still starting */
#define SCE_KERNEL_MODULE_STATE_STARTED		6
#define SCE_KERNEL_MODULE_STATE_STOPPED		9	/* stopping or stopped */

/* a loaded segment of a module (SceKernelModuleInfo.segments) */
typedef struct SceKernelSegmentInfo {
	SceSize size;			/* 0x18 */
	SceUInt perms;
	void *vaddr;
	SceSize memsz;
	SceSize filesz;
	SceUInt reserved;
} SceKernelSegmentInfo;

/* a module's information (SceModulemgrForKernel_DAA90093, the sceKernelGetModuleInfo syscall) */
typedef struct SceKernelModuleInfo {
	SceSize size;			/* 0x1b8 */
	SceUID modid;
	SceUInt16 modattr;
	SceUInt8 modver[2];
	char module_name[28];
	SceUInt reserved;
	void *start_entry;
	void *stop_entry;
	void *exit_entry;
	void *exidx_top;
	void *exidx_btm;
	void *extab_top;
	void *extab_btm;
	void *tlsInit;
	SceSize tlsInitSize;
	SceSize tlsAreaSize;
	char path[256];
	SceKernelSegmentInfo segments[4];
	SceUInt state;			/* SCE_KERNEL_MODULE_STATE_* */
} SceKernelModuleInfo;

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelSegmentInfo) == 0x18, "SceKernelSegmentInfo size");
_Static_assert(sizeof(SceKernelModuleInfo) == 0x1b8, "SceKernelModuleInfo size");
#endif

/* SceModulemgr: the user syscalls */
SceUID sceKernelGetModuleIdByAddr(const void *addr);
int sceKernelGetModuleInfo(SceUID modid, SceKernelModuleInfo *uinfo);

#ifdef __cplusplus
}
#endif
