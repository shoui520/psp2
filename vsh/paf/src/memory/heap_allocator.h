#ifndef PAF_MEMORY_HEAP_ALLOCATOR_H
#define PAF_MEMORY_HEAP_ALLOCATOR_H

#include "../thread/mutex.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct HeapAllocator HeapAllocator;

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

HeapAllocator *scePafCreateHeap(HeapAllocator *self, void *membase,
		size_t size, const char *name, const HeapAllocatorOption *option);
HeapAllocator *ScePafMisc_F36F1C66(HeapAllocator *self, void *membase,
		size_t size, const char *name, const HeapAllocatorOption *option);
HeapAllocator *scePafDeleteHeap(HeapAllocator *self);
void ScePafMisc_79F13D72(HeapAllocator *self);

#define HeapAllocatorGetType ScePafMisc_9B22F2FE
#define HeapAllocatorGetBlockType ScePafMisc_D9E9BEC8
#define HeapAllocatorGetName ScePafMisc_68B7420B
#define HeapAllocatorGetRange ScePafMisc_D4358B8E
#define HeapAllocatorGetFreeSize ScePafMisc_C32ECD73

uint32_t HeapAllocatorGetType(const HeapAllocator *self);
AllocationBlockType HeapAllocatorGetBlockType(const HeapAllocator *self);
const char *HeapAllocatorGetName(const HeapAllocator *self);
void HeapAllocatorGetRange(const HeapAllocator *self, void **membase, size_t *size);
size_t HeapAllocatorGetFreeSize(HeapAllocator *self);

#define HeapAllocatorCheckMemoryBounds ScePafMisc_0A463284
#define HeapAllocatorFreeDebug ScePafMisc_FE016005
#define HeapAllocatorReallocDebug ScePafMisc_18667637
#define HeapAllocatorReportMemoryBlocks ScePafMisc_786D314A

int HeapAllocatorCheckMemoryBounds(HeapAllocator *self, void *ptr);
void HeapAllocatorFreeDebug(HeapAllocator *self, void *ptr, const char *filename,
			   int32_t lineno, int32_t type);
void *HeapAllocatorReallocDebug(HeapAllocator *self, void *ptr, size_t size,
			       const char *filename, int32_t lineno, int32_t type);
int32_t HeapAllocatorReportMemoryBlocks(HeapAllocator *self,
		int (*callback)(void *, size_t, int, void *), void *userdata);

void *scePafMallocWithContext(HeapAllocator *allocator, size_t size);
void *scePafMallocAlignWithContext(HeapAllocator *allocator, size_t align, size_t size);
void *scePafReallocWithContext(HeapAllocator *allocator, void *ptr, size_t size);
void scePafFreeWithContext(HeapAllocator *allocator, void *ptr);

#endif
