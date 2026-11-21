/*
 * The library's memory: two heaps (general and secure) in memory the application gives,
 * each behind its lock. A heap's blocks are in address order, the free ones also in a
 * list the rover walks; allocations of up to 64 bytes come from pages of 256 units of
 * 16 bytes, whose map gives each run's length at its first unit.
 */
#include "mln_internal.h"

typedef struct MlnHeapSlot {
	MlnHeap *heap;
	MlnMutex mutex;
} MlnHeapSlot;

static MlnHeapSlot s_heaps[2];

static int MlnHeapContains(const MlnHeap *heap, const void *p)
{
	return (const void *)heap->start <= p && p < (const void *)heap->end;
}

static int MlnHeapBlockIsFree(const MlnHeapBlock *block)
{
	return block->freePrev != NULL || block->freeNext != NULL;
}

/* joins block b, which follows a, to a; the joined block is free if either was */
__attribute__((noinline)) MlnHeapBlock *MlnHeapMerge(MlnHeap *heap, MlnHeapBlock *a, MlnHeapBlock *b)
{
	MlnHeapBlock *freePrev = a->freePrev;
	MlnHeapBlock *freeNext;
	int state;

	state = (MlnHeapBlockIsFree(a) ? 2 : 0) + MlnHeapBlockIsFree(b);
	switch (state) {
	case 2:
		freeNext = a->freeNext;
		break;
	case 3:
		freeNext = b->freeNext;
		break;
	case 1:
		freeNext = b->freeNext;
		freePrev = b->freePrev;
		break;
	default:
		freePrev = NULL;
		freeNext = NULL;
		break;
	}
	a->freePrev = freePrev;
	a->freeNext = freeNext;
	a->next = b->next;
	b->prev = NULL;
	b->next = NULL;
	b->freePrev = NULL;
	b->freeNext = NULL;
	if (MlnHeapContains(heap, a->next))
		a->next->prev = a;
	if (MlnHeapContains(heap, freePrev))
		freePrev->freeNext = a;
	if (MlnHeapContains(heap, freeNext))
		freeNext->freePrev = a;
	if (heap->rover == b)
		heap->rover = a;
	return a;
}

/* a run of units free units on a page, marked used */
static inline __attribute__((always_inline)) void *MlnHeapPageAllocate(MlnHeap *heap, MlnHeapPage *page, int units)
{
	int i, k, run;

	if (units > page->freeUnits)
		return NULL;
	for (i = 0; i <= MLN_HEAP_PAGE_UNITS - units; i++) {
		run = page->map[i];
		if (run != 0) {
			if (run != MLN_HEAP_RUN_CONTINUED)
				i += run - 1;
			continue;
		}
		if (units > 1) {
			for (k = 0; k < units - 1; k++) {
				if (page->map[i + 1 + k] != heap->freeMap[k])
					break;
			}
			if (k != units - 1)
				continue;
		}
		page->map[i] = units;
		for (k = 1; k < units; k++)
			page->map[i + k] = MLN_HEAP_RUN_CONTINUED;
		page->freeUnits -= units;
		return page->units[i];
	}
	return NULL;
}

