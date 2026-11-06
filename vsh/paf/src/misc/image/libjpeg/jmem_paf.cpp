/*
 * The paf heap allocations Sony's jmemnobs makes (names ours; see jpeg_paf.h).
 */
#include <stddef.h>
#include <std/stdlib.h>
#include <memory/heap_allocator.h>
#include <stdio.h>
#include <jpeglib.h>
#include "jpeg_paf.h"

using paf::memory::HeapAllocator;

void *jpeg_paf_malloc(size_t size, void *heap)
{
	if (heap != NULL)
		return ((HeapAllocator *)heap)->sce_paf_malloc(size);
	else
		return sce_paf_malloc(size);
}

void *jpeg_paf_memalign(size_t align, size_t size, void *heap)
{
	if (heap != NULL)
		return ((HeapAllocator *)heap)->sce_paf_memalign(align, size);
	else
		return sce_paf_memalign(align, size);
}

void jpeg_paf_free(void *ptr, void *heap)
{
	if (heap != NULL)
		((HeapAllocator *)heap)->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}
