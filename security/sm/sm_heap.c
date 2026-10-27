/*
 * sm_heap.c: Shared 4 KB page bitmap and best-fit block heap allocator for
 * PlayStation Vita os0:sm secure modules (spkg_verifier_sm_w_key_2, qaf_sm,
 * pm_sm, update_service_sm).
 *
 * Internal function, structure, and variable names are ours.
 */

#include "sm_common.h"

static void *s_heapPoolBase;
static uint32_t s_heapTotalPages;
static uint32_t s_heapPageBitmap;
static uint32_t s_heapTotalBytes;
static SmHeapBlock *s_heapUsedList;
static SmHeapBlock *s_heapFreeList;

/* 0x80c3e4 (name ours; C++ operator new(unsigned int)) */
void *operatorNew(size_t size)
{
	if (size != 0u)
		return smHeapMalloc(size);
	return NULL;
}

/* 0x80c3f4 (name ours; C++ operator new(unsigned int, std::nothrow_t const&)) */
void *operatorNewNothrow(size_t size, const void *nothrow)
{
	(void)nothrow;
	return operatorNew(size);
}

/* 0x80c3f8 (name ours; C++ operator new[](unsigned int)) */
void *operatorNewArray(size_t size)
{
	if (size != 0u)
		return smHeapMalloc(size);
	return NULL;
}

/* 0x80c408 (name ours; C++ operator new[](unsigned int, std::nothrow_t const&)) */
void *operatorNewArrayNothrow(size_t size, const void *nothrow)
{
	(void)nothrow;
	return operatorNewArray(size);
}

/* 0x80c40c (name ours; C++ operator delete(void*)) */
void operatorDelete(void *ptr)
{
	if (ptr != NULL)
		smHeapFree(ptr);
}

/* 0x80c41a (name ours; C++ operator delete(void*, std::nothrow_t const&)) */
void operatorDeleteNothrow(void *ptr, const void *nothrow)
{
	(void)nothrow;
	operatorDelete(ptr);
}

/* 0x80c41e (name ours; C++ operator delete[](void*)) */
void operatorDeleteArray(void *ptr)
{
	if (ptr != NULL)
		smHeapFree(ptr);
}

/* 0x80c42c (name ours; C++ operator delete[](void*, std::nothrow_t const&)) */
void operatorDeleteArrayNothrow(void *ptr, const void *nothrow)
{
	(void)nothrow;
	operatorDelete(ptr);
}

/* 0x80c430 (name ours) */
void smHeapInsertSorted(SmHeapBlock *blk, SmHeapBlock **listHead)
{
	SmHeapBlock *head;
	SmHeapBlock *cur;

	if (blk == NULL)
		return;

	head = *listHead;
	if (head == NULL) {
		*listHead = blk;
		blk->prev = NULL;
		blk->next = NULL;
		return;
	}

	cur = head;
	do {
		int64_t expectedUser = (int64_t)(intptr_t)cur + 24ll;
		int64_t actualUser = (int64_t)(intptr_t)cur->userPtr;

		if (actualUser != expectedUser)
			return;

		if ((uintptr_t)blk->userPtr < (uintptr_t)cur->userPtr) {
			SmHeapBlock *prev = cur->prev;

			if (prev != NULL) {
				cur->prev = blk;
				prev->next = blk;
				blk->prev = prev;
				blk->next = cur;
				goto walk_tail;
			}
			if (cur != NULL) {
				*listHead = blk;
				blk->next = cur;
				cur->prev = blk;
				goto walk_tail;
			}
			break;
		}
		cur = cur->next;
	} while (cur != NULL);

	while (head->next != NULL)
		head = head->next;

	head->next = blk;
	blk->prev = head;
	blk->next = NULL;

walk_tail:
	cur = *listHead;
	while (cur != NULL)
		cur = cur->next;
}

/* 0x80c4b8 (name ours) */
void smHeapUnlinkBlock(SmHeapBlock *blk, SmHeapBlock **listHead)
{
	SmHeapBlock *prev;
	SmHeapBlock *next;

	if (blk == NULL || *listHead == NULL)
		return;

	prev = blk->prev;
	if (prev != NULL)
		prev->next = blk->next;

	next = blk->next;
	if (next != NULL)
		next->prev = prev;

	if (blk == *listHead)
		*listHead = next;
}

