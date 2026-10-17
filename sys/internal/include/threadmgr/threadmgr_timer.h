/* SceKernelThreadMgr: complete kernel timer information. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

typedef struct ThreadmgrTimerInfo {
	SceSize size;
	SceUID timerId;
	char name[32];
	uint32_t attr;
	uint32_t running;
	uint64_t time;
	uint64_t current;
	uint64_t eventTime;
	uint64_t interval;
	uint32_t pulse;
	uint32_t repeat;
	int numWaitThreads;
	uint32_t reserved;
} ThreadmgrTimerInfo;

_Static_assert(sizeof(ThreadmgrTimerInfo) == 0x60, "timer info size");

int SceThreadmgrForDriver_19F3E92A(SceUID timerId, ThreadmgrTimerInfo *info);
