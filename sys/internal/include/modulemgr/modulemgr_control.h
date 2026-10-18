/* Module control blocks shared with kernel debugger consumers. */
#pragma once
#include <stddef.h>
#include <scetypes.h>

struct SceModuleExports;
struct SceModuleImports;
struct LibEntCB;
struct LibStubCB;
struct SharedModuleEntry;
struct SceModuleProbesInfo;

typedef struct SceModuleSegment {
	SceSize filesz;
	SceSize memsz;
	SceUInt8 perms;
	SceUInt8 extraPages;		/* pages reserved in front of the image */
	SceUInt16 alignLog2;
	void *vaddr;
	SceUID memblk;
} SceModuleSegment;

typedef int (*SceModuleEntry)(SceSize args, void *argp);

typedef struct SceModuleCB {
	struct SceModuleCB *next;	/* LibraryDB.modules */
	SceUInt16 flags;
	SceUInt8 status;
	SceUInt8 reserved0;
	SceUInt32 sdkVersion;
	SceUID modid;
	SceUID puid;
	SceUID pid;
	SceUInt16 attr;
	SceUInt8 version[2];
	const char *name;
	struct SceModuleExports *libentTop;
	struct SceModuleExports *libentBtm;
	void *libstubTop;
	void *libstubBtm;
	SceUInt32 fingerprint;
	void *tlsStart;
	SceSize tlsFilesz;
	SceSize tlsMemsz;
	void *exidxTop;
	void *exidxBtm;
	void *extabStart;
	void *extabEnd;
	SceUInt16 libEntCount;
	SceUInt16 libStubCount;
	void *linkInfo;			/* LibEntCB/LibStubCB storage */
	struct SceModuleExports *libent;	/* kernel copy of the export table */
	struct LibEntCB *libEntCB;
	struct SceModuleImports *libstub;	/* normalised import table */
	struct LibStubCB *libStubCB;
	char *path;
	int nSegments;
	SceModuleSegment segment[3];
	void *metadata;			/* segment type 0x6fffff00 */
	SceSize metadataSize;
	SceModuleEntry moduleStart;
	SceModuleEntry moduleStop;
	void *moduleExit;
	void *moduleBootstart;
	void *procParam;
	void *startThreadParam;
	void *stopThreadParam;
	void *exidxCopy;		/* tool heap copy of the exidx table */
	struct SharedModuleEntry *shared;
	SceUInt32 reserved1;
	struct SceModuleProbesInfo *probesInfo;
	void **probes;
	SceUInt32 reserved2;
	SceUInt32 reserved3;
} SceModuleCB;

#ifndef __cplusplus
_Static_assert(sizeof(SceModuleSegment) == 0x14, "module segment record");
_Static_assert(sizeof(SceModuleCB) == 0xec, "module control record");
_Static_assert(offsetof(SceModuleCB, modid) == 0xc, "module ID");
_Static_assert(offsetof(SceModuleCB, version) == 0x1a, "module version");
_Static_assert(offsetof(SceModuleCB, name) == 0x1c, "module name");
_Static_assert(offsetof(SceModuleCB, fingerprint) == 0x30, "module fingerprint");
_Static_assert(offsetof(SceModuleCB, path) == 0x68, "module path");
_Static_assert(offsetof(SceModuleCB, nSegments) == 0x6c, "module segment count");
_Static_assert(offsetof(SceModuleCB, segment) == 0x70, "module segments");
#endif