__attribute__((noinline)) void *MlnHeapAllocate(MlnHeap *heap, unsigned int size)
{
	MlnHeapBlock *block, *next, *rest;
	MlnHeapPage *page;
	unsigned char *bytes;
	int units, blockUnits;
	unsigned int i;
	void *p;

	block = heap->rover;
	if (size == 0)
		return NULL;
	if (size > MLN_HEAP_SMALL_MAX) {
		if (block >= heap->end)
			return NULL;
		units = (size + MLN_HEAP_UNIT - 1) >> 4;
		next = block->next;
		blockUnits = (((char *)next - (char *)block) >> 4) - 1;
		if (units + 1 >= blockUnits) {
			while (units > blockUnits) {
				block = block->freeNext;
				if (block >= heap->end)
					return NULL;
				next = block->next;
				blockUnits = (((char *)next - (char *)block) >> 4) - 1;
				if (blockUnits > units + 1)
					goto split;
			}
			if (MlnHeapContains(heap, block->freePrev))
				block->freePrev->freeNext = block->freeNext;
			if (MlnHeapContains(heap, block->freeNext))
				block->freeNext->freePrev = block->freePrev;
			if (heap->rover == block)
				heap->rover = block->freeNext;
			block->freePrev = NULL;
			block->freeNext = NULL;
			return block + 1;
		}
split:
		rest = block + units + 1;
		rest->prev = block;
		rest->freeNext = block->freeNext;
		rest->next = next;
		rest->freePrev = block->freePrev;
		if (MlnHeapContains(heap, next))
			next->prev = rest;
		if (MlnHeapContains(heap, block->freePrev))
			block->freePrev->freeNext = rest;
		if (MlnHeapContains(heap, block->freeNext))
			block->freeNext->freePrev = rest;
		if (heap->rover == block)
			heap->rover = rest;
		block->next = rest;
		block->freePrev = NULL;
		block->freeNext = NULL;
		return block + 1;
	}

	units = (int)(size + MLN_HEAP_UNIT - 1) >> 4;
	for (page = heap->pages; page != NULL; page = page->next) {
		p = MlnHeapPageAllocate(heap, page, units);
		if (p != NULL)
			return p;
	}
	page = MlnHeapAllocate(heap, sizeof(*page));
	if (page == NULL)
		return NULL;
	bytes = (unsigned char *)page;
	for (i = 0; i < sizeof(*page); i++)
		bytes[i] = 0;
	page->prev = NULL;
	page->next = heap->pages;
	page->freeUnits = MLN_HEAP_PAGE_UNITS;
	heap->pages = page;
	if (page->next != NULL)
		page->next->prev = page;
	return MlnHeapPageAllocate(heap, page, units);
}

/* the page holding p, or NULL */
static inline __attribute__((always_inline)) MlnHeapPage *MlnHeapFindPage(MlnHeap *heap, const void *p)
{
	MlnHeapPage *page;

	for (page = heap->pages; page != NULL; page = page->next) {
		if ((const unsigned char *)p >= page->units[0] &&
		    (const unsigned char *)p < page->units[MLN_HEAP_PAGE_UNITS])
			return page;
	}
	return NULL;
}

/* the block header of p when p is the start of a used block of this heap, else NULL */
static inline __attribute__((always_inline)) MlnHeapBlock *MlnHeapUsedBlock(MlnHeap *heap, void *p)
{
	MlnHeapBlock *block = (MlnHeapBlock *)(((unsigned int)p & ~(MLN_HEAP_UNIT - 1)) -
					       sizeof(MlnHeapBlock));

	if (block < heap->start || block >= heap->end)
		return NULL;
	if (block->prev != NULL && !MlnHeapContains(heap, block->prev))
		return NULL;
	if (block->next < heap->start || block->next > heap->end)
		return NULL;
	if (block->freePrev != NULL && !MlnHeapContains(heap, block->freePrev))
		return NULL;
	if (block->freeNext != NULL &&
	    (block->freeNext < heap->start || block->freeNext > heap->end))
		return NULL;
	if (block->prev >= block->next || block->freePrev > block->freeNext)
		return NULL;
	if (p != (void *)((unsigned int)p & ~(MLN_HEAP_UNIT - 1)))
		return NULL;
	if (block->freePrev != NULL || block->freeNext != NULL)
		return NULL;
	return block;
}

__attribute__((noinline)) void MlnHeapFree(MlnHeap *heap, void *p)
{
	MlnHeapBlock *block, *next;
	MlnHeapPage *page;
	unsigned int run, k;
	int i, merged;

	if (p == NULL)
		return;
	page = MlnHeapFindPage(heap, p);
	if (page != NULL) {
		i = (int)((unsigned char *)p - page->units[0]) / MLN_HEAP_UNIT;
		if (p == page->units[i]) {
			run = page->map[i];
			if (run != 0 && run != MLN_HEAP_RUN_CONTINUED) {
				for (k = 0; k < run; k++)
					page->map[i + k] = 0;
				page->freeUnits += run;
				if (page->freeUnits == MLN_HEAP_PAGE_UNITS) {
					if (page->prev != NULL)
						page->prev->next = page->next;
					if (page->next != NULL)
						page->next->prev = page->prev;
					if (heap->pages == page)
						heap->pages = page->next;
					MlnHeapFree(heap, page);
				}
				return;
			}
		}
	}

	block = MlnHeapUsedBlock(heap, p);
	if (block == NULL)
		return;
	next = block->next;
	merged = 0;
	if (MlnHeapContains(heap, block->prev) && MlnHeapBlockIsFree(block->prev)) {
		block = MlnHeapMerge(heap, block->prev, block);
		next = block->next;
		if (next < heap->start)
			return;
		merged = 1;
	}
	if (next < heap->end && MlnHeapBlockIsFree(next))
		MlnHeapMerge(heap, block, next);
	else if (!merged)
		MlnHeapAddFree(heap, block);
}

