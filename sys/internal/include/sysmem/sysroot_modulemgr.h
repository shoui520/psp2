#pragma once

#include <stddef.h>
#include <scetypes.h>

/* info of the library function a stub address calls (sysroot callback) */
struct ModulemgrStubInfo {
	SceSize size;			/* 0x3c */
	SceUID modid;
	SceUInt16 attr;
	SceUInt8 version[2];
	char moduleName[0x1c];
	SceUInt32 fingerprint;
	const char *libname;
	SceUInt32 reserved0;
	SceUInt16 libVersion;
	SceUInt16 reserved1;
	SceUInt32 funcnid;
};

/* one frame of a backtrace */
struct SceKernelBacktraceFrameForKernel {
	SceUInt32 sp;
	SceUInt32 pc;
	SceUInt32 fingerprint;
	SceUInt32 textBase;
};

/* Module-manager services registered with Sysroot by NID B171CC2D. */
typedef struct SysrootModulemgrOps {
	SceSize size;
	int (*getStubInfo)(SceUID pid, SceUInt32 addr, struct ModulemgrStubInfo *info);
	void (*showCallerInfo)(SceUID pid, SceUInt32 addr);
	int (*getModuleInfoMinByAddr)(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint,
				    void **textBase, char *name);
	int (*backtrace)(SceUID thid, struct SceKernelBacktraceFrameForKernel *frames,
			 SceSize size, SceSize *count, SceUInt32 flags);
	int (*printBacktrace)(SceUID pid,
			      const struct SceKernelBacktraceFrameForKernel *frames,
			      SceSize count, SceUInt32 start);
} SysrootModulemgrOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootModulemgrOps) == 0x18,
	       "Sysroot module-manager service table size");
_Static_assert(offsetof(SysrootModulemgrOps, backtrace) == 0x10,
	       "Sysroot module-manager backtrace service offset");
#endif

#ifndef __cplusplus
_Static_assert(sizeof(struct ModulemgrStubInfo) == 0x3c, "module stub info size");
#endif
