#include "new.h"
#include "stdlib.h"
#ifdef PAF_STATIC_BUILD
#include "../memory/heap_allocator.h"
#endif

/* Only the import declaration is needed; the broad SDK header uses C11. */
extern int sceClibPrintf(const char *fmt, ...);

#ifndef PAF_STATIC_BUILD
/* Sony exports this as __cxa_deleted_virtual (NID 0x25A6D9BB). */
int __cxa_deleted_virtual(void)
{
	int result = sceClibPrintf(
		"[PAF C++ Runtime] __cxa_deleted_virtual() is called\n");
	__breakpoint(0);
	return result;
}
#endif

#if !defined(PAF_STATIC_BUILD) || defined(PAF_BUILD_2014)
int ScePafStdc_D608246A(void)
{
	sce_paf_printf_dummy();
	__breakpoint(0);
	return -1;
}
#endif

void *PafNew(size_t size)
{
	return sce_paf_malloc(size);
}

void *PafNewArray(size_t size)
{
	return sce_paf_malloc(size);
}

void *PafAlignedNew(size_t size, size_t align)
{
	return sce_paf_memalign(align, size);
}

void *PafAlignedNewArray(size_t size, size_t align)
{
	return sce_paf_memalign(align, size);
}

#ifdef PAF_STATIC_BUILD
void *_ZnwjPN3paf6memory13HeapAllocatorE(size_t size, HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMalloc(allocator, size) : NULL;
}

void *_ZnajPN3paf6memory13HeapAllocatorE(size_t size, HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMalloc(allocator, size) : NULL;
}

void *_ZnwjjPN3paf6memory13HeapAllocatorE(size_t size, size_t alignment,
					 HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMemalign(allocator, alignment, size) : NULL;
}

void *_ZnajjPN3paf6memory13HeapAllocatorE(size_t size, size_t alignment,
					 HeapAllocator *allocator)
{
	return allocator ? HeapAllocatorMemalign(allocator, alignment, size) : NULL;
}
#endif

void PafDelete(void *ptr)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafDeleteArray(void *ptr)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafAlignedDelete(void *ptr, size_t size, size_t align)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafAlignedDeleteArray(void *ptr, size_t size, size_t align)
{
	if (ptr)
		sce_paf_free(ptr);
}

#ifdef PAF_STATIC_BUILD
void _ZdlPvPN3paf6memory13HeapAllocatorE(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void _ZdaPvPN3paf6memory13HeapAllocatorE(void *ptr, HeapAllocator *allocator)
{
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void _ZdlPvjjPN3paf6memory13HeapAllocatorE(void *ptr, size_t size, size_t alignment,
					   HeapAllocator *allocator)
{
	(void)size;
	(void)alignment;
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}

void _ZdaPvjjPN3paf6memory13HeapAllocatorE(void *ptr, size_t size, size_t alignment,
					   HeapAllocator *allocator)
{
	(void)size;
	(void)alignment;
	if (allocator && ptr)
		HeapAllocatorFree(allocator, ptr);
}
#else
void *PafPlacementNew(size_t size, void *where)
{
	return where;
}

void __cxa_pure_virtual(void)
{
}
#endif
