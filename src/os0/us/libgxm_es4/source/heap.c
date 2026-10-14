/*
 * SceGxm: a range allocator.
 *
 * Free ranges are kept sorted by base and merged with their neighbours;
 * the nodes describing ranges come from the owner's allocator.
 */

#include "gxm_internal.h"

void gxmHeapInit(GxmHeap *heap, void *userData, GxmHeapNodeAlloc *allocNode,
		 GxmHeapNodeFree *freeNode)
{
	heap->userData = userData;
	heap->allocNode = allocNode;
	heap->freeNode = freeNode;
	heap->used = NULL;
	heap->free = NULL;
}

/* put a range on the free list, merging it with the ranges either side */
static inline void gxmHeapInsertFree(GxmHeap *heap, GxmHeapNode *node, SceUInt32 base)
{
	GxmHeapNode *prev, *next;

	next = heap->free;
	if (next == NULL || base <= next->base) {
		heap->free = node;
		prev = NULL;
	} else {
		do {
			prev = next;
			next = prev->next;
		} while (next != NULL && next->base < base);
		prev->next = node;
	}
	node->next = next;
	if (next != NULL && base + node->size == next->base) {
		node->size = next->size + node->size;
		node->next = next->next;
		heap->freeNode(heap->userData, next);
	}
	if (prev != NULL && base == prev->size + prev->base) {
		prev->size = node->size + prev->size;
		prev->next = node->next;
		heap->freeNode(heap->userData, node);
	}
}

int gxmHeapAddRange(GxmHeap *heap, SceUInt32 base, SceSize size)
{
	GxmHeapNode *node = heap->allocNode(heap->userData);

	if (node == NULL)
		return -1;
	node->base = base;
	node->size = size;
	node->next = NULL;
	gxmHeapInsertFree(heap, node, base);
	return 0;
}

void gxmHeapClear(GxmHeap *heap)
{
	GxmHeapNode *node, *next;

	for (node = heap->used; node != NULL; node = next) {
		next = node->next;
		heap->freeNode(heap->userData, node);
	}
	heap->used = NULL;
	for (node = heap->free; node != NULL; node = next) {
		next = node->next;
		heap->freeNode(heap->userData, node);
	}
	heap->free = NULL;
}

/* the base of an aligned range of size bytes, or 0 */
SceUInt32 gxmHeapAlloc(GxmHeap *heap, SceSize size, SceSize alignment)
{
	GxmHeapNode *node, *prev, *before, *after;
	SceUInt32 padding, end;

	node = heap->free;
	if (node == NULL)
		return 0;
	prev = NULL;
	for (;;) {
		padding = ((node->base - 1 + alignment) & -alignment) - node->base;
		end = padding + size;
		if (end <= node->size)
			break;
		prev = node;
		node = node->next;
		if (node == NULL)
			return 0;
	}

	before = NULL;
	if (padding != 0) {
		before = heap->allocNode(heap->userData);
		if (before == NULL)
			return 0;
	}
	after = NULL;
	if (end != node->size) {
		after = heap->allocNode(heap->userData);
		if (after == NULL) {
			if (before != NULL)
				heap->freeNode(heap->userData, before);
			return 0;
		}
	}

	/* the alignment padding stays free in front */
	if (padding != 0) {
		if (prev != NULL)
			prev->next = before;
		else
			heap->free = before;
		before->next = node;
		before->base = node->base;
		before->size = padding;
		node->size -= padding;
		node->base += padding;
		prev = before;
	}
	/* and so does the rest behind */
	if (size != node->size) {
		after->next = node->next;
		node->next = after;
		after->base = size + node->base;
		after->size = node->size - size;
		node->size = size;
	}
	if (prev != NULL)
		prev->next = node->next;
	else
		heap->free = node->next;
	node->next = NULL;
	node->next = heap->used;
	heap->used = node;
	return node->base;
}

void gxmHeapFree(GxmHeap *heap, SceUInt32 base)
{
	GxmHeapNode *node, *prev;

	node = heap->used;
	if (node == NULL)
		return;
	if (base == node->base) {
		heap->used = node->next;
	} else {
		do {
			prev = node;
			node = prev->next;
			if (node == NULL)
				return;
		} while (base != node->base);
		prev->next = node->next;
	}
	node->next = NULL;
	gxmHeapInsertFree(heap, node, node->base);
}

#define GXM_HEAP_GENERAL		0
#define GXM_HEAP_FRAGMENT_USSE		4
#define GXM_HEAP_VERTEX_USSE		5
#define GXM_HEAP_SLOTS			8
#define GXM_HEAP_14			14
#define GXM_HEAP_FRAGMENT_USSE_INTERNAL	18
#define GXM_HEAP_VERTEX_USSE_INTERNAL	19

/* where the driver put the heaps the library maps into */
int gxmQueryHeaps(void)
{
	PVRSRV_HEAP_INFO heaps[32];
	SceUInt32 count, i;

	if (SceGpuEs4User_1E0A9C4A(&gxmGlobals.top, &gxmGlobals.top.memContext, &count, heaps) != 0)
		return SCE_GXM_ERROR_DRIVER;
	for (i = 0; i < count; i++) {
		switch (heaps[i].ui32HeapID & 0xffffff) {
		case GXM_HEAP_GENERAL:
			gxmGlobals.top.generalHeap = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_FRAGMENT_USSE:
			gxmGlobals.top.usse[0].heap[1] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[0].base[1] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_VERTEX_USSE:
			gxmGlobals.top.usse[0].heap[0] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[0].base[0] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_SLOTS:
			gxmGlobals.top.slotHeap = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_14:
			gxmGlobals.top.heap14 = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_FRAGMENT_USSE_INTERNAL:
			gxmGlobals.top.usse[1].heap[1] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[1].base[1] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_VERTEX_USSE_INTERNAL:
			gxmGlobals.top.usse[1].heap[0] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[1].base[0] = heaps[i].sDevVAddrBase;
			break;
		}
	}
	if (gxmGlobals.top.generalHeap == 0 || gxmGlobals.top.usse[0].heap[0] == 0 ||
	    gxmGlobals.top.usse[0].heap[1] == 0 || gxmGlobals.top.slotHeap == 0 ||
	    gxmGlobals.top.usse[1].heap[0] == 0 || gxmGlobals.top.usse[1].heap[1] == 0 ||
	    gxmGlobals.top.heap14 == 0)
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	return 0;
}