/* 0x80c4e0 (name ours) */
SmHeapBlock *smHeapMergeBlocks(SmHeapBlock *b1, SmHeapBlock *b2)
{
	SmHeapBlock *low;
	uint32_t mergedSize;

	if (b1 == NULL || b2 == NULL)
		return NULL;

	low = ((uintptr_t)b2 < (uintptr_t)b1) ? b2 : b1;
	mergedSize = b1->size + b2->size + 24u + b1->padBytes + b2->padBytes;
	low->size = mergedSize;
	low->pageCount = (mergedSize + 0xfffu) >> 12;
	low->prev = NULL;
	low->next = NULL;
	return low;
}

/* 0x80c524 (name ours) */
uint8_t smHeapAreBlocksAdjacentInPage(const SmHeapBlock *b1, const SmHeapBlock *b2)
{
	const SmHeapBlock *low;
	const SmHeapBlock *high;

	if (b1 == NULL || b2 == NULL)
		return 0u;

	if ((uintptr_t)b2 < (uintptr_t)b1) {
		low = b2;
		high = b1;
	} else if ((uintptr_t)b1 < (uintptr_t)b2) {
		low = b1;
		high = b2;
	} else {
		return 0u;
	}

	if ((uintptr_t)low->userPtr + low->size + low->padBytes != (uintptr_t)high)
		return 0u;

	return ((((uintptr_t)low->userPtr ^ (uintptr_t)high->userPtr) & ~0xfffu) == 0u) ? 1u : 0u;
}

