#ifndef PAF_SRC_MEMORY_HEAP_ALLOCATOR_H
#define PAF_SRC_MEMORY_HEAP_ALLOCATOR_H

#include "../thread/mutex.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct HeapAllocator HeapAllocator;

typedef struct HeapInfo {
	size_t num_of_free;
	size_t total_free_size;
} HeapInfo;

_Static_assert(sizeof(HeapInfo) == 8, "HeapInfo size");

/* paf::memory::HeapAllocator and Option, with Vita's virtual-method layout. */
typedef enum AllocationBlockType {
	AllocationBlockType_User = 0,
	AllocationBlockType_User_Cdlg = 1
} AllocationBlockType;

typedef struct HeapAllocatorOption {
	uint32_t entry_max;
	size_t warning_size;
	bool disable_dbgprint;
	bool disable_assert_on_alloc_failure;
	AllocationBlockType memblk_type;
	uint32_t alignment;
} HeapAllocatorOption;

typedef struct MemoryAllocatorMethods {
	uint32_t (*GetType)(const HeapAllocator *self);
	AllocationBlockType (*GetBlockType)(const HeapAllocator *self);
	const char *(*GetName)(const HeapAllocator *self);
	void (*GetRange)(const HeapAllocator *self, void **membase, size_t *size);
	size_t (*GetFreeSize)(HeapAllocator *self);
	HeapAllocator *(*Destruct)(HeapAllocator *self);
	void (*Delete)(HeapAllocator *self);
} MemoryAllocatorMethods;

struct HeapAllocator {
	const MemoryAllocatorMethods *vtable;
	void *m_heap_id;
	void *m_heap_addr;
	size_t m_heap_size;
	char m_heap_name[32];
	bool m_memory_attached;
	bool m_heap_disable_dbgprint;
	bool m_heap_disable_assert_on_alloc_failure;
	Mutex m_mutex;
	SceUID m_memblk_id;
	AllocationBlockType m_heap_memblk_type;
};

_Static_assert(sizeof(HeapAllocatorOption) == 20, "HeapAllocator::Option size");
_Static_assert(sizeof(HeapAllocator) == 96, "HeapAllocator size");
_Static_assert(offsetof(HeapAllocator, m_mutex) == 56, "HeapAllocator mutex");
_Static_assert(offsetof(HeapAllocator, m_memblk_id) == 88, "HeapAllocator memblk");
_Static_assert(offsetof(HeapAllocator, m_heap_memblk_type) == 92,
	       "HeapAllocator block type");
_Static_assert(offsetof(HeapAllocatorOption, memblk_type) == 12,
	       "HeapAllocator::Option block type");

/*
 * The C view of paf::memory::HeapAllocator (include/memory/heap_allocator.h)
 * for the C files not yet converted: its methods by their link names.
 */
#define HeapAllocatorConstruct _ZN3paf6memory13HeapAllocatorC1EPvjPKcPKNS1_6OptionE
#define HeapAllocatorDestroy _ZN3paf6memory13HeapAllocatorD1Ev
#define HeapAllocatorMalloc _ZN3paf6memory13HeapAllocator14sce_paf_mallocEj
#define HeapAllocatorFree _ZN3paf6memory13HeapAllocator12sce_paf_freeEPv
#define HeapAllocatorRealloc _ZN3paf6memory13HeapAllocator15sce_paf_reallocEPvj
#define HeapAllocatorMemalign _ZN3paf6memory13HeapAllocator16sce_paf_memalignEjj
#define HeapAllocatorCalloc _ZN3paf6memory13HeapAllocator14sce_paf_callocEjj
#define HeapAllocatorTrim _ZN3paf6memory13HeapAllocator13sce_paf_mtrimEv
#define HeapAllocatorUsableSize _ZN3paf6memory13HeapAllocator20sce_paf_musable_sizeEPv
#define HeapAllocatorDumpHeapInfo _ZN3paf6memory13HeapAllocator22sce_paf_dump_heap_infoEi
#define HeapAllocatorDumpHeapStatus _ZN3paf6memory13HeapAllocator24sce_paf_dump_heap_statusEv
#define HeapAllocatorReportMemoryBlocks \
	_ZN3paf6memory13HeapAllocator27sce_paf_report_memoryblocksEPFiPvjiS2_ES2_
#define HeapAllocatorCheckMemoryBoundsAll _ZN3paf6memory13HeapAllocator26sce_paf_check_memoryboundsEv

HeapAllocator *HeapAllocatorConstruct(HeapAllocator *self, void *membase,
		size_t size, const char *name, const HeapAllocatorOption *option);
HeapAllocator *HeapAllocatorDestroy(HeapAllocator *self);
void *HeapAllocatorMalloc(HeapAllocator *self, size_t size);
void HeapAllocatorFree(HeapAllocator *self, void *ptr);
void *HeapAllocatorRealloc(HeapAllocator *self, void *ptr, size_t size);
void *HeapAllocatorMemalign(HeapAllocator *self, size_t alignment, size_t size);
void *HeapAllocatorCalloc(HeapAllocator *self, size_t nitems, size_t size);
void HeapAllocatorTrim(HeapAllocator *self);
size_t HeapAllocatorUsableSize(HeapAllocator *self, const void *ptr);
void HeapAllocatorDumpHeapInfo(HeapAllocator *self, int32_t sort_policy);
void HeapAllocatorDumpHeapStatus(HeapAllocator *self);
int32_t HeapAllocatorReportMemoryBlocks(HeapAllocator *self,
		int (*callback)(void *, size_t, int, void *), void *userdata);
int32_t HeapAllocatorCheckMemoryBoundsAll(HeapAllocator *self);

#endif
