#ifndef PAF_STD_GLOBAL_HEAP_H
#define PAF_STD_GLOBAL_HEAP_H

#include "../memory/heap_allocator.h"

HeapAllocator *GlobalHeapConstruct(HeapAllocator *self);
HeapAllocator *GlobalHeapDestruct(HeapAllocator *self);
int GlobalHeapInitialize(void);

#endif
