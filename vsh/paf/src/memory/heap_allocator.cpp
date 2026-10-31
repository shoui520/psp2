#include <memory/heap_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>
#include "dlmalloc.h"

namespace paf {
namespace memory {

uint32_t HeapAllocator::GetType() const
{
	return 0;
}

AllocationBlockType HeapAllocator::GetBlockType() const
{
	return m_heap_memblk_type;
}

const char *HeapAllocator::GetName() const
{
	return m_heap_name;
}

void HeapAllocator::GetRange(void **membase, size_t *size) const
{
	*membase = m_heap_addr;
	*size = m_heap_size;
}

size_t HeapAllocator::GetFreeSize()
{
	m_mutex.Lock();

	struct mallinfo info = mspace_mallinfo(m_heap_id);

	m_mutex.Unlock();
	return info.fordblks;
}

void HeapAllocator::QueryInfo(HeapInfo *info)
{
	m_mutex.Lock();

	struct mallinfo minfo = mspace_mallinfo(m_heap_id);

	m_mutex.Unlock();
	info->num_of_free = minfo.ordblks;
	info->total_free_size = minfo.fordblks;
}

int HeapAllocator::CheckMemoryBounds(void *ptr)
{
	return mspace_check_memorybounds(m_heap_id, ptr, (unsigned int)this);
}

void *HeapAllocator::sce_paf_malloc(size_t size)
{
	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	m_mutex.Lock();

	void *ptr = mspace_malloc(m_heap_id, size, (unsigned int)this);

	if (ptr == NULL && !m_heap_disable_dbgprint)
		sce_paf_printf_dummy();
	m_mutex.Unlock();
	return ptr;
}

void *HeapAllocator::sce_paf_malloc_d(size_t size, const char *filename, int lineno, int type)
{
	return sce_paf_malloc(size);
}

void HeapAllocator::sce_paf_free(void *ptr)
{
	m_mutex.Lock();
	if (mspace_check_memorybounds(m_heap_id, ptr, (unsigned int)this) != 0)
		sce_paf_printf_dummy();
	mspace_free(m_heap_id, ptr, (unsigned int)this);
	m_mutex.Unlock();
}

void HeapAllocator::sce_paf_free_d(void *ptr, const char *filename, int lineno, int type)
{
	sce_paf_free(ptr);
}

void *HeapAllocator::sce_paf_realloc(void *ptr, size_t size)
{
	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	m_mutex.Lock();
	if (mspace_check_memorybounds(m_heap_id, ptr, (unsigned int)this) != 0)
		sce_paf_printf_dummy();

	void *result = mspace_realloc(m_heap_id, ptr, size, (unsigned int)this);

	if (result == NULL && !m_heap_disable_dbgprint)
		sce_paf_printf_dummy();
	m_mutex.Unlock();
	return result;
}

void *HeapAllocator::sce_paf_realloc_d(void *ptr, size_t size, const char *filename, int lineno,
				       int type)
{
	return sce_paf_realloc(ptr, size);
}

void *HeapAllocator::sce_paf_calloc(size_t nitems, size_t size)
{
	size_t total = nitems * size;

	if (total > 0x1fffffff)
		ScePafStdc_D608246A();
	m_mutex.Lock();

	void *ptr = mspace_malloc(m_heap_id, total, (unsigned int)this);

	if (ptr != NULL)
		sce_paf_memset(ptr, 0, total);
	if (ptr == NULL && !m_heap_disable_dbgprint)
		sce_paf_printf_dummy();
	m_mutex.Unlock();
	return ptr;
}

void *HeapAllocator::sce_paf_calloc_d(size_t nitems, size_t size, const char *filename,
				      int lineno, int type)
{
	return sce_paf_calloc(nitems, size);
}

void *HeapAllocator::sce_paf_memalign(size_t alignment, size_t size)
{
	if (size > 0x1fffffff)
		ScePafStdc_D608246A();
	m_mutex.Lock();

	void *ptr = mspace_memalign(m_heap_id, alignment, size, (unsigned int)this);

	if (ptr == NULL && !m_heap_disable_dbgprint)
		sce_paf_printf_dummy();
	m_mutex.Unlock();
	return ptr;
}

void *HeapAllocator::sce_paf_memalign_d(size_t alignment, size_t size, const char *filename,
					int lineno, int type)
{
	return sce_paf_memalign(alignment, size);
}

void HeapAllocator::sce_paf_mtrim()
{
	m_mutex.Lock();
	mspace_trim(m_heap_id, 0);
	m_mutex.Unlock();
}

size_t HeapAllocator::sce_paf_musable_size(void *ptr)
{
	m_mutex.Lock();

	size_t size = mspace_usable_size(ptr);

	m_mutex.Unlock();
	return size;
}

/* The release build prints nothing: the heap's state is read and dropped. */
void HeapAllocator::sce_paf_dump_heap_info(int sort_policy)
{
	m_mutex.Lock();
	mspace_mallinfo(m_heap_id);
	m_mutex.Unlock();
	sce_paf_printf_dummy();
}

void HeapAllocator::sce_paf_dump_heap_status()
{
	m_mutex.Lock();
	mspace_mallinfo(m_heap_id);
	m_mutex.Unlock();
	sce_paf_printf_dummy();
}

int HeapAllocator::sce_paf_report_memoryblocks(int (*callback)(void *, size_t, int, void *),
					       void *userdata)
{
	m_mutex.Lock();

	int count = mspace_report_free_blocks(m_heap_id, callback, userdata);

	m_mutex.Unlock();
	return count;
}

int HeapAllocator::sce_paf_check_memorybounds()
{
	m_mutex.Lock();
	m_mutex.Unlock();
	return 0;
}

int HeapAllocator::sce_paf_check_memoryleaks(void *start, void *end)
{
	return 0;
}

HeapAllocator::HeapAllocator(void *membase, size_t size, const char *name, const Option *option)
	: m_heap_addr(membase), m_heap_size(size), m_memory_attached(false),
	  m_heap_disable_dbgprint(false), m_heap_disable_assert_on_alloc_failure(true),
	  m_mutex(name, 0)
{
	SceKernelMemBlockType type;

	m_heap_memblk_type = AllocationBlockType_User;
	if (option != NULL && option->memblk_type != AllocationBlockType_User)
		m_heap_memblk_type = option->memblk_type;
	switch (m_heap_memblk_type) {
	case AllocationBlockType_User:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
		break;
	case AllocationBlockType_User_Cdlg:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
		break;
	default:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
		break;
	}
	if (option != NULL) {
		if (option->disable_dbgprint)
			m_heap_disable_dbgprint = true;
		if (!option->disable_assert_on_alloc_failure)
			m_heap_disable_assert_on_alloc_failure = false;
	}
	sce_paf_strncpy(m_heap_name, name, sizeof(m_heap_name) - 1);
	m_heap_name[sizeof(m_heap_name) - 1] = '\0';
	if (m_heap_addr != NULL) {
		m_memory_attached = true;
	} else {
		size_t padding = 0;

		if (m_heap_size & 4095)
			padding = 4096 - (m_heap_size & 4095);
		m_memblk_id = sceKernelAllocMemBlock(m_heap_name, type, m_heap_size + padding, NULL);
		if (m_memblk_id >= 0) {
			void *base = NULL;

			sceKernelGetMemBlockBase(m_memblk_id, &base);
			m_heap_addr = base;
		}
		m_memory_attached = false;
	}
	if (m_heap_addr == NULL) {
		sce_paf_printf_dummy();
		ScePafStdc_D608246A();
	}
	if (m_heap_size & 15)
		m_heap_size -= m_heap_size & 15;
	m_heap_id = create_mspace_with_base(m_heap_addr, m_heap_size, 0,
					    option != NULL ? option->alignment : 0);
}

HeapAllocator::~HeapAllocator()
{
	destroy_mspace(m_heap_id);
	if (m_heap_addr != NULL && !m_memory_attached)
		sceKernelFreeMemBlock(m_memblk_id);
}

}
}
