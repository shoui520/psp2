/* Variable-size records returned by sceKernelGetModuleInfoForDebugger. */
#pragma once

#include <psp2kern/types.h>
#include <modulemgr/modulemgr_control.h>

typedef struct SceKernelSegmentInfoForDebugger {
	SceSize size;			/* 0x14 */
	SceUInt32 perms;
	void *vaddr;
	SceSize memsz;
	SceSize align;
} SceKernelSegmentInfoForDebugger;

typedef struct SceKernelModuleInfoForDebugger {
	SceSize size;
	SceUID modid;
	SceUInt32 sdkVersion;
	SceUInt32 version;
	SceUInt8 type;			/* 0x10, 0x20: fixed address SELF */
	SceUInt8 reserved0;
	SceUInt16 flags;
	SceModuleEntry moduleStart;
	SceUInt32 reserved1;
	SceModuleEntry moduleStop;
	void *moduleExit;
	char name[0x1c];
	SceUInt32 reserved2;
	SceUInt32 status;
	SceUInt32 fingerprint;
	SceUInt32 nSegments;
	SceKernelSegmentInfoForDebugger segment[];
	/* followed by SceKernelModuleExidxInfo */
} SceKernelModuleInfoForDebugger;

typedef struct SceKernelModuleExidxInfo {
	void *exidxTop;
	void *exidxBtm;
	void *extabStart;
	void *extabEnd;
} SceKernelModuleExidxInfo;

int sceKernelGetModuleInfoForDebugger(SceUID pid,
				    SceKernelModuleInfoForDebugger *info, int *num);

int sceKernelGetMetaDataForDebugger(SceUID pid, SceUID modid,
				  void **metadata, SceSize *size);
