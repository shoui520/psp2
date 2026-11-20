/*
 * np_friend_storage_heap.cpp: the heap the NP friend storage allocates from (0x810FB33C-0x810FB388),
 * set while the storage is open. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include "np_friend_storage.h"

using namespace paf;

/* (0x8155965C) */
static memory::HeapAllocator *s_heap;

/* (0x81559660) the storage's strings' empty string */
template<> char NpFriendString::empty_string = '\0';

/* (0x810FB33C) */
void npFriendStorageSetHeap(memory::HeapAllocator *heap)
{
	s_heap = heap;
}

/* (0x810FB348) */
void npFriendStorageClearHeap(void)
{
	s_heap = NULL;
}

/* (0x810FB356) */
void *npFriendStorageMalloc(SceSize size)
{
	if (s_heap == NULL)
		return NULL;
	return s_heap->sce_paf_malloc(size);
}

/* (0x810FB372) */
void npFriendStorageFree(void *ptr)
{
	if (s_heap != NULL)
		s_heap->sce_paf_free(ptr);
}