/* 0x80c55a (name ours) */
int smHeapFreePages(void *pageAddr, uint32_t pageCount)
{
	uint32_t startPage;
	uint32_t mask;
	uint32_t i;

	if (pageAddr == NULL || pageCount == 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	startPage = ((uintptr_t)pageAddr - (uintptr_t)s_heapPoolBase) >> 12;
	if (startPage + pageCount > s_heapTotalPages)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(pageAddr, 0, pageCount << 12);

	mask = s_heapPageBitmap;
	for (i = 0u; i < pageCount; i++)
		mask &= ~(1u << (startPage + i));

	s_heapPageBitmap = mask;
	return 0;
}

/* 0x80c5c0 (name ours) */
int smHeapAllocPages(void **outAddr, uint32_t pageCount, int zeroMem)
{
	uint32_t totalPages;
	uint32_t bitmap;
	uint32_t startPage = 0u;
	uint32_t runLen = 0u;
	uint32_t i;

	if (outAddr == NULL || pageCount == 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	totalPages = s_heapTotalPages;
	if (pageCount > totalPages)
		return SCE_SBL_ERROR_INVALID_VALUE;

	*outAddr = NULL;
	bitmap = s_heapPageBitmap;

	for (i = 0u; i < totalPages; i++) {
		if ((bitmap & (1u << i)) != 0u) {
			startPage = i + 1u;
			runLen = 0u;
		} else {
			runLen++;
			if (runLen == pageCount) {
				uint32_t j;
				void *addr;

				for (j = 0u; j < pageCount; j++)
					bitmap |= (1u << (startPage + j));

				s_heapPageBitmap = bitmap;
				addr = (uint8_t *)s_heapPoolBase + (startPage << 12);
				*outAddr = addr;
				if (zeroMem == 1)
					memset(addr, 0, pageCount << 12);
				return 0;
			}
		}
	}

	return SCE_SBL_ERROR_NO_MEMORY;
}

/* 0x80c65a (name ours) */
void smHeapCoalesceBlock(SmHeapBlock *blk)
{
	SmHeapBlock *prev;
	SmHeapBlock *next;
	SmHeapBlock *merged;
	uint32_t adjMask;

	if (blk == NULL || s_heapFreeList == NULL)
		return;

	prev = blk->prev;
	next = blk->next;
	adjMask = smHeapAreBlocksAdjacentInPage(prev, blk);
	if (smHeapAreBlocksAdjacentInPage(blk, next) != 0u)
		adjMask += 2u;

	if (adjMask == 1u) {
		smHeapUnlinkBlock(prev, &s_heapFreeList);
		smHeapUnlinkBlock(blk, &s_heapFreeList);
		merged = smHeapMergeBlocks(prev, blk);
	} else if (adjMask == 2u) {
		smHeapUnlinkBlock(blk, &s_heapFreeList);
		smHeapUnlinkBlock(next, &s_heapFreeList);
		merged = smHeapMergeBlocks(blk, next);
	} else if (adjMask == 3u) {
		smHeapUnlinkBlock(prev, &s_heapFreeList);
		smHeapUnlinkBlock(blk, &s_heapFreeList);
		smHeapUnlinkBlock(next, &s_heapFreeList);
		merged = smHeapMergeBlocks(prev, blk);
		merged = smHeapMergeBlocks(merged, next);
	} else {
		return;
	}

	smHeapInsertSorted(merged, &s_heapFreeList);
}

/* 0x80c714 (name ours) */
int smHeapInitCore(void *poolBase, uint32_t poolSize)
{
	if (poolBase == NULL || poolSize == 0u ||
	    ((uintptr_t)poolBase & 0xfffu) != 0u ||
	    (poolSize & 0xfffu) != 0u ||
	    ((uintptr_t)poolBase & 3u) != 0u ||
	    poolSize > 0x20000u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	s_heapTotalBytes = poolSize;
	s_heapPoolBase = poolBase;
	s_heapTotalPages = poolSize >> 12;
	s_heapPageBitmap = 0u;
	s_heapUsedList = NULL;
	s_heapFreeList = NULL;
	return 0;
}

/* 0x80c75a (name ours) */
int smHeapInit(void *poolBase, uint32_t poolSize)
{
	memset(poolBase, 0, poolSize);
	return smHeapInitCore(poolBase, poolSize);
}

/* 0x80c782 (name ours) */
void *smHeapMalloc(size_t size)
{
	uint32_t alignPad;
	uint32_t alignedSize;
	uint32_t neededPages;
	uint32_t totalNeeded;
	SmHeapBlock *cur;
	SmHeapBlock *best;

	if (s_heapTotalBytes == 0u || size - 1u >= 0xffffffe5u)
		return NULL;

	alignPad = (uint32_t)(-(int32_t)size) & 3u;
	alignedSize = (uint32_t)size + alignPad;
	neededPages = (alignedSize + 0x1017u) >> 12;
	totalNeeded = alignedSize + 24u;

	cur = s_heapFreeList;
	if (cur != NULL) {
		uint32_t bestRem = 0u;

		best = NULL;
		do {
			int64_t expectedUser = (int64_t)(intptr_t)cur + 24ll;
			int64_t actualUser = (int64_t)(intptr_t)cur->userPtr;

			if (actualUser != expectedUser)
				goto alloc_new_page;

			if (cur->size >= alignedSize) {
				uint32_t rem = cur->size - alignedSize;

				if (best == NULL || rem < bestRem) {
					best = cur;
					bestRem = rem;
				}
			}
			cur = cur->next;
		} while (cur != NULL);

		if (best != NULL) {
			uint32_t remBytes;
			uint32_t extraPad;
			SmHeapBlock *splitBlk;

			smHeapUnlinkBlock(best, &s_heapFreeList);
			remBytes = best->size - alignedSize;
			if ((int32_t)remBytes >= 25) {
				splitBlk = (SmHeapBlock *)((uint8_t *)best + 24u + size + alignPad);
				extraPad = 0u;
			} else {
				if (remBytes > 24u)
					return NULL;
				splitBlk = NULL;
				extraPad = remBytes;
			}

			best->userPtr = (uint8_t *)best + 24u;
			best->size = (uint32_t)size;
			best->pageCount = neededPages;
			best->padBytes = extraPad + alignPad;

			if (splitBlk != NULL) {
				splitBlk->userPtr = (uint8_t *)splitBlk + 24u;
				splitBlk->size = remBytes - 24u;
				splitBlk->pageCount = (remBytes + 0xfffu) >> 12;
				splitBlk->padBytes = 0u;
				splitBlk->prev = NULL;
				splitBlk->next = NULL;
			}

			best->prev = NULL;
			best->next = NULL;
			smHeapInsertSorted(best, &s_heapUsedList);
			if (splitBlk != NULL) {
				smHeapInsertSorted(splitBlk, &s_heapFreeList);
				smHeapCoalesceBlock(splitBlk);
			}
			return best->userPtr;
		}
	}

alloc_new_page:
	{
		void *pageMem = NULL;

		if (smHeapAllocPages(&pageMem, neededPages, 1) != 0 || pageMem == NULL)
			return NULL;

		best = (SmHeapBlock *)pageMem;
		best->userPtr = (uint8_t *)best + 24u;
		best->padBytes = alignPad;
		best->size = (uint32_t)size;
		best->pageCount = neededPages;
		best->prev = NULL;
		best->next = NULL;
		smHeapInsertSorted(best, &s_heapUsedList);

		if (best->pageCount == 1u) {
			uint32_t pageRem = (uint32_t)(-(int32_t)totalNeeded) & 0xfffu;

			if (pageRem >= 25u) {
				SmHeapBlock *tailBlk = (SmHeapBlock *)((uint8_t *)best->userPtr + size + alignPad);

				tailBlk->pageCount = 1u;
				tailBlk->userPtr = (uint8_t *)tailBlk + 24u;
				tailBlk->size = pageRem - 24u;
				tailBlk->padBytes = 0u;
				tailBlk->prev = NULL;
				tailBlk->next = NULL;
				smHeapInsertSorted(tailBlk, &s_heapFreeList);
			}
		}
		return best->userPtr;
	}
}

/* 0x80c926 (name ours) */
void *smHeapCalloc(size_t nmemb, size_t size)
{
	size_t total;
	void *ptr;

	if (s_heapTotalBytes == 0u)
		return NULL;

	total = size * nmemb;
	ptr = smHeapMalloc(total);
	if (ptr != NULL)
		memset(ptr, 0, total);
	return ptr;
}

/* 0x80c964 (name ours) */
void smHeapFree(void *ptr)
{
	SmHeapBlock *blk;
	int64_t expectedUser;
	int64_t actualUser;

	if (s_heapTotalBytes == 0u || ptr == NULL)
		return;

	blk = (SmHeapBlock *)((uint8_t *)ptr - 24);
	expectedUser = (int64_t)(intptr_t)ptr;
	actualUser = (int64_t)(intptr_t)blk->userPtr;
	if (actualUser != expectedUser)
		return;

	smHeapUnlinkBlock(blk, &s_heapUsedList);
	if (blk->pageCount >= 2u) {
		smHeapFreePages((void *)((uintptr_t)blk & ~0xfffu), blk->pageCount);
		return;
	}

	if (s_heapUsedList != NULL) {
		SmHeapBlock *cur = s_heapUsedList;
		uint32_t samePageCount = 0u;

		do {
			if ((int64_t)(intptr_t)cur->userPtr != (int64_t)(intptr_t)cur + 24ll)
				return;
			if ((((uintptr_t)blk->userPtr ^ (uintptr_t)cur->userPtr) & ~0xfffu) == 0u)
				samePageCount++;
			cur = cur->next;
		} while (cur != NULL);

		if (samePageCount != 0u) {
			blk->prev = NULL;
			blk->padBytes = 0u;
			blk->size += blk->padBytes;
			blk->next = NULL;
			smHeapInsertSorted(blk, &s_heapFreeList);
			smHeapCoalesceBlock(blk);
			return;
		}
	}

	{
		SmHeapBlock *cur = s_heapFreeList;

		while (cur != NULL) {
			if ((int64_t)(intptr_t)cur->userPtr != (int64_t)(intptr_t)cur + 24ll)
				break;
			if ((((uintptr_t)blk->userPtr ^ (uintptr_t)cur->userPtr) & ~0xfffu) == 0u)
				smHeapUnlinkBlock(cur, &s_heapFreeList);
			cur = cur->next;
		}
	}

	smHeapFreePages((void *)((uintptr_t)blk & ~0xfffu), blk->pageCount);
}

/* 0x80ca7a (name ours) */
void *smHeapAllocPagesZeroed(uint32_t pageCount)
{
	void *addr = NULL;

	if (smHeapAllocPages(&addr, pageCount, 1) == 0)
		return addr;
	return NULL;
}

/* 0x80ca9a (name ours) */
void *smHeapAllocPagesUninit(uint32_t pageCount)
{
	void *addr = NULL;

	if (smHeapAllocPages(&addr, pageCount, 0) == 0)
		return addr;
	return NULL;
}

/* 0x80cab8 (name ours) */
int smHeapFreePagesThunk(void *pageAddr, uint32_t pageCount)
{
	return smHeapFreePages(pageAddr, pageCount);
}

/* 0x80cabc (name ours) */
uint32_t smHeapAlignUpShift(uint32_t val, uint32_t alignMask, uint32_t shift)
{
	return ((val + ~alignMask) & alignMask) >> shift;
}
