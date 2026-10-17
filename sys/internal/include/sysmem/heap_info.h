/* Kernel heap information and the dlmalloc segment ranges it reports. */
#pragma once

#include <psp2kern/types.h>

struct malloc_segment_range {
	void *base;
	SceSize size;
};

_Static_assert(sizeof(struct malloc_segment_range) == 8,
	"malloc_segment_range size");

/* ksceKernelGetHeapInfo */
typedef struct HeapInfo {
	SceSize size;				/* 0x70 */
	SceUInt32 reserved;
	SceUID uid;				/* 0x08 */
	const char *name;			/* 0x0c */
	SceSize totalSize;			/* 0x10 */
	SceSize peakTotalSize;			/* 0x14 */
	SceSize maxSize;			/* 0x18 */
	SceSize footprint;			/* 0x1c */
	SceSize arena;				/* 0x20: mallinfo */
	SceSize ordblks;
	SceSize hblkhd;
	SceSize usmblks;
	SceSize uordblks;
	SceSize fordblks;
	SceSize keepcost;
	SceSize stateSize;			/* 0x3c: of the mspace's own chunk */
	SceSize maxSystemBytes;			/* 0x40: malloc_stats */
	SceSize systemBytes;
	SceSize inUseBytes;
	SceUInt32 chunks;			/* 0x4c */
	SceUInt32 peakChunks;
	SceSize used;
	SceSize peakUsed;
	SceSize maxChunk;			/* 0x5c */
	SceUInt32 blockCount;			/* 0x60 */
	SceUInt32 maxBlocks;			/* 0x64: in */
	struct malloc_segment_range *blockList;	/* 0x68: in */
	SceUInt32 flags;			/* 0x6c */
} HeapInfo;

_Static_assert(sizeof(HeapInfo) == 0x70, "HeapInfo size");
