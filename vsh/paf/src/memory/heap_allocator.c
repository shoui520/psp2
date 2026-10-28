#include "heap_allocator.h"
#include "dlmalloc.h"
#include "../std/new.h"
#include "../std/string.h"
#include "../std/stdlib.h"
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>

typedef struct MemoryAllocatorClass {
	int offset_to_top;
	const void *type_info;
	MemoryAllocatorMethods methods;
} MemoryAllocatorClass;

static HeapAllocator *MemoryAllocatorDestruct(HeapAllocator *self);
static void MemoryAllocatorDelete(HeapAllocator *self);

static const MemoryAllocatorClass memoryAllocatorClass = {
	0, NULL, {
		(uint32_t (*)(const HeapAllocator *))__cxa_pure_virtual,
		(AllocationBlockType (*)(const HeapAllocator *))__cxa_pure_virtual,
		(const char *(*)(const HeapAllocator *))__cxa_pure_virtual,
		(void (*)(const HeapAllocator *, void **, size_t *))__cxa_pure_virtual,
		(size_t (*)(HeapAllocator *))__cxa_pure_virtual,
		MemoryAllocatorDestruct, MemoryAllocatorDelete
	}
};

static const MemoryAllocatorClass heapAllocatorClass = {
	0, NULL, {
		HeapAllocatorGetType, HeapAllocatorGetBlockType,
		HeapAllocatorGetName, HeapAllocatorGetRange,
		HeapAllocatorGetFreeSize, scePafDeleteHeap, ScePafMisc_79F13D72
	}
};

uint32_t HeapAllocatorGetType(const HeapAllocator *self)
{
	return 0;
}

AllocationBlockType HeapAllocatorGetBlockType(const HeapAllocator *self)
{
	return self->m_heap_memblk_type;
}

const char *HeapAllocatorGetName(const HeapAllocator *self)
{
	return self->m_heap_name;
}

void HeapAllocatorGetRange(const HeapAllocator *self, void **membase, size_t *size)
{
	void *base = self->m_heap_addr;
	size_t length = self->m_heap_size;

	*membase = base;
	*size = length;
}

size_t HeapAllocatorGetFreeSize(HeapAllocator *self)
{
	struct mallinfo info;

	MutexLock(&self->m_mutex);
	info = mspace_mallinfo(self->m_heap_id);
	MutexUnlock(&self->m_mutex);
	return info.fordblks;
}

int HeapAllocatorCheckMemoryBounds(HeapAllocator *self, void *ptr)
{
	return mspace_check_memorybounds(self->m_heap_id, ptr, (unsigned int)self);
}

void *scePafMallocWithContext(HeapAllocator *self, size_t size)
{
	Mutex *mutex = &self->m_mutex;
	void *result;

	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	MutexLock(mutex);
	result = mspace_malloc(self->m_heap_id, size, (unsigned int)self);
	if (!result && !self->m_heap_disable_dbgprint)
		ScePafStdc_D41FD265();
	MutexUnlock(mutex);
	return result;
}

void *HeapAllocatorMallocDebug(HeapAllocator *self, size_t size,
		const char *filename, int32_t lineno, int32_t type)
{
	return scePafMallocWithContext(self, size);
}

void *HeapAllocatorCalloc(HeapAllocator *self, size_t nitems, size_t size)
{
	size_t fullsz = nitems * size;
	Mutex *mutex = &self->m_mutex;
	void *result;

	if (fullsz > 0x1fffffff)
		ScePafStdc_D608246A();
	MutexLock(mutex);
	result = mspace_malloc(self->m_heap_id, fullsz, (unsigned int)self);
	if (result)
		sce_paf_memset(result, 0, fullsz);
	if (!result && !self->m_heap_disable_dbgprint)
		ScePafStdc_D41FD265();
	MutexUnlock(mutex);
	return result;
}

