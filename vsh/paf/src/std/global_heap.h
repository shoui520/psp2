#ifndef PAF_STD_GLOBAL_HEAP_H
#define PAF_STD_GLOBAL_HEAP_H

#include "../memory/heap_allocator.h"

/* operator new(size_t, paf::memory::HeapAllocator *): NULL where there is no heap */
void *_ZnwjPN3paf6memory13HeapAllocatorE(size_t size, HeapAllocator *allocator);
void *ScePafStdc_3CF19248(size_t size, HeapAllocator *allocator);
void *ScePafStdc_5B828DF7(size_t size, size_t alignment,
			  HeapAllocator *allocator);
void *ScePafStdc_6FE032F8(size_t size, size_t alignment,
			  HeapAllocator *allocator);
void ScePafStdc_D04CBFC0(void *ptr, HeapAllocator *allocator);
void ScePafStdc_BE896EAF(void *ptr, HeapAllocator *allocator);
void ScePafStdc_46A0F0D9(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator);
void ScePafStdc_F22F2F50(void *ptr, size_t unused1, size_t unused2,
			 HeapAllocator *allocator);

HeapAllocator *GlobalHeapConstruct(HeapAllocator *self);
HeapAllocator *GlobalHeapDestruct(HeapAllocator *self);
int GlobalHeapInitialize(void);

#endif
