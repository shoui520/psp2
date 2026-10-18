#ifndef LIBKERNEL_LW_SYNC_H
#define LIBKERNEL_LW_SYNC_H

#include <kernel/threadmgr.h>
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

int _sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name,
		unsigned int attributes, int initialCount,
		const SceKernelLwMutexOptParam *option);

#endif
