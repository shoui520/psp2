#ifndef PAF_MEMORY_HEAP_ALLOCATOR_H
#define PAF_MEMORY_HEAP_ALLOCATOR_H

#include <stddef.h>

typedef struct HeapAllocator HeapAllocator;

void *scePafReallocWithContext(HeapAllocator *allocator, void *ptr, size_t size);
void scePafFreeWithContext(HeapAllocator *allocator, void *ptr);

#endif
