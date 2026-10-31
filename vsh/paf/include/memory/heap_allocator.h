/*
 * paf::memory::HeapAllocator: a dlmalloc mspace over a memory block, its own or
 * one the caller attaches, with every call made under the heap's mutex. Blocks
 * are tagged with the allocator that owns them, and a free or realloc first
 * checks the block's tag and padding.
 */
#ifndef PAF_MEMORY_HEAP_ALLOCATOR_H
#define PAF_MEMORY_HEAP_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <thread/thread.h>
#include <memory/allocator.h>

namespace paf {
namespace memory {

class HeapInfo {
public:
	int32_t num_of_free;
	size_t total_free_size;
};

class HeapAllocator : public MemoryAllocator {
public:
	class Option {
	public:
		uint32_t entry_max;
		size_t warning_size;
		bool disable_dbgprint;
		bool disable_assert_on_alloc_failure;
		AllocationBlockType memblk_type;
		uint32_t alignment;
	};

	HeapAllocator(void *membase, size_t size, const char *name, const Option *option = NULL);
	virtual ~HeapAllocator();

	virtual uint32_t GetType() const;
	virtual AllocationBlockType GetBlockType() const;
	virtual const char *GetName() const;
	virtual void GetRange(void **membase, size_t *size) const;
	virtual size_t GetFreeSize();

	void QueryInfo(HeapInfo *info);
	int CheckMemoryBounds(void *ptr);

	void *sce_paf_malloc(size_t size);
	void *sce_paf_malloc_d(size_t size, const char *filename, int lineno, int type);
	void sce_paf_free(void *ptr);
	void sce_paf_free_d(void *ptr, const char *filename, int lineno, int type);
	void *sce_paf_realloc(void *ptr, size_t size);
	void *sce_paf_realloc_d(void *ptr, size_t size, const char *filename, int lineno, int type);
	void *sce_paf_calloc(size_t nitems, size_t size);
	void *sce_paf_calloc_d(size_t nitems, size_t size, const char *filename, int lineno,
			       int type);
	void *sce_paf_memalign(size_t alignment, size_t size);
	void *sce_paf_memalign_d(size_t alignment, size_t size, const char *filename, int lineno,
				 int type);
	void sce_paf_mtrim();
	size_t sce_paf_musable_size(void *ptr);
	void sce_paf_dump_heap_info(int sort_policy);
	void sce_paf_dump_heap_status();
	int sce_paf_report_memoryblocks(int (*callback)(void *, size_t, int, void *), void *userdata);
	int sce_paf_check_memorybounds();
	int sce_paf_check_memoryleaks(void *start, void *end);

	/* A failed allocation returns NULL without the assert (a name of ours). */
	void SetDisableAssertOnAllocFailure(bool disable)
	{
		m_heap_disable_assert_on_alloc_failure = disable;
	}

private:
	void *m_heap_id;
	void *m_heap_addr;
	size_t m_heap_size;
	char m_heap_name[32];
	bool m_memory_attached;
	bool m_heap_disable_dbgprint;
	bool m_heap_disable_assert_on_alloc_failure;
	thread::Mutex m_mutex;
	SceUID m_memblk_id;
	AllocationBlockType m_heap_memblk_type;
};

}
}

#endif
