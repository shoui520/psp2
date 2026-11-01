/*
 * paf::graph::SurfacePool, as far as converted C++ uses it: the memory the
 * surfaces are made in.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_POOL_H
#define PAF_GRAPHICS_SURFACE_SURFACE_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>

namespace paf {
namespace graph {

class SurfacePoolAllocator;

class SurfacePool {
public:
	enum MemoryType {
		MemoryType_Normal = 0,
		/* video memory other processes may open */
		MemoryType_SharedHost = 1,
		/* a block another process shares */
		MemoryType_SharedClient = 2
	};

	SurfacePool(void *pool_addr, int32_t pool_size, const char *pool_name,
		    const memory::HeapAllocator::Option *option = NULL,
		    MemoryType type = MemoryType_Normal);
	virtual ~SurfacePool();

	SurfacePoolAllocator *m_allocator;
	void *m_addr;
	size_t m_size;
	char *m_name;
	MemoryType m_type;
	SceUID m_shared_uid;
	void *m_shared_addr;
	size_t m_count;
	size_t m_max_usage;
	thread::Mutex m_mutex;
};

}
}

#endif
