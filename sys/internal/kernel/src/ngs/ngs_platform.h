/* SceNgs platform-owned process storage. */

#pragma once

#include <scetypes.h>
#include <ngs/ngs_top.h>
#include <threadmgr/fast_mutex_object.h>

struct NgsSynSystem;

typedef struct NgsPlatformContext {
	unsigned char reserved[12];
	struct ThreadmgrFastMutex systemMutex;
	int systemLock;
	int paramsLock;
	int voiceLock;
	int updateLock;
	int transferLock;
	SceUID codecMutex;
	void *systemHeap;
} NgsPlatformContext;

_Static_assert(sizeof(NgsPlatformContext) == 104, "NgsPlatformContext size");

typedef struct NgsVeneziaAllocation {
	void *address;
	SceSize size;
} NgsVeneziaAllocation;

typedef struct NgsProcessStorage {
	SceUID memBlocks[65];
	NgsVeneziaAllocation allocations[65];
	void *systemHeap;
	int systemId;
	int runState;
	int suspendRequested;
	int copyUserData;
	struct NgsSynSystem *system;
	int processLock;
} NgsProcessStorage;

_Static_assert(sizeof(NgsProcessStorage) == 808, "NgsProcessStorage size");

typedef struct NgsProcessEntry {
	SceUID pid;
	struct NgsProcessStorage *storage;
} NgsProcessEntry;

typedef struct NgsPlatformState {
	void *sharedMemory;
	unsigned int sharedPhysicalAddress;
	unsigned int processCount;
	SceUID globalMutex;
	int processStorageKey;
	NgsProcessEntry processes[8];
} NgsPlatformState;

_Static_assert(sizeof(NgsPlatformState) == 84, "NgsPlatformState size");

/* ngs_platform.c */
int ngsPlatformInitialize(void);
int ngsPlatformShutdown(void);
int ngsProcessInitialize(SceUID pid);
int ngsProcessSuspend(SceUID pid);
int ngsProcessResume(SceUID pid);
int ngsGetSystemInstanceCount(void);
int ngsGetSystemExtraMemorySize(SceSize *size);
int ngsGetProcessStorage(NgsProcessStorage **storage);
int ngsGetAlignedMemorySize(SceSize size, int unused, int unused2,
			int unused3, SceSize *alignedSize);
int ngsReleaseDebugBuffer(SceNgsBufferInfo *buffer, SceUID memBlock);
int ngsAllocateDebugBuffer(SceNgsBufferInfo *buffer, SceUID *memBlock, int size);
