/* Native GPU MMU and process-context layouts. */
#ifndef GPU_MMU_H
#define GPU_MMU_H

#include <gpu/gpu-heap.h>

typedef struct GpuPageTablePool {
	int uid;
	GpuHeap *heap;
} GpuPageTablePool;

typedef struct GpuPageTable {
	unsigned reservedHead;
	GpuPageTablePool *pool;
	unsigned *entries;
	unsigned pageShift;
	unsigned entryCount;
	unsigned physicalAddress;
	unsigned reservedTail;
} GpuPageTable;

typedef struct GpuMmuContext {
	unsigned pageDirectoryMemblockUid;
	unsigned *pageDirectory;
	unsigned pageDirectoryPhysicalAddress;
	GpuPageTablePool pageTablePool;
	GpuPageTable *pageTables[1024];
} GpuMmuContext;

typedef struct GpuMemoryContext {
	unsigned pid;
	GpuMmuContext mmu;
	GpuHeap *heaps;
	struct Resource *hResItem;
	struct GpuMemoryContext *next;
	int processLocalStorageUid;
} GpuMemoryContext;

/* SCE process-local page-directory description. Both constructors clear the
 * final word; no reader of that word has been observed in the GPU module.
 */
typedef struct GpuProcessMemory {
	unsigned pageDirectoryMemblockUid;
	unsigned *pageDirectory;
	unsigned pageDirectoryPhysicalAddress;
	unsigned reserved;
} GpuProcessMemory;

_Static_assert(sizeof(GpuPageTable) == 0x1c, "native page-table object size");
_Static_assert(sizeof(GpuPageTablePool) == 8, "native pool UID/heap pair");
_Static_assert(sizeof(GpuMemoryContext) == 0x1028, "native memory context size");
_Static_assert(offsetof(GpuMemoryContext, heaps) == 0x1018, "native heap list");
_Static_assert(offsetof(GpuMemoryContext, next) == 0x1020, "native process list");
_Static_assert(offsetof(GpuMemoryContext, mmu.pageTablePool.uid) == 0x10,
	       "native page-table pool UID");
_Static_assert(offsetof(GpuMmuContext, pageTables) == 0x14,
	       "native page-table object pointers");

#endif
