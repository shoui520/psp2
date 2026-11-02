/*
 * The allocators a SurfacePool hands its memory out through: a heap (HeapAllocator) or a range
 * allocator (RangeAllocator) over the pool's memory (the file's name ours: placed after
 * surface_pool.cpp, out of line where the pool's constructors call them).
 */
#include <stddef.h>
#include <stdint.h>
#include <common/common_internal.h>
#include <graphics/surface/surface_pool.h>
#include <memory/heap_allocator.h>
#include <memory/range_allocator.h>
#include <std/new>

namespace paf {
namespace graph {

SurfacePoolAllocator_Heap::SurfacePoolAllocator_Heap(void *addr, size_t size,
						     const char *heap_name,
						     const memory::HeapAllocator::Option *option)
	: m_heap(NULL)
{
	if (addr == NULL)
		common_internal::on_error();
	if (size == 0)
		common_internal::on_error();
	m_heap = new memory::HeapAllocator(addr, size, heap_name, option);
	if (m_heap == NULL)
		common_internal::on_error();
}

SurfacePoolAllocator_Heap::~SurfacePoolAllocator_Heap()
{
	if (m_heap != NULL)
		delete m_heap;
}

void *SurfacePoolAllocator_Heap::Allocate(int32_t align_bytes, int32_t size)
{
	return m_heap->sce_paf_memalign(align_bytes, size);
}

void SurfacePoolAllocator_Heap::Free(void *ptr)
{
	m_heap->sce_paf_free(ptr);
}

size_t SurfacePoolAllocator_Heap::GetFreeSize()
{
	return m_heap->GetFreeSize();
}

int32_t SurfacePoolAllocator_Heap::CheckMemoryBounds(void *ptr)
{
	return m_heap->CheckMemoryBounds(ptr);
}

void SurfacePoolAllocator_Heap::DumpFreeChunk()
{
}

void SurfacePoolAllocator_Heap::SetDisableAssertOnAllocFailure(bool disable)
{
	if (m_heap != NULL)
		m_heap->SetDisableAssertOnAllocFailure(disable);
}

bool SurfacePoolAllocator_Heap::IsDbgPrintEnabled()
{
	if (m_heap != NULL)
		return m_heap->IsDbgPrintEnabled();

	return true;
}

SurfacePoolAllocator_Range::SurfacePoolAllocator_Range(void *addr, size_t size,
						       const char *range_name,
						       const memory::RangeAllocator::Option *option)
	: m_range(NULL)
{
	m_range = new memory::RangeAllocator(addr, size, range_name, option);
}

SurfacePoolAllocator_Range::~SurfacePoolAllocator_Range()
{
	if (m_range != NULL)
		delete m_range;
}

void *SurfacePoolAllocator_Range::Allocate(int32_t align_bytes, int32_t size)
{
	return (void *)m_range->Alloc(align_bytes, size);
}

void SurfacePoolAllocator_Range::Free(void *ptr)
{
	m_range->Free((uintptr_t)ptr);
}

size_t SurfacePoolAllocator_Range::GetFreeSize()
{
	return m_range->GetFreeSize();
}

/* a range allocator has no bounds to check: it checks for leaks */
int32_t SurfacePoolAllocator_Range::CheckMemoryBounds(void *ptr)
{
	return m_range->CheckMemoryLeaks();
}

void SurfacePoolAllocator_Range::DumpFreeChunk()
{
}

void SurfacePoolAllocator_Range::SetDisableAssertOnAllocFailure(bool disable)
{
	if (m_range != NULL)
		m_range->SetDisableAssertOnAllocFailure(disable);
}

bool SurfacePoolAllocator_Range::IsDbgPrintEnabled()
{
	if (m_range != NULL)
		return m_range->IsDbgPrintEnabled();

	return true;
}

}
}
