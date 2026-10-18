/* Variable-size records returned by sceKernelGetModuleInfoForDebugger. */
#pragma once

#include <psp2kern/types.h>
#include <modulemgr/module_library.h>
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

/* sceKernelGetLostLibraryInfo */
typedef struct SceKernelLostLibraryInfo {
	SceSize size;
	SceUID modid;
	SceNID libnid;
	char libname[0x100];
	SceUInt16 version;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceNID *funcnid;
	void **funcent;
	SceNID *varnid;
	void **varent;
} SceKernelLostLibraryInfo;

/* sceKernelGetLostLibraryList* */
typedef struct SceKernelLostLibraryEntry {
	SceUID modid;
	SceNID libnid;
} SceKernelLostLibraryEntry;

/* sceKernelGetLibraryInfoForDebugger */
typedef struct SceKernelLibraryInfoForDebugger {
	SceSize size;
	SceUID libid;
	SceNID libnid;
	SceUInt16 version;
	SceUInt16 attr;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceUInt16 reserved;
	char libname[0x100];
	int clientCount;
	SceUID modid;
} SceKernelLibraryInfoForDebugger;

/* sceKernelGetLibraryExportInfoForDebugger */
typedef struct SceKernelLibraryExportEntry {
	SceNID nid;
	void *entry;
} SceKernelLibraryExportEntry;

_Static_assert(sizeof(SceKernelLostLibraryInfo) == 0x124, "SceKernelLostLibraryInfo size");
_Static_assert(sizeof(SceKernelLibraryInfoForDebugger) == 0x120,
	       "SceKernelLibraryInfoForDebugger size");
_Static_assert(sizeof(SceKernelLibraryExportEntry) == 8, "SceKernelLibraryExportEntry size");

int sceKernelGetLibraryInfoForDebugger(SceUID pid, SceUID libid,
				       SceKernelLibraryInfoForDebugger *info);

int sceKernelGetLibraryExportInfoForDebugger(SceUID pid, SceUID libid,
					     SceKernelLibraryExportEntry *list, SceSize *num,
					     SceUInt32 start);

int sceKernelGetModuleInfoForDebugger(SceUID pid,
				    SceKernelModuleInfoForDebugger *info, int *num);

int sceKernelGetMetaDataForDebugger(SceUID pid, SceUID modid,
				  void **metadata, SceSize *size);
