/* Complete NGS system allocation and its memory-range cache. */

#pragma once

#include <scetypes.h>
#include <ngs/ngs_top.h>
#include <stddef.h>

#include "ngs_platform.h"

struct NgsModuleDefinition;
struct NgsSynRack;
struct NgsVoice;
struct NgsDspGraph;
struct NgsDspBatch;
struct NgsCallbackList;

typedef struct NgsLockedMemoryRange {
	void *address;
	SceSize size;
} NgsLockedMemoryRange;

typedef struct NgsSynSystem {
	int systemId;
	unsigned int signature;
	unsigned int MaxRacks;
	unsigned int MaxVoices;
	unsigned int Granularity;
	unsigned int SampleRate;
	unsigned int flags;
	int copyUserData;
	SceSize initialCoherentAllocationSize;
	struct NgsSynRack **racks;
	struct NgsSynRack **processingRacks;
	struct NgsSynRack **rackWorkList;
	struct NgsDspGraph *emptyGraph;
	struct NgsDspBatch *dspBatch;
	int pendingRackReleases;
	NgsPlatformContext *context;
	unsigned int ModulesLinked;
	unsigned int MaxModules;
	struct NgsModuleDefinition **modules;
	unsigned int nextRackId;
	unsigned int CycleCount;
	unsigned char state;
	SceNgsParamsErrorCallbackFunc paramErrorCallback;
	void *synthUpdateCallback;
	SceNgsBufferInfo debugBuffer;
	SceNgsBufferInfo callerDebugBuffer;
	SceUID debugMemBlock;
	void *dspHeap;
	SceSize dspHeapUsed;
	void *coherentAllocation;
	void *dspHeapAllocation;
	void *systemAllocation;
	void *callbackUserMemory;
	void *callbackKernelMemory;
	SceSize coherentAllocationSize;
	SceSize dspHeapAllocationSize;
	SceSize systemAllocationSize;
	SceSize callbackMemorySize;
	SceUID systemMemBlock;
	SceNgsCallbackInfo *rackReleaseInfo;
	SceNgsCallbackInfo *paramErrorInfo;
	struct NgsCallbackList *callbacks;
	struct NgsCallbackList *callerCallbacks;
	unsigned int callbackCount;
	unsigned int callbackCapacity;
	struct NgsVoice **updateVoices;
	unsigned int lockedRangeCount;
	NgsLockedMemoryRange lockedRanges[1024];
	unsigned int pullDataDeferred;
	unsigned int compiledSdkVersion;
	unsigned int fixForBugzilla89940;
} NgsSynSystem;

_Static_assert(sizeof(NgsLockedMemoryRange) == 8, "NgsLockedMemoryRange size");
_Static_assert(offsetof(NgsSynSystem, context) == 60, "NgsSynSystem context");
_Static_assert(offsetof(NgsSynSystem, lockedRanges) == 196, "NgsSynSystem ranges");
_Static_assert(offsetof(NgsSynSystem, compiledSdkVersion) == 8392,
	"NgsSynSystem compiled SDK");
_Static_assert(sizeof(NgsSynSystem) == 8400, "NgsSynSystem size");
