/*
 * paf::memory::MemoryAllocator: the interface of paf's allocators. GetType()
 * tells them apart: 0 HeapAllocator, 1 RangeAllocator, 2 PoolAllocator.
 */
#ifndef PAF_MEMORY_ALLOCATOR_H
#define PAF_MEMORY_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace memory {

enum AllocationBlockType {
	AllocationBlockType_User = 0,
	AllocationBlockType_User_Cdlg = 1
};

class MemoryAllocator {
public:
	MemoryAllocator()
	{
	}

	virtual uint32_t GetType() const = 0;
	virtual AllocationBlockType GetBlockType() const = 0;
	virtual const char *GetName() const = 0;
	virtual void GetRange(void **membase, size_t *size) const = 0;
	virtual size_t GetFreeSize() = 0;

	virtual ~MemoryAllocator()
	{
	}
};

}
}

#endif
