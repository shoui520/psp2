/*
 * paf::memory::RangeAllocator (a name of ours: no record or export names the
 * class): hands out aligned address ranges of a memory block, its own or one
 * the caller attaches, without touching the memory. The ranges are kept as
 * chunks on the paf heap, in address order, each chunk running up to the next
 * one's address; the free chunks are also linked on a free list.
 */
#ifndef PAF_MEMORY_RANGE_ALLOCATOR_H
#define PAF_MEMORY_RANGE_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <thread/thread.h>
#include <memory/allocator.h>
#include <std/stdlib.h>

namespace paf {
namespace memory {

class RangeAllocator : public MemoryAllocator {
public:
	class Option {
	public:
		bool disable_dbgprint;
		bool disable_assert_on_alloc_failure;
		AllocationBlockType memblk_type;
	};

	RangeAllocator(void *membase, size_t size, const char *name, const Option *option = NULL);
	virtual ~RangeAllocator();

	virtual uint32_t GetType() const;
	virtual AllocationBlockType GetBlockType() const;
	virtual const char *GetName() const;
	virtual void GetRange(void **membase, size_t *size) const;
	virtual size_t GetFreeSize();

	uintptr_t Alloc(int alignment, size_t size);
	void Free(uintptr_t addr);
	void QueryFreeSize(size_t *size);
	void DumpInfo(int sort_policy);
	int CheckMemoryLeaks();

private:
	enum {
		CHUNK_USED = 1
	};

	/* A chunk's address, with CHUNK_USED in bit 0 while it is handed out. */
	struct Chunk {
		static void operator delete(void *ptr)
		{
			sce_paf_free(ptr);
		}

		Chunk *next;
		Chunk *next_free;
		uintptr_t addr;
	};

	void *m_range_addr;
	size_t m_range_size;
	char m_range_name[32];
	size_t m_free_size;
	bool m_memory_attached;
	bool m_range_disable_dbgprint;
	bool m_range_disable_assert_on_alloc_failure;
	thread::Mutex m_mutex;
	Chunk *m_chunks;
	Chunk *m_free_chunks;
	AllocationBlockType m_range_memblk_type;
	SceUID m_memblk_id;
};

}
}

#endif
