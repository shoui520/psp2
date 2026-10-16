/* Native lockup process/record rings. */
#ifndef GPU_LOCKUP_HISTORY_H
#define GPU_LOCKUP_HISTORY_H

#include <stdint.h>

struct GpuLockupRecord {
	/* Values returned by ExtractRegisterDumpFrameInfo, in native record order. */
	unsigned frameInfoSecond;
	unsigned reason;
	unsigned frameInfoFirst;
	unsigned analysisDetail;
	uint64_t timestamp;
};

struct GpuLockupProcessHistory {
	unsigned recordCount;
	int processUid;
	char processName[32];
	struct GpuLockupRecord firstRecord;
	struct GpuLockupRecord records[8];
};

struct GpuLockupHistory {
	unsigned identifier;
	unsigned processCount;
	unsigned size;
	/* Unused header word: only zeroed and copied by the native readers/writer. */
	unsigned reserved;
	struct GpuLockupProcessHistory processes[8];
};

_Static_assert(sizeof(struct GpuLockupRecord) == 0x18, "record size");
_Static_assert(sizeof(struct GpuLockupProcessHistory) == 0x100, "process history size");
_Static_assert(sizeof(struct GpuLockupHistory) == 0x810, "history size");

/* The native thread publishes stack buffers here; startup is still pending. */
extern struct GpuLockupHistory *volatile lockupHistory;
extern struct GpuLockupHistory *volatile orderedLockupHistory;

#endif