int MlnHeapInit(int heap, void *memory, unsigned int size)
{
	MlnHeapSlot *slot = heap ? &s_heaps[1] : &s_heaps[0];
	MlnHeap *h;
	MlnHeapBlock *first, *last;

	if (size < 0x400)
		return MLN_ERROR_NO_MEMORY;
	h = (MlnHeap *)(((unsigned int)memory + MLN_HEAP_UNIT - 1) & ~(MLN_HEAP_UNIT - 1));
	first = (MlnHeapBlock *)(((unsigned int)memory + sizeof(MlnHeap) + MLN_HEAP_UNIT - 1) &
				 ~(MLN_HEAP_UNIT - 1));
	last = (MlnHeapBlock *)(((unsigned int)memory + size - MLN_HEAP_UNIT) &
				~(MLN_HEAP_UNIT - 1));
	h->freeMap[0] = 0;
	h->freeMap[1] = 0;
	h->freeMap[2] = 0;
	h->freeMap[3] = 0;
	h->start = first;
	h->end = last;
	h->rover = first;
	h->pages = NULL;
	first->prev = NULL;
	first->next = last;
	first->freePrev = NULL;
	first->freeNext = last;
	if (slot->heap != NULL)
		return 1;
	slot->heap = h;
	MlnMutexCreate(&slot->mutex);
	return MLN_OK;
}

void *MlnAllocate(int heap, unsigned int size)
{
	MlnHeap *h;
	void *p;

	if (!heap) {
		if (MlnMutexLock(&s_heaps[0].mutex) != 0)
			return NULL;
		h = s_heaps[0].heap;
	} else {
		if (MlnMutexLock(&s_heaps[1].mutex) != 0)
			return NULL;
		h = s_heaps[1].heap;
	}
	p = h != NULL ? MlnHeapAllocate(h, size) : NULL;
	MlnMutexUnlock(heap ? &s_heaps[1].mutex : &s_heaps[0].mutex);
	return p;
}