void *HeapAllocatorCallocDebug(HeapAllocator *self, size_t nitems, size_t size,
		const char *filename, int32_t lineno, int32_t type)
{
	return HeapAllocatorCalloc(self, nitems, size);
}

void *scePafMallocAlignWithContext(HeapAllocator *self, size_t alignment, size_t size)
{
	Mutex *mutex = &self->m_mutex;
	void *result;

	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	MutexLock(mutex);
	result = mspace_memalign(self->m_heap_id, alignment, size, (unsigned int)self);
	if (!result && !self->m_heap_disable_dbgprint)
		ScePafStdc_D41FD265();
	MutexUnlock(mutex);
	return result;
}

void *HeapAllocatorMemalignDebug(HeapAllocator *self, size_t alignment, size_t size,
		const char *filename, int32_t lineno, int32_t type)
{
	return scePafMallocAlignWithContext(self, alignment, size);
}

void HeapAllocatorTrim(HeapAllocator *self)
{
	Mutex *mutex = &self->m_mutex;

	MutexLock(mutex);
	mspace_trim(self->m_heap_id, 0);
	MutexUnlock(mutex);
}

size_t HeapAllocatorUsableSize(HeapAllocator *self, const void *ptr)
{
	Mutex *mutex = &self->m_mutex;
	size_t size;

	MutexLock(mutex);
	size = mspace_usable_size(ptr);
	MutexUnlock(mutex);
	return size;
}

void HeapAllocatorDumpHeapStatus(HeapAllocator *self)
{
	Mutex *mutex = &self->m_mutex;

	MutexLock(mutex);
	(void)mspace_mallinfo(self->m_heap_id);
	MutexUnlock(mutex);
	ScePafStdc_D41FD265();
}

void HeapAllocatorDumpHeapInfo(HeapAllocator *self, int32_t sort_policy)
{
	HeapAllocatorDumpHeapStatus(self);
}

void HeapAllocatorQueryInfo(HeapAllocator *self, HeapInfo *info)
{
	Mutex *mutex = &self->m_mutex;
	struct mallinfo minfo;

	MutexLock(mutex);
	minfo = mspace_mallinfo(self->m_heap_id);
	MutexUnlock(mutex);
	info->num_of_free = minfo.ordblks;
	info->total_free_size = minfo.fordblks;
}

int32_t HeapAllocatorCheckMemoryBoundsAll(HeapAllocator *self)
{
	Mutex *mutex = &self->m_mutex;

	MutexLock(mutex);
	MutexUnlock(mutex);
	return 0;
}

void scePafFreeWithContext(HeapAllocator *self, void *ptr)
{
	Mutex *mutex = &self->m_mutex;

	MutexLock(mutex);
	if (mspace_check_memorybounds(self->m_heap_id, ptr, (unsigned int)self))
		ScePafStdc_D41FD265();
	mspace_free(self->m_heap_id, ptr, (unsigned int)self);
	MutexUnlock(mutex);
}

void HeapAllocatorFreeDebug(HeapAllocator *self, void *ptr, const char *filename,
			   int32_t lineno, int32_t type)
{
	scePafFreeWithContext(self, ptr);
}

void *scePafReallocWithContext(HeapAllocator *self, void *ptr, size_t size)
{
	Mutex *mutex = &self->m_mutex;
	void *result;

	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	MutexLock(mutex);
	if (mspace_check_memorybounds(self->m_heap_id, ptr, (unsigned int)self))
		ScePafStdc_D41FD265();
	result = mspace_realloc(self->m_heap_id, ptr, size, (unsigned int)self);
	if (!result && !self->m_heap_disable_dbgprint)
		ScePafStdc_D41FD265();
	MutexUnlock(mutex);
	return result;
}

void *HeapAllocatorReallocDebug(HeapAllocator *self, void *ptr, size_t size,
			       const char *filename, int32_t lineno, int32_t type)
{
	return scePafReallocWithContext(self, ptr, size);
}

