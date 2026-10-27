/* Kernel heap information and the dlmalloc segment ranges it reports. */
#pragma once

#include <scetypes.h>

struct malloc_segment_range {
	void *base;
	SceSize size;
};

_Static_assert(sizeof(struct malloc_segment_range) == 8,
	"malloc_segment_range size");

/* sceKernelGetHeapInfo */
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

#define FIXED_HEAP_CPUS			4

/* freed objects a CPU keeps for itself, linked through each object's first word */
typedef struct FixedHeapCpuCache {
	SceUInt16 max;
	SceUInt16 count;			/* "nCount" */
	SceUInt32 reserved;			/* only copied into SceKernelFixedHeapInfo */
	void *head;
	void *tail;
} FixedHeapCpuCache;

/* sceKernelGetFixedHeapInfo */
typedef struct SceKernelFixedHeapInfo {
	SceSize size;				/* 0x90 */
	SceUInt32 itemSize;
	SceSize totalSize;			/* of all pages */
	SceUInt32 perPage;
	SceUInt32 freeCount;
	SceUInt32 peakCount;
	void *cursor;				/* 0x18 */
	SceUInt16 usedInPage;
	SceUInt16 perPage16;
	SceUInt32 listCount;			/* 0x20: "nListCount", counted on the free list */
	FixedHeapCpuCache caches[FIXED_HEAP_CPUS];	/* 0x24 */
	void *freeHead;				/* 0x64 */
	void *freeTail;
	SceUInt32 attr;
	SceSize pageSize;			/* 0x70 */
	SceUInt16 totalCount;
	SceUInt16 freeCount16;
	SceUInt32 maxPages;
	SceUInt32 pageCount;
	void **pages;				/* 0x80 */
	void *currentPage;
	char *name;
	void **blocks;
} SceKernelFixedHeapInfo;

_Static_assert(sizeof(SceKernelFixedHeapInfo) == 0x90, "SceKernelFixedHeapInfo size");

/* sceKernelNameHeapGetInfo */
typedef struct SceKernelNameHeapInfo {
	SceSize size;				/* 0x14 */
	struct {
		SceUInt32 count;
		SceUInt32 peak;
	} heaps[2];
} SceKernelNameHeapInfo;

/* sceUIDEntryHeapGetInfo */
typedef struct SceKernelUIDEntryHeapInfo {
	SceSize size;				/* 0x18 */
	SceUInt32 itemSize;
	SceSize totalSize;
	SceUInt32 perPage;
	SceUInt32 freeCount;
	SceUInt32 peakCount;
} SceKernelUIDEntryHeapInfo;

struct ObjectHeap;

/* the size classes of an object heap */
typedef struct ObjectHeapInfo {
	SceSize size;				/* 200 */
	SceUInt32 count;
	struct {
		SceSize size;
		void *heap;
		SceUInt32 reserved;
	} sizeClasses[16];
} ObjectHeapInfo;

/* name unknown: called on every allocation and free (SceSysmemForKernel_42834422; debug
   builds only); an allocation reports its caller and the allocating function by name hash */
typedef struct HeapHooks {
	void (*alloc)(SceUID uid, void *ptr, SceSize size, const void *caller, SceUInt32 func);
	void (*free)(SceUID uid, void *ptr);
} HeapHooks;

/* the function hashes the allocation hook gets (sha1(name)[0..3], as in the debug records) */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY		0x3b956c63	/* sceKernelAllocHeapMemory */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPTION	0xdb747aeb	/* sceKernelAllocHeapMemoryWithOption */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPT	0x16fdddfd	/* PLACEHOLDER: name */
#define HEAP_FUNC_ALLOC				0x52e5216a	/* sceKernelAlloc */
#define HEAP_FUNC_ALLOC_WITH_OPT		0x65ff9138	/* sceKernelAllocWithOpt */
#define HEAP_FUNC_ALLOC_UNCACHE			0x010b9fc3	/* sceKernelAllocUncacheHeapMemory */
#define HEAP_FUNC_ALLOC_UNCACHE_WITH_OPTION	0xaa952bc9	/* sceKernelAllocUncacheHeapMemoryWithOption */
#define HEAP_FUNC_ALLOC_METHOD			0xb2e40fe1	/* AllocMethod (deci4p_sdbgp's table) */

int sceKernelGetFixedHeapInfo(SceUID uid, SceKernelFixedHeapInfo *info);
int SceSysmemForKernel_DBEACE2C(struct ObjectHeap *heap, ObjectHeapInfo *info);
/* processmgr.c has a static function of this name (Sony's records): declared here, not in
   sysmem.h, so processmgr does not see it */
struct ObjectHeap *sceKernelGetObjectHeap(void);
int SceSysmemForKernel_42834422(const HeapHooks *hooks);
int sceKernelGetHeapInfoAll(SceUInt32 level, HeapInfo *infos, SceSize size, SceSize *count);
