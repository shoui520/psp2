/* SceNgs platform-owned process storage. */

#pragma once

#include <psp2kern/types.h>
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
