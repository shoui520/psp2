/*
 * paf::graph::SurfacePool: the memory the surfaces are made in, with the list of every pool.
 * A pool hands its memory out through a SurfacePoolAllocator, a heap or a range allocator; a
 * pool shared between processes also keeps the offset of each allocation in a block other
 * processes open, so a client pool hands out the host's allocations in the same order.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_POOL_H
#define PAF_GRAPHICS_SURFACE_SURFACE_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include <memory/range_allocator.h>
#include <std/list>
#include <thread/thread.h>

namespace paf {
namespace graph {

class SurfacePoolAllocator {
public:
	SurfacePoolAllocator()
	{
	}

	virtual ~SurfacePoolAllocator()
	{
	}

	virtual void *Allocate(int32_t align_bytes, int32_t size) = 0;
	virtual void Free(void *ptr) = 0;
	virtual size_t GetFreeSize() = 0;
	virtual int32_t CheckMemoryBounds(void *ptr) = 0;
	virtual void DumpFreeChunk() = 0;
	virtual void SetDisableAssertOnAllocFailure(bool disable) = 0;
	virtual bool IsDbgPrintEnabled() = 0;
};

class SurfacePoolAllocator_Heap : public SurfacePoolAllocator {
public:
	SurfacePoolAllocator_Heap(void *addr, size_t size, const char *heap_name,
				  const memory::HeapAllocator::Option *option);
	virtual ~SurfacePoolAllocator_Heap();

	virtual void *Allocate(int32_t align_bytes, int32_t size);
	virtual void Free(void *ptr);
	virtual size_t GetFreeSize();
	virtual int32_t CheckMemoryBounds(void *ptr);
	virtual void DumpFreeChunk();
	virtual void SetDisableAssertOnAllocFailure(bool disable);
	virtual bool IsDbgPrintEnabled();

private:
	memory::HeapAllocator *m_heap;
};

class SurfacePoolAllocator_Range : public SurfacePoolAllocator {
public:
	SurfacePoolAllocator_Range(void *addr, size_t size, const char *range_name,
				   const memory::RangeAllocator::Option *option);
	virtual ~SurfacePoolAllocator_Range();

	virtual void *Allocate(int32_t align_bytes, int32_t size);
	virtual void Free(void *ptr);
	virtual size_t GetFreeSize();
	virtual int32_t CheckMemoryBounds(void *ptr);
	virtual void DumpFreeChunk();
	virtual void SetDisableAssertOnAllocFailure(bool disable);
	virtual bool IsDbgPrintEnabled();

private:
	memory::RangeAllocator *m_range;
};

class SurfacePool {
public:
	enum MemoryType {
		MemoryType_Normal = 0,
		/* video memory other processes may open */
		MemoryType_SharedHost = 1,
		/* a block another process shares */
		MemoryType_SharedClient = 2
	};

	class Option {
	public:
		void *pool_addr;
		int32_t pool_size;
		const char *pool_name;
		const memory::HeapAllocator::Option *heap_allocator_option;
		const memory::RangeAllocator::Option *fast_allocator_option;
		MemoryType type;
	};

	static SurfacePool *FindPool(const char *name);
	static int32_t GetIndex(SurfacePool *pool);
	static SurfacePool *GetPool(int32_t index);

	SurfacePool(void *pool_addr, int32_t pool_size, const char *pool_name,
		    const memory::HeapAllocator::Option *option = NULL,
		    MemoryType type = MemoryType_Normal);
	SurfacePool(const Option *option);
	virtual ~SurfacePool();
	virtual void *Allocate(int32_t align_bytes, int32_t size);
	virtual void Free(void *ptr);
	virtual size_t GetFreeSize() const;

	void SetDisableAssertOnAllocFailure(bool disable);

	SurfacePoolAllocator *m_allocator;
	void *m_addr;
	size_t m_size;
	char *m_name;
	MemoryType m_type;
	/* the block of allocation offsets a shared pool keeps, and its address */
	SceUID m_shared_uid;
	int32_t *m_shared_addr;
	size_t m_count;
	size_t m_max_usage;
	thread::Mutex m_mutex;

	static list<SurfacePool *> m_pool_list;
	static thread::Mutex m_list_mutex;
};

}
}

/* whether the pool's allocator prints its messages (Sony's name not found) */
extern "C" int32_t ScePafGraphics_404AC3D7(paf::graph::SurfacePool *pool);

#endif
