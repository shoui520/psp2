#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

/* Process-manager services registered with Sysroot by NID733C243E. */
typedef struct SysrootProcessmgrOps {
	SceSize size;
	int (*getUIDEntryHeap)(SceUID pid, void **entryHeap);
	SceUID (*getAddressSpace)(SceUID pid);
	int (*pidToAddressSpaceCB)(SceUID pid, void **addressSpaceCB);
	int (*getSelfAuthInfo)(SceUID pid, SceSelfAuthInfo *authInfo);
	int (*getName)(SceUID pid, char *name, SceSize size);
	int (*incResource)(SceUInt32 type);
	int (*decResource)(SceUInt32 type);
} SysrootProcessmgrOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootProcessmgrOps) == 0x20,
	       "Sysroot process-manager service table size");
_Static_assert(offsetof(SysrootProcessmgrOps, getSelfAuthInfo) == 0x10,
	       "Sysroot process-manager authentication query offset");
#endif

struct SceProcessObject;

/* Distinct process runtime services registered by NID D29BCA77. */
typedef struct SysrootProcessmgrRuntimeOps {
	SceSize size;
	void *(*getLibkernelAddr0)(struct SceProcessObject *process);
	void *(*getLibkernelAddr1)(struct SceProcessObject *process);
	void *(*getLibkernelAddr2)(struct SceProcessObject *process);
	void *(*allocCurrentHeap)(SceSize size, void *opt, const void *caller);
	int (*freeCurrentHeap)(void *ptr);
	void *(*allocRemoteHeap)(SceUID pid, SceSize size, void *opt, const void *caller);
	int (*freeRemoteHeap)(SceUID pid, void *ptr);
	int (*moduleStart2)(void);
	int (*inhibitProcEvent)(SceUID pid);
	int (*permitProcEvent)(SceUID pid);
	void *(*getObjectHeap)(SceUID pid, int kind);
	int (*createLocalStorage)(const char *name, SceSize size);
	void *(*getLocalStorageAddr)(SceUInt32 key);
	int (*getLocalStorageAddrForPid)(SceUID pid, SceUInt32 key, void **addr, int create);
	int (*debugSuspend)(SceUID pid, SceUInt32 state);
	int (*getSdkVersion)(SceUID pid, SceUInt32 *sdkVersion);
} SysrootProcessmgrRuntimeOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootProcessmgrRuntimeOps) == 0x44,
	       "Sysroot process-manager runtime service table size");
_Static_assert(offsetof(SysrootProcessmgrRuntimeOps, getSdkVersion) == 0x40,
	       "Sysroot process-manager SDK version service offset");
#endif
