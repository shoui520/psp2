/*
 * paf::memory::PoolAllocator: equal blocks carved from a memory block, its own
 * or one the caller attaches, kept on a free list that runs through the free
 * blocks themselves.
 */
#ifndef PAF_MEMORY_POOL_ALLOCATOR_H
#define PAF_MEMORY_POOL_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <thread/thread.h>
#include <memory/allocator.h>

namespace paf {
namespace memory {

class PoolInfo {
public:
	size_t block_size;
	uint32_t num_blocks;
	uint32_t num_free;
};

class PoolAllocator : public MemoryAllocator {
public:
	class Option {
	public:
		bool disable_dbgprint;
		bool disable_assert_on_alloc_failure;
		AllocationBlockType memblk_type;
	};

	PoolAllocator(void *membase, size_t size, size_t blockSize, const char *name,
		      const Option *option = NULL);
	virtual ~PoolAllocator();

	virtual uint32_t GetType() const;
	virtual AllocationBlockType GetBlockType() const;
	virtual const char *GetName() const;
	virtual void GetRange(void **membase, size_t *size) const;
	virtual size_t GetFreeSize();

	void *Alloc();
	void Free(void *ptr);
	void QueryInfo(PoolInfo *info);
	void DumpInfo(int sort_policy);

private:
	/* A free block's first word links it to the next free block. */
	struct FreeBlock {
		FreeBlock *next;
	};

	void *m_pool_addr;
	size_t m_pool_size;
	size_t m_block_size;
	char m_pool_name[32];
	bool m_memory_attached;
	bool m_pool_disable_dbgprint;
	bool m_pool_disable_assert_on_alloc_failure;
	uint32_t m_num_blocks;
	uint32_t m_num_free;
	FreeBlock *m_free_list;
	thread::Mutex m_mutex;
	AllocationBlockType m_pool_memblk_type;
	SceUID m_memblk_id;
};

}
}

#endif
