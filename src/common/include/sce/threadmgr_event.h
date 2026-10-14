/* Kernel event information returned to debugger clients. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

/* One requested event and one outcome of a wait on several events. */
typedef struct ThreadmgrWaitEvent {
	SceUID eventId;
	uint32_t pattern;
} ThreadmgrWaitEvent;

typedef struct ThreadmgrWaitEventResult {
	SceUID eventId;
	int result;
	uint32_t pattern;
	uint32_t reserved;
	uint64_t userData;
} ThreadmgrWaitEventResult;

_Static_assert(sizeof(ThreadmgrWaitEvent) == 8, "wait event size");
_Static_assert(sizeof(ThreadmgrWaitEventResult) == 24, "wait event result size");

int SceThreadmgrForDriver_36F8E58A(const ThreadmgrWaitEvent *events,
		uint32_t count, uint32_t mode, ThreadmgrWaitEventResult *results,
		SceUInt32 *timeout);

typedef struct ThreadmgrEventInfo {
	SceSize size;
	SceUID eventId;
	char name[32];
	uint32_t attr;
	uint32_t pattern;
	uint64_t userData;
	int numWaitThreads;
	uint32_t reserved;
} ThreadmgrEventInfo;

_Static_assert(sizeof(ThreadmgrEventInfo) == 0x40, "event info size");

int SceThreadmgrForDriver_CBA7FAAA(SceUID eventId, ThreadmgrEventInfo *info);
