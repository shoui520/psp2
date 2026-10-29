#ifndef PAF_STD_GLOBAL_HEAP_H
#define PAF_STD_GLOBAL_HEAP_H

#include "../memory/heap_allocator.h"

void *ScePafStdc_D00387E7(size_t size, HeapAllocator *allocator);
void *ScePafStdc_3CF19248(size_t size, HeapAllocator *allocator);
void *ScePafStdc_5B828DF7(size_t size, size_t alignment,
			  HeapAllocator *allocator);
void *ScePafStdc_6FE032F8(size_t size, size_t alignment,
			  HeapAllocator *allocator);

HeapAllocator *GlobalHeapConstruct(HeapAllocator *self);
HeapAllocator *GlobalHeapDestruct(HeapAllocator *self);
int GlobalHeapInitialize(void);

#endif
