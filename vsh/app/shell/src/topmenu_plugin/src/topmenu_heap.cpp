/*
 * topmenu_heap.cpp: topmenu_plugin heap allocator binding (0x8128430C-0x81284397).
 */
#include "topmenu_internal.h"

paf::memory::HeapAllocator *g_topmenuHeap;

/* 0x8128430C */
void topmenuHeapInit(void)
{
	g_topmenuHeap = g_topmenuPlugin->m_private_heap;
}

/* 0x81284326 */
void topmenuHeapTerm(void)
{
}
