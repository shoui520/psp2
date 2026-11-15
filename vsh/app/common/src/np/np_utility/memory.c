/*
 * SceNpUtility: the services' heaps. Each service allocates from its own heap: an mspace on
 * memory the caller gives (sceNpCommunity's), one on a memory block of its own, the caller's
 * allocator, or SceNpCommon's heap. A heap counts its live allocations and the bytes it ever
 * handed out.
 */

#include "np_utility_internal.h"

/* the heap of no service: nothing is allocated from it */
#define NP_LOOKUP_HEAP_ID_NONE	4

static NpLookupHeap s_heaps[NP_LOOKUP_SERVICE_NUM];

/* the heap a service allocates from */
static inline unsigned int heapId(int service)
{
	if (service == NP_LOOKUP_SERVICE_INT)
		return NP_LOOKUP_SERVICE_INT;
	if (service == NP_LOOKUP_SERVICE_LOOKUP)
		return NP_LOOKUP_SERVICE_LOOKUP;
	if (service == NP_LOOKUP_SERVICE_CENSOR)
		return NP_LOOKUP_SERVICE_CENSOR;
	return NP_LOOKUP_HEAP_ID_NONE;
}

/* forgets the heap's memory, leaving its type */
static inline void clearHeap(NpLookupHeap *heap)
{
	if (heap == NULL)
		return;
	heap->memblock = 0;
	heap->mspace = NULL;
	heap->malloc = NULL;
	heap->free = NULL;
	heap->count = 0;
	heap->total = 0;
}

static __attribute__((noinline)) void *heapMalloc(unsigned int id, SceSize size)
{
	NpLookupHeap *heap;
	struct MspaceStats stats;
	void *ptr;

	if (id >= NP_LOOKUP_SERVICE_NUM || size == 0)
		return NULL;
	heap = &s_heaps[id];

	switch (heap->type) {
	case NP_LOOKUP_HEAP_MSPACE:
	case NP_LOOKUP_HEAP_MEMBLOCK:
		if (heap->mspace == NULL)
			return NULL;
		ptr = sceClibMspaceMalloc(heap->mspace, size);
		if (ptr == NULL) {
			sceClibMspaceMallocStats(heap->mspace, &stats);
			return NULL;
		}
		break;
	case NP_LOOKUP_HEAP_CALLBACK:
		if (heap->malloc == NULL)
			return NULL;
		ptr = heap->malloc(size);
		if (ptr == NULL)
			return NULL;
		break;
	case NP_LOOKUP_HEAP_NP_COMMON:
		ptr = sceNpCommonMallocNpServerName(size);
		break;
	default:
		return NULL;
	}
	heap->count++;
	heap->total += size;
	return ptr;
}

/* size bytes from the service's heap */
void *SceNpUtility_8CA24D19(SceSize size, int service)
{
	return heapMalloc(heapId(service), size);
}

static __attribute__((noinline)) void heapFree(unsigned int id, void *ptr)
{
	NpLookupHeap *heap;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return;
	heap = &s_heaps[id];

	switch (heap->type) {
	case NP_LOOKUP_HEAP_MSPACE:
	case NP_LOOKUP_HEAP_MEMBLOCK:
		heap->count--;
		if (heap->mspace != NULL)
			sceClibMspaceFree(heap->mspace, ptr);
		break;
	case NP_LOOKUP_HEAP_CALLBACK:
		heap->count--;
		if (heap->free != NULL)
			heap->free(ptr);
		break;
	case NP_LOOKUP_HEAP_NP_COMMON:
		sceNpCommonFreeNpServerName(ptr);
		heap->count--;
		break;
	}
}

void SceNpUtility_6C1161AD(void *ptr, int service)
{
	heapFree(heapId(service), ptr);
}

/* a heap on a 16 KiB memory block of its own, for the lookup and the censor */
static __attribute__((noinline)) int heapInitMemblock(unsigned int id)
{
	NpLookupHeap *heap;
	const char *name;
	void *base;
	int ret;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	heap = &s_heaps[id];
	clearHeap(heap);
	heap->type = NP_LOOKUP_HEAP_MEMBLOCK;

	switch (id) {
	case NP_LOOKUP_SERVICE_LOOKUP:
		name = "SceNpLookup";
		break;
	case NP_LOOKUP_SERVICE_CENSOR:
		name = "SceNpCensor";
		break;
	default:
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	}

	base = NULL;
	ret = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 0x4000, NULL);
	if (ret < 0)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	heap->memblock = ret;
	sceKernelGetMemBlockBase(ret, &base);
	heap->mspace = sceClibMspaceCreate(base, 0x4000);
	if (heap->mspace == NULL) {
		if (heap->memblock != 0) {
			sceKernelFreeMemBlock(heap->memblock);
			heap->memblock = 0;
		}
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	}
	return 0;
}

/* destroys the heap's mspace */
static inline void destroyMspace(NpLookupHeap *heap)
{
	struct MspaceStats stats;

	if (heap->mspace != NULL) {
		sceClibMspaceMallocStats(heap->mspace, &stats);
		sceClibMspaceDestroy(heap->mspace);
		heap->count = 0;
	}
	heap->mspace = NULL;
}

static __attribute__((noinline)) void heapTerm(unsigned int id)
{
	NpLookupHeap *heap;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return;
	heap = &s_heaps[id];

	switch (heap->type) {
	case NP_LOOKUP_HEAP_MSPACE:
		destroyMspace(heap);
		break;
	case NP_LOOKUP_HEAP_MEMBLOCK:
		destroyMspace(heap);
		if (heap->memblock != 0) {
			sceKernelFreeMemBlock(heap->memblock);
			heap->memblock = 0;
		}
		break;
	case NP_LOOKUP_HEAP_CALLBACK:
		heap->malloc = NULL;
		heap->free = NULL;
		heap->count = 0;
		break;
	}
}

/* the service allocates from an mspace on the caller's memory */
void SceNpUtility_B5711AF9(int service, SceSize size, void *memblock)
{
	unsigned int id = heapId(service);
	NpLookupHeap *heap;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return;
	heap = &s_heaps[id];
	clearHeap(heap);
	heap->type = NP_LOOKUP_HEAP_MSPACE;
	heap->mspace = sceClibMspaceCreate(memblock, size);
}

/* the service allocates from a memory block of its own */
int SceNpUtility_74B0D0AC(int service)
{
	return heapInitMemblock(heapId(service));
}

/* the service allocates with the caller's allocator */
void SceNpUtility_4A3FDD3B(int service, void *(*alloc)(SceSize size), void (*free)(void *ptr))
{
	unsigned int id = heapId(service);
	NpLookupHeap *heap;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return;
	heap = &s_heaps[id];
	clearHeap(heap);
	heap->malloc = alloc;
	heap->type = NP_LOOKUP_HEAP_CALLBACK;
	heap->free = free;
}

/* the service allocates from SceNpCommon's heap */
void SceNpUtility_5FC4B7D5(int service)
{
	unsigned int id = heapId(service);
	NpLookupHeap *heap;

	if (id >= NP_LOOKUP_SERVICE_NUM)
		return;
	heap = &s_heaps[id];
	clearHeap(heap);
	heap->type = NP_LOOKUP_HEAP_NP_COMMON;
}

void SceNpUtility_418E1324(int service)
{
	heapTerm(heapId(service));
}