void *MlnReallocate(int heap, void *p, unsigned int size)
{
	MlnHeap *h;
	MlnHeapBlock *block, *rest;
	MlnHeapPage *page;
	unsigned int run, copy, k;
	int i, units, blockUnits;
	void *q = NULL;

	if (!heap) {
		MlnMutexLock(&s_heaps[0].mutex);
		h = s_heaps[0].heap;
	} else {
		MlnMutexLock(&s_heaps[1].mutex);
		h = s_heaps[1].heap;
	}
	if (h == NULL)
		goto out;
	if (p == NULL && size != 0) {
		q = MlnHeapAllocate(h, size);
		goto out;
	}
	if (p != NULL && size == 0) {
		MlnHeapFree(h, p);
		goto out;
	}
	if (p == NULL || size == 0)
		goto out;

	units = (size + MLN_HEAP_UNIT - 1) >> 4;
	page = MlnHeapFindPage(h, p);
	if (page != NULL) {
		i = (int)((unsigned char *)p - page->units[0]) / MLN_HEAP_UNIT;
		run = page->map[i];
		if (p == page->units[i] && run != 0 && run != MLN_HEAP_RUN_CONTINUED) {
			if (units == (int)run) {
				q = p;
				goto out;
			}
			q = MlnHeapAllocate(h, size);
			if (q == NULL)
				goto out;
			copy = run * MLN_HEAP_UNIT < size ? run * MLN_HEAP_UNIT : size;
			MlnMemcpy(q, p, copy);
			for (k = 0; k < run; k++)
				page->map[i + k] = 0;
			page->freeUnits += run;
			if (page->freeUnits == MLN_HEAP_PAGE_UNITS) {
				if (page->prev != NULL)
					page->prev->next = page->next;
				if (page->next != NULL)
					page->next->prev = page->prev;
				if (h->pages == page)
					h->pages = page->next;
				MlnHeapFree(h, page);
			}
			goto out;
		}
	}

	block = MlnHeapUsedBlock(h, p);
	if (block == NULL)
		goto out;
	blockUnits = (((char *)block->next - (char *)block) >> 4) - 1;
	if (blockUnits > units + 1) {
		rest = block + units + 1;
		rest->prev = block;
		rest->next = block->next;
		rest->freePrev = NULL;
		rest->freeNext = NULL;
		block->next = rest;
		if (MlnHeapContains(h, rest->next)) {
			rest->next->prev = rest;
			if (MlnHeapBlockIsFree(rest->next)) {
				q = p;
				MlnHeapMerge(h, rest, rest->next);
				goto out;
			}
			MlnHeapAddFree(h, rest);
		}
		q = p;
		goto out;
	}
	if (units <= blockUnits) {
		q = p;
		goto out;
	}
	q = MlnHeapAllocate(h, size);
	if (q == NULL)
		goto out;
	copy = (unsigned int)blockUnits * MLN_HEAP_UNIT < size ?
		(unsigned int)blockUnits * MLN_HEAP_UNIT : size;
	MlnMemcpy(q, p, copy);
	MlnHeapFree(h, p);
out:
	MlnMutexUnlock(heap ? &s_heaps[1].mutex : &s_heaps[0].mutex);
	return q;
}

void MlnFree(int heap, void *p)
{
	MlnHeap *h;

	if (!heap) {
		MlnMutexLock(&s_heaps[0].mutex);
		h = s_heaps[0].heap;
	} else {
		MlnMutexLock(&s_heaps[1].mutex);
		h = s_heaps[1].heap;
	}
	if (h != NULL)
		MlnHeapFree(h, p);
	MlnMutexUnlock(heap ? &s_heaps[1].mutex : &s_heaps[0].mutex);
}

void MlnHeapTerm(int heap)
{
	MlnHeap **slot;
	MlnMutex *mutex;
	MlnHeap *h;

	if (!heap) {
		slot = &s_heaps[0].heap;
		h = s_heaps[0].heap;
		mutex = &s_heaps[0].mutex;
	} else {
		mutex = &s_heaps[1].mutex;
		slot = &s_heaps[1].heap;
		h = s_heaps[1].heap;
	}
	if (h == NULL)
		return;
	MlnMutexLock(mutex);
	*slot = NULL;
	MlnMutexUnlock(mutex);
	MlnMutexDelete(mutex);
}

char *MlnStrdup(int heap, const char *s)
{
	unsigned int length;
	char *copy;

	if (s == NULL)
		return NULL;
	length = MlnStrlen(s);
	copy = MlnAllocate(heap, length + 1);
	if (copy == NULL)
		return NULL;
	MlnMemcpy(copy, s, length + 1);
	return copy;
}

void *MlnMemmove(void *dst, const void *src, unsigned int size)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	if (d < s) {
		while (size--)
			*d++ = *s++;
	} else {
		d += size;
		s += size;
		while (size--)
			*--d = *--s;
	}
	return dst;
}

void *MlnMemcpy(void *dst, const void *src, unsigned int size)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	while (size--)
		*d++ = *s++;
	return dst;
}

int MlnMemcmp(const void *a, const void *b, unsigned int size)
{
	const signed char *p = a;
	const signed char *q = b;

	if (size == 0)
		return 0;
	while (*p == *q) {
		if (--size == 0)
			return 0;
		p++;
		q++;
	}
	return *p < *q ? -1 : 1;
}

void *MlnMemset(void *dst, int c, unsigned int size)
{
	unsigned char *d = dst;

	while (size--)
		*d++ = c;
	return dst;
}
