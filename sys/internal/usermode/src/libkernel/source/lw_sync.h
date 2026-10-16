#ifndef LIBKERNEL_LW_SYNC_H
#define LIBKERNEL_LW_SYNC_H

#include <psp2common/kernel/threadmgr.h>
#include <stdint.h>

struct LwMutexState {
	union {
		volatile uint64_t ownerAndWaiters;
		struct {
			volatile uint32_t owner;
			volatile uint32_t waiters;
		};
	};
	volatile int count;
	uint32_t attributes;
	SceUID uid;
	uint32_t reserved[3];
};

typedef struct SceKernelLwMutexInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	SceUInt32 attributes;
	SceKernelLwMutexWork *work;
	SceInt32 initialCount;
	SceInt32 currentCount;
	SceUID currentOwnerId;
	SceUInt32 numWaitThreads;
} SceKernelLwMutexInfo;

typedef struct SceKernelLwCondInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	SceUInt32 attributes;
	SceKernelLwCondWork *work;
	SceKernelLwMutexWork *mutex;
	SceUInt32 numWaitThreads;
} SceKernelLwCondInfo;

int _sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name,
		unsigned int attributes, int initialCount,
		const SceKernelLwMutexOptParam *option);

#endif