int32_t HeapAllocatorReportMemoryBlocks(HeapAllocator *self,
		int (*callback)(void *, size_t, int, void *), void *userdata)
{
	Mutex *mutex = &self->m_mutex;
	int32_t count;

	MutexLock(mutex);
	count = mspace_report_free_blocks(self->m_heap_id, callback, userdata);
	MutexUnlock(mutex);
	return count;
}

HeapAllocator *scePafCreateHeap(HeapAllocator *self, void *membase,
		size_t size, const char *name, const HeapAllocatorOption *option)
{
	SceKernelMemBlockType type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
	void *base;
	size_t length, alignment;

	self->m_heap_addr = membase;
	self->m_heap_size = size;
	self->vtable = &heapAllocatorClass.methods;
	self->m_heap_disable_dbgprint = false;
	self->m_memory_attached = false;
	self->m_heap_disable_assert_on_alloc_failure = true;
	MutexConstruct(&self->m_mutex, name, MutexOption_None);
	self->m_heap_memblk_type = AllocationBlockType_User;
	if (option && option->memblk_type != AllocationBlockType_User) {
		self->m_heap_memblk_type = option->memblk_type;
		if (option->memblk_type == AllocationBlockType_User_Cdlg)
			type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	}
	if (option) {
		if (option->disable_dbgprint)
			self->m_heap_disable_dbgprint = true;
		if (!option->disable_assert_on_alloc_failure)
			self->m_heap_disable_assert_on_alloc_failure = false;
	}
	sce_paf_strncpy(self->m_heap_name, name, 31);
	base = self->m_heap_addr;
	self->m_heap_name[31] = '\0';
	if (base) {
		self->m_memory_attached = true;
	} else {
		size_t extra = 0;
		SceUID block;

		length = self->m_heap_size;
		if (length & 4095)
			extra = 4096 - (length & 4095);
		block = sceKernelAllocMemBlock(self->m_heap_name, type,
					      length + extra, NULL);
		base = self->m_heap_addr;
		self->m_memblk_id = block;
		if (block >= 0) {
			void *allocated = NULL;

			sceKernelGetMemBlockBase(block, &allocated);
			base = allocated;
			self->m_heap_addr = allocated;
		}
		self->m_memory_attached = false;
	}
	if (!base) {
		ScePafStdc_D41FD265();
		ScePafStdc_D608246A();
		base = self->m_heap_addr;
	}
	length = self->m_heap_size;
	if (length & 15) {
		length -= length & 15;
		self->m_heap_size = length;
	}
	alignment = option ? option->alignment : 0;
	self->m_heap_id = create_mspace_with_base(base, length, 0, alignment);
	return self;
}

HeapAllocator *ScePafMisc_F36F1C66(HeapAllocator *self, void *membase,
		size_t size, const char *name, const HeapAllocatorOption *option)
{
	return scePafCreateHeap(self, membase, size, name, option);
}

HeapAllocator *scePafDeleteHeap(HeapAllocator *self)
{
	self->vtable = &heapAllocatorClass.methods;
	destroy_mspace(self->m_heap_id);
	if (self->m_heap_addr && !self->m_memory_attached)
		sceKernelFreeMemBlock(self->m_memblk_id);
	MutexDestroy(&self->m_mutex);
	self->vtable = &memoryAllocatorClass.methods;
	return self;
}

void ScePafMisc_79F13D72(HeapAllocator *self)
{
	scePafDeleteHeap(self);
	ScePafStdc_1B77082E(self);
}

static HeapAllocator *MemoryAllocatorDestruct(HeapAllocator *self)
{
	self->vtable = &memoryAllocatorClass.methods;
	return self;
}

static void MemoryAllocatorDelete(HeapAllocator *self)
{
	self->vtable = &memoryAllocatorClass.methods;
	ScePafStdc_1B77082E(self);
}
