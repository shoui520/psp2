/*
 * SceSysmem: fixed heaps. A fixed heap hands out objects of one size, carved
 * one after another from 4K or 64K pages (simple memblocks). Freed objects go
 * to a per-CPU cache when the heap has one, else to a free list; new pages are
 * added as the heap runs out, and the page tables grow with them.
 */

#include "sysmem_internal.h"
#include <sysmem/uid_guid.h>
#include <sysmem/compiler.h>

/* name unknown */
/* name unknown: AllocSimpleMemBlock found no free physical memory */

/* the smbType of a page */
#define SMB_TYPE_4K			0x1000
#define SMB_TYPE_64K			0x10004

/* "SceUIDFixedHeapClass" */
static FixedHeapClass *s_fixedHeapClass;

/* name unknown: carve the next object from the current page */
static void *FixedHeapCarveObject(FixedHeap *heap)
{
	void *object = heap->cursor;
	SceUInt16 used;
	void (*onCarve)(void *object, SceUInt32 index);

	if (object == NULL)
		return object;
	used = heap->usedInPage;
	if (used >= heap->perPage) {
		heap->currentPage = NULL;
		heap->cursor = NULL;
		heap->usedInPage = 0;
		return NULL;
	}
	heap->usedInPage = used + 1;
	onCarve = heap->uidHeap.cls->onCarve;
	heap->cursor = (char *)object + heap->itemSize;
	*(void **)object = NULL;
	if (onCarve != NULL)
		onCarve(object, heap->perPage * (heap->pageCount - 1) + used);
	heap->totalCount++;
	heap->freeCount++;
	return object;
}

/* name unknown: the class's setPage */
static int FixedHeapClassSetPage(FixedHeap *heap, void *page)
{
	heap->currentPage = page;
	heap->cursor = page;
	heap->usedInPage = 0;
	return 0;
}

/* give a freed object to this CPU's cache; 0: the cache is full */
static inline int CpuCacheFree(FixedHeap *heap, void *object)
{
	FixedHeapCpuCache *cache;
	int state;

	state = SceCpuForDriver_821FC0EE();
	cache = &heap->caches[SceCpuId()];
#if FW_DEBUG
	if (cache->count != 0) {
		void **entry;

		for (entry = cache->head; entry != NULL; entry = *entry) {
			if (entry != object)
				continue;
			SCE_DBG_PRINTF(0xb, 360, "***** bugzilla 71075 *****\n");
			SCE_DBG_PRINTF(0xb, 361, "pFixedHeap=%p\n", heap);
			SCE_DBG_PRINTF(0xb, 362, ".itemSize=0x%08x\n", heap->itemSize);
			SCE_DBG_PRINTF(0xb, 363, "pCache->nCount=%d\n", cache->count);
			for (entry = cache->head; entry != NULL; entry = *entry)
				SCE_DBG_PRINTF(0xb, 369, "q2=%p, pObject=%p\n", entry, object);
			__breakpoint(0);
			break;
		}
	}
#endif
	if (cache->count < cache->max) {
		*(void **)object = NULL;
		if (cache->count == 0) {
			cache->head = object;
			cache->tail = object;
		} else {
			*(void **)cache->tail = object;
			cache->tail = object;
		}
		cache->count++;
		sceKernelCpuResumeIntr(state);
		SceCpuForDriver_59F74E94(&heap->freeCount, 1);
		return 1;
	} else {
		sceKernelCpuResumeIntr(state);
		return 0;
	}
}

/* whether an object is in one of the heap's pages */
static inline unsigned int FixedHeapFindPage(const FixedHeap *heap, const void *object)
{
	unsigned int i;

	for (i = 0; i < heap->pageCount; i++) {
		const char *page = heap->pages[i];

		if (((const char *)object < page + heap->pageSize) &
		    ((const char *)object >= page))
			return 1;
	}
	return 0;
}

/* put a freed object on the free list; the heap is write locked */
/* name unknown: FixedHeapFindPage with the bounds tested the other way round */
static inline unsigned int FixedHeapHasObject(const FixedHeap *heap, const void *object)
{
	unsigned int i;

	for (i = 0; i < heap->pageCount; i++) {
		const char *page = heap->pages[i];

		if (((const char *)object >= page) &
		    ((const char *)object < page + heap->pageSize))
			return 1;
	}
	return 0;
}

static inline void FixedHeapPushFree(FixedHeap *heap, void *object)
{
	void *tail = heap->freeTail;

	*(void **)object = NULL;
	if (tail == NULL) {
		heap->freeHead = object;
		heap->freeTail = object;
	} else {
		*(void **)tail = object;
		heap->freeTail = object;
	}
	heap->fifoCount++;
	SceCpuForDriver_59F74E94(&heap->freeCount, 1);
}

/* take the oldest object off the free list; the heap is write locked */
static inline void *FixedHeapPopFree(FixedHeap *heap)
{
	void *object = heap->freeHead;

	if (object == NULL)
		return NULL;
	if (object == heap->freeTail) {
		heap->freeHead = NULL;
		heap->freeTail = NULL;
	} else {
		heap->freeHead = *(void **)object;
	}
	*(void **)object = NULL;
	heap->fifoCount--;
	return object;
}

/* one more object in use */
static inline void FixedHeapCountAlloc(FixedHeap *heap)
{
	SceUInt16 used;

	SceCpuForDriver_9A693F5B(&heap->freeCount);
	used = heap->totalCount - heap->freeCount;
	if (used > heap->peakCount)
		heap->peakCount = used;
}

/* name unknown: the kernel heap's page tables (and name) back where they came from */
static void FixedHeapReleaseTables(FixedHeap *heap, int all)
{
	if (heap->attr & FIXED_HEAP_ATTR_NAME_HEAP) {
		if (all == 1 && heap->name != NULL) {
			HeapFreeLocked(heap->heap, heap->name);
			heap->name = NULL;
			heap->attr &= ~FIXED_HEAP_ATTR_NAME_HEAP;
		}
		heap->attr &= ~FIXED_HEAP_ATTR_NAME_HEAP;
	}
	if ((heap->attr & FIXED_HEAP_ATTR_NAME_HOOK) && all == 1 && heap->name != NULL) {
		heap->hook->funcFree(heap->hook->arg, heap->name);
		heap->name = NULL;
		heap->attr &= ~FIXED_HEAP_ATTR_NAME_HOOK;
	}
	if (heap->attr & FIXED_HEAP_ATTR_TABLES_HEAP) {
		HeapFreeLocked(heap->heap, heap->pages);
		HeapFreeLocked(heap->heap, heap->blocks);
		heap->attr &= ~FIXED_HEAP_ATTR_TABLES_HEAP;
	}
	if (heap->attr & FIXED_HEAP_ATTR_TABLES_KERNEL) {
		KernelFixedHeapFreeObject(heap->maxPages * sizeof(void *), heap->pages);
		KernelFixedHeapFreeObject(heap->maxPages * sizeof(void *), heap->blocks);
		heap->attr &= ~FIXED_HEAP_ATTR_TABLES_KERNEL;
	}
	if (heap->attr & FIXED_HEAP_ATTR_TABLES_HOOK) {
		heap->hook->funcFree(heap->hook->arg, heap->pages);
		heap->hook->funcFree(heap->hook->arg, heap->blocks);
		heap->attr &= ~FIXED_HEAP_ATTR_TABLES_HOOK;
	}
	heap->pages = NULL;
	heap->blocks = NULL;
}

/* name unknown: move the page tables to larger ones, from where attr says */
static int FixedHeapReplaceTables(FixedHeap *heap, SceUInt32 attr, uintptr_t *pages, void **blocks,
				  SceUInt32 maxPages)
{
	unsigned int i;

	for (i = 0; i < heap->pageCount; i++) {
		void **oldBlocks = heap->blocks;

		pages[i] = (uintptr_t)heap->pages[i];
		if (oldBlocks != NULL)
			blocks[i] = oldBlocks[i];
		else
			blocks[i] = NULL;
	}
	FixedHeapReleaseTables(heap, 0);
	heap->pages = (void **)pages;
	heap->blocks = blocks;
	heap->maxPages = maxPages;
	heap->attr |= attr;
	return 0;
}

static inline void FixedHeapCallAllocHook(FixedHeap *heap, void *object, const void *caller)
{
	if (heap != NULL && heap->objectHook != NULL)
		heap->objectHook->onAlloc(heap->objectHook->arg, heap, object, caller);
}

static inline void FixedHeapCallFreeHook(FixedHeap *heap, void *object, const void *caller)
{
	if (heap != NULL && heap->objectHook != NULL)
		heap->objectHook->onFree(heap->objectHook->arg, heap, object, caller);
}

/* the page and its simple memblock into the page tables, twice as large when full */
static inline void FixedHeapAddPage(FixedHeap *heap, void *page, struct MemBlockObject *block)
{
	void **pages, **blocks;
	void *kernelHeap;

	if (heap->pageCount >= heap->maxPages) {
		kernelHeap = heap->heap;
		if (kernelHeap == NULL)
			heap->heap = kernelHeap = UIDManagerGetKernelHeap();
		pages = HeapAllocLocked(kernelHeap, heap->maxPages * 2 * sizeof(void *));
		blocks = HeapAllocLocked(kernelHeap, heap->maxPages * 2 * sizeof(void *));
		if (pages == NULL || blocks == NULL) {
			if (kernelHeap != NULL) {
				if (pages != NULL)
					HeapFreeLocked(kernelHeap, pages);
				if (blocks != NULL)
					HeapFreeLocked(kernelHeap, blocks);
			}
			return;
		}
		FixedHeapReplaceTables(heap, FIXED_HEAP_ATTR_TABLES_HEAP, (uintptr_t *)pages, blocks,
				       heap->maxPages * 2);
	}
	heap->pages[heap->pageCount] = page;
	heap->blocks[heap->pageCount] = block;
	heap->pageCount++;
}

__attribute__((noinline)) int FixedHeapAllocObjectWithOpt(FixedHeap *heap, SceUInt32 opt, int raw,
						    void **object)
{
	struct MemBlockObject *smb;
	void *entry, *base;
	const char *name;
	SceUInt32 attr;
	int state, ret;

	if (heap->attr & FIXED_HEAP_ATTR_CPU_CACHE) {
		FixedHeapCpuCache *cache;

		state = SceCpuForDriver_821FC0EE();
		cache = &heap->caches[SceCpuId()];
		if (cache->count != 0) {
			entry = cache->head;
			cache->count--;
			if (cache->count == 0) {
				cache->head = NULL;
				cache->tail = NULL;
			} else {
				cache->head = *(void **)entry;
			}
			*(void **)entry = NULL;
			sceKernelCpuResumeIntr(state);
			FixedHeapCountAlloc(heap);
			*object = entry;
			return 0;
		}
		sceKernelCpuResumeIntr(state);
	}

	if (raw == 0) {
		state = SceCpuForDriver_4C38CE4D(&heap->lock);
	} else {
#if FW_DEBUG
		state = SceCpuForDriver_F02467D1(&heap->lock);
		if (state >= 0) {
			sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
			SCE_DBG_PRINTF(0xb, 1272, "Raw mode but not write locked\n");
			SCE_DBG_ASSERT_FAIL(1273);
		}
#endif
	}

	ret = 0;
	entry = FixedHeapCarveObject(heap);
	if (entry == NULL)
		entry = FixedHeapPopFree(heap);
	if (entry == NULL) {
		/* a new page */
		name = heap->name;
		attr = (opt << 4 & 0x30) | 1;
		if (heap->pageSize == 0x1000) {
			ret = AllocSimpleMemBlock(name, SMB_TYPE_4K, heap->partition, attr, &smb);
			if (ret < 0) {
				if (heap->partition == NULL ||
				    heap->partition == SceSysmemForKernel_0164D817()) {
					if (ret == SCE_KERNEL_ERROR_NO_SMB)
						SCE_PANIC(1332);
					SCE_PANIC(1334);
				}
				goto failed;
			}
		} else {
			if (heap->pageSize != 0x10000)
				SCE_PANIC(1370);
			ret = AllocSimpleMemBlock(name, SMB_TYPE_64K, heap->partition, attr, &smb);
			if (ret < 0) {
				if (heap->partition == NULL ||
				    heap->partition == SceSysmemForKernel_0164D817()) {
					SCE_DBG_PRINTF(0xb, 1354, "AllocSimpleMemBlock([%s], 64K) failed 0x%08x\n",
						       name, ret);
					if (ret == SCE_KERNEL_ERROR_NO_SMB)
						SCE_PANIC(1356);
					SCE_PANIC(1358);
				}
				goto failed;
			}
		}
		ret = SceSysmemForKernel_39D5A3B7(smb, &base);
		if (ret < 0) {
			if (smb != NULL)
				SceSysmemForKernel_A4DB88E1(smb);
			goto failed;
		}
		FixedHeapAddPage(heap, base, smb);
		heap->uidHeap.cls->setPage(heap, base);
		entry = FixedHeapCarveObject(heap);
		if (entry == NULL)
			entry = FixedHeapPopFree(heap);
	}
	if (entry != NULL)
		FixedHeapCountAlloc(heap);

	if (raw == 0) {
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	} else {
#if FW_DEBUG
		state = SceCpuForDriver_F02467D1(&heap->lock);
		if (state >= 0) {
			sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
			SCE_DBG_ASSERT_FAIL(1422);
		}
#endif
	}
	if (entry == NULL)
		ret = SCE_KERNEL_ERROR_FIXEDHEAP_ERROR;
	*object = entry;
	return ret;

failed:
	if (raw == 0) {
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	} else {
#if FW_DEBUG
		state = SceCpuForDriver_F02467D1(&heap->lock);
		if (state >= 0) {
			sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
			SCE_DBG_ASSERT_FAIL(1448);
		}
#endif
	}
	*object = NULL;
	return ret;
}

/* name unknown: an object, told to the object hook */
int FixedHeapAllocObject(FixedHeap *heap, void **object)
{
	int ret;

	ret = FixedHeapAllocObjectWithOpt(heap, 0, 0, object);
	if (ret >= 0)
		FixedHeapCallAllocHook(heap, *object, __builtin_return_address(0));
	return ret;
}

/* name unknown: the class's alloc, for objects of up to itemSize bytes */
int FixedHeapClassAlloc(void *uidHeap, SceSize size, void **object)
{
	FixedHeap *heap = uidHeap;
	SceUInt16 itemSize = heap->itemSize;

	if (itemSize != size) {
		switch (itemSize) {
		case 0x10:
			if (size < 0x10)
				size = 0x10;
			break;
		case 0x20:
			if (size < 0x20)
				size = 0x20;
			break;
		case 0x40:
			if (size > 0x20 && size < itemSize)
				size = itemSize;
			break;
		case 0x80:
			if (size > 0x40 && size < itemSize)
				size = itemSize;
			break;
		case 0x100:
			if (size > 0x80 && size < itemSize)
				size = itemSize;
			break;
		case 0x200:
			if (size > 0x100 && size < itemSize)
				size = itemSize;
			break;
		}
		if (size != itemSize) {
			*object = NULL;
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	return FixedHeapAllocObject(heap, object);
}

int KernelFixedHeapAllocObject(SceSize size, void **object)
{
	switch (size) {
	case 0x20:
		return FixedHeapAllocObject(g_uidManager->fixedHeap32B, object);
	case 0x30:
		return FixedHeapAllocObject(g_uidManager->fixedHeap48B, object);
	case 0x40:
		FixedHeapAllocObject(g_uidManager->fixedHeap64B, object);
		return 0;
	case 0x80:
		FixedHeapAllocObject(g_uidManager->fixedHeap128B, object);
		return 0;
	}
	SCE_DBG_ASSERT_FAIL(1577);
	*object = NULL;
	return SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_SIZE;
}

/* name unknown: the fixed heap's class, for the classes based on it */
FixedHeapClass *FixedHeapGetClass(void)
{
	return s_fixedHeapClass;
}

/* name unknown */
int FixedHeapInitClass(FixedHeapClass *cls, const char *name)
{
	int ret;

	ret = UIDHeapClassInit(&cls->heapClass, name, UIDHeapObjectCreate, UIDHeapObjectDestroy,
			       FixedHeapClassAlloc, (int (*)(void *, void *))FixedHeapFreeObject);
	cls->onCarve = NULL;
	cls->setPage = FixedHeapClassSetPage;
	return ret;
}

/* name unknown: the lock, for uid.c */
void FixedHeapInitLock(FixedHeap *heap)
{
	heap->lock = 0;
}

int FixedHeapWriteLock(FixedHeap *heap, const char *func, int line)
{
	return SceCpuForDriver_4C38CE4D(&heap->lock);
}

void FixedHeapWriteUnlock(FixedHeap *heap, int state)
{
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
}

int FixedHeapReadLock(FixedHeap *heap, const char *func, int line)
{
	return SceCpuForDriver_EC53D007(&heap->lock);
}

void FixedHeapReadUnlock(FixedHeap *heap, int state)
{
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
}

int FixedHeapFreeObjectRaw(FixedHeap *heap, void *object)
{
	FixedHeapCallFreeHook(heap, object, __builtin_return_address(0));
#if FW_DEBUG
	int state;

	state = SceCpuForDriver_F02467D1(&heap->lock);
	if (state >= 0) {
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
		SCE_DBG_PRINTF(0xb, 1631, "FATAL ERROR: %p is not LOCKED!!!\n", heap);
		SCE_DBG_ASSERT_FAIL(1632);
		return SCE_KERNEL_ERROR_ERROR;
	}
	if (!FixedHeapHasObject(heap, object)) {
		SCE_DBG_PRINTF(0xb, 1639, "*** Invalid pointer pFixedHeap=%p, pObject=%p\n", heap,
			       object);
		__breakpoint(0);
		return SCE_KERNEL_ERROR_ERROR;
	}
#endif
	if ((heap->attr & FIXED_HEAP_ATTR_CPU_CACHE) && CpuCacheFree(heap, object))
		return 0;
	FixedHeapPushFree(heap, object);
	return 0;
}

int FixedHeapFreeObject(FixedHeap *heap, void *object)
{
	int state;

	FixedHeapCallFreeHook(heap, object, __builtin_return_address(0));
#if FW_DEBUG
	state = SceCpuForDriver_EC53D007(&heap->lock);
	if (!FixedHeapFindPage(heap, object)) {
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
		SCE_DBG_PRINTF(0xb, 1677, "*** Invalid pointer pFixedHeap=%p, pObject=%p\n", heap,
			       object);
		__breakpoint(0);
		return SCE_KERNEL_ERROR_ERROR;
	}
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
#endif
	if ((heap->attr & FIXED_HEAP_ATTR_CPU_CACHE) && CpuCacheFree(heap, object))
		return 0;
	state = SceCpuForDriver_4C38CE4D(&heap->lock);
	FixedHeapPushFree(heap, object);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	return 0;
}

int KernelFixedHeapFreeObject(SceSize size, void *object)
{
	switch (size) {
	case 0x20:
		return FixedHeapFreeObject(g_uidManager->fixedHeap32B, object);
	case 0x30:
		return FixedHeapFreeObject(g_uidManager->fixedHeap48B, object);
	case 0x40:
		return FixedHeapFreeObject(g_uidManager->fixedHeap64B, object);
	case 0x80:
		return FixedHeapFreeObject(g_uidManager->fixedHeap128B, object);
	case 0x100:
		return FixedHeapFreeObject(g_uidManager->fixedHeap256B, object);
	case 0x200:
		return FixedHeapFreeObject(g_uidManager->fixedHeap512B, object);
	}
	SCE_DBG_ASSERT_FAIL(1744);
	return SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_SIZE;
}

/* name unknown: only an allocation for the heap's own partition keeps its opt */
int FixedHeapAllocObjectWithAttr(FixedHeap *heap, struct Partition *partition, SceUInt32 opt,
				 void **object)
{
	int ret;

	if (heap->optPartition != partition)
		opt = 0;
	ret = FixedHeapAllocObjectWithOpt(heap, opt, 0, object);
	if (ret >= 0) {
		FixedHeapObjectHook *hook = heap->objectHook;
		void *entry = *object;

		if (hook != NULL)
			hook->onAlloc(hook->arg, heap, entry, __builtin_return_address(0));
	}
	return ret;
}

/* name unknown: the same as FixedHeapAllocObject */
int FixedHeapAllocObjectExt(FixedHeap *heap, void **object)
{
	int ret;

	ret = FixedHeapAllocObjectWithOpt(heap, 0, 0, object);
	if (ret >= 0)
		FixedHeapCallAllocHook(heap, *object, __builtin_return_address(0));
	return ret;
}

/* name unknown: raw: the caller holds the write lock */
int FixedHeapAllocObjectRaw(FixedHeap *heap, int raw, void **object)
{
	return FixedHeapAllocObjectWithOpt(heap, 0, raw, object);
}

#if !SYSMEM_NSKBL
/* the objects on a free list, stopping at its tail or at a loop */
static inline SceUInt32 FixedHeapCountList(void *head, void *tail)
{
	void **entry = head, *next;
	SceUInt32 count;

	if (entry == NULL)
		return 0;
	count = 1;
	while (entry != tail) {
		next = *entry;
		if (next == entry || next == NULL)
			break;
		count++;
		entry = next;
	}
	return count;
}

int FixedHeapGetInfo(FixedHeap *heap, SceKernelFixedHeapInfo *info)
{
	unsigned int i;
	int state;

	if (info->size != sizeof(*info)) {
		SCE_DBG_ASSERT_FAIL(1761);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	state = SceCpuForDriver_EC53D007(&heap->lock);
	info->itemSize = heap->itemSize;
	info->totalSize = heap->pageSize * heap->pageCount;
	info->perPage = heap->perPage;
	info->freeCount = (SceUInt16)heap->freeCount;
	info->peakCount = heap->peakCount;
	info->cursor = heap->cursor;
	info->usedInPage = heap->usedInPage;
	info->perPage16 = heap->perPage;
	info->freeHead = heap->freeHead;
	info->freeTail = heap->freeTail;
	info->attr = heap->attr;
	info->pageSize = heap->pageSize;
	info->totalCount = heap->totalCount;
	info->freeCount16 = heap->freeCount;
	info->maxPages = heap->maxPages;
	info->pageCount = heap->pageCount;
	info->pages = heap->pages;
	info->currentPage = heap->currentPage;
	info->blocks = heap->blocks;
	info->name = heap->name;
	info->listCount = FixedHeapCountList(heap->freeHead, heap->freeTail);
	for (i = 0; i < FIXED_HEAP_CPUS; i++) {
		info->caches[i].max = heap->caches[i].max;
		info->caches[i].count = heap->caches[i].count;
		info->caches[i].reserved = heap->caches[i].reserved;
		info->caches[i].head = heap->caches[i].head;
		info->caches[i].tail = heap->caches[i].tail;
	}
	SCE_DBG_PRINTF(0xb, 1844, "%p:pFixedHeap->nItemInFifo=0x%08x\n", heap, heap->fifoCount);
	SCE_DBG_PRINTF(0xb, 1846, "%p:pInfo->nListCount      =0x%08x\n", heap, info->listCount);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&heap->lock, state);
	return 0;
}
#endif

#if FW_DEBUG
/* debug builds only (and so 4.00 only here) */
int FixedHeapGetIndexRaw(FixedHeap *heap, const void *object)
{
	unsigned int i;

	for (i = 0; i < heap->pageCount && heap->pages[i] != (void *)-1; i++) {
		const char *page = heap->pages[i];

		if ((const char *)object >= page && (const char *)object < page + heap->pageSize)
			return heap->perPage * i +
			       (SceSize)((const char *)object - page) / heap->itemSize;
	}
	SCE_DBG_PRINTF(0xb, 1881, "<-Failed(pFixedHeap %p, pEntry %p), nPage=%d\n", heap, object,
		       heap->pageCount);
	return SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_INDEX;
}
#endif

/* name unknown: the object at an index */
int FixedHeapGetObjectByIndexRaw(FixedHeap *heap, SceUInt32 index, void **object)
{
	SceUInt32 perPage, page;
	char *base;

	if (object == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	perPage = heap->perPage;
	page = index / perPage;
	if (page >= heap->pageCount)
		return SCE_KERNEL_ERROR_FIXEDHEAP_INDEX_OVERFLOW;
	base = heap->pages[page];
	if (base == (void *)-1) {
		*object = NULL;
		return SCE_KERNEL_ERROR_FIXEDHEAP_INDEX_OVERFLOW;
	}
	*object = base + heap->itemSize * (index % perPage);
	return 0;
}

#if !SYSMEM_NSKBL
/* name unknown: func(object, index, arg) for every object carved, until it returns 1 */
int FixedHeapWalk(FixedHeap *heap, int (*func)(void *object, SceUInt32 index, void *arg),
		  void *arg)
{
	SceUInt32 index = 0, end;
	unsigned int i;
	char *object;
	SceUInt16 count;

	for (i = 0; i < heap->pageCount; i++) {
		object = heap->pages[i];
		count = heap->currentPage == object ? heap->usedInPage : heap->perPage;
		if (count == 0)
			continue;
		end = index + count;
		do {
			if (func(object, index++, arg) == 1)
				return 0;
			object += heap->itemSize;
		} while (index != end);
	}
	return SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_INDEX;
}

/*
 * func(object, index, arg) for the objects from start on, until it returns 1
 * (returns 0), count objects are done (returns the next index) or the page of
 * start ends (returns 0, *next: the index after the last object).
 */
int FixedHeapApplyRangeWithIndexArgRaw(FixedHeap *heap, SceUInt32 start, SceUInt32 count,
				       int (*func)(void *object, SceUInt32 index, void *arg),
				       void *arg, SceUInt32 *next)
{
	SceUInt32 index = 0, base, inPage;
	SceUInt32 limit = start + count;
	unsigned int i;
	char *object;

	*next = heap->perPage * heap->pageCount;
	for (i = 0; i < heap->pageCount; i++) {
		object = heap->pages[i];
		inPage = heap->currentPage == object ? heap->usedInPage : heap->perPage;
		index += inPage;
		if (start > index)
			continue;
		base = i * heap->perPage;
		object += heap->itemSize * (start - base);
		for (index = start; index - base < inPage; object += heap->itemSize) {
			if (func(object, index++, arg) == 1)
				return 0;
			if (index >= limit) {
				SCE_DBG_ASSERT_CHECK(index < 0x7fffffff, 2036);
				return index;
			}
		}
	}
	SCE_DBG_ASSERT_CHECK(index < 0x7fffffff, 2042);
	*next = index;
	return 0;
}
#endif

static __attribute__((noinline)) int FixedHeapInit(FixedHeap *heap, char *name, SceUInt32 attr, SceSize itemSize,
			 SceSize pageSize, void *firstPage, struct Partition *partition, SceUInt16 maxPages,
			 void **pages, void **blocks)
{
	unsigned int i;
	int state;

	SCE_DBG_ASSERT_CHECK(itemSize < 0xffff, 2121);
	if (maxPages > 0x100)
		SCE_DBG_ASSERT_FAIL(2124);
	if (pageSize < 0x1000)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (pageSize & 0xfff) {
		SCE_DBG_ASSERT_FAIL(2138);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (pageSize / itemSize < 8) {
		SCE_DBG_ASSERT_FAIL(2144);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	heap->lock = 0;
	state = SceCpuForDriver_4C38CE4D(&heap->lock);
	heap->magic = FIXED_HEAP_MAGIC;
	heap->name = name;
	heap->attr = attr;
	UIDHeapInit(&heap->uidHeap);
	heap->pageSize = pageSize;
	SCE_DBG_ASSERT_CHECK(itemSize < 0xffff, 2165);
	heap->itemSize = itemSize;
	heap->perPage = heap->pageSize / heap->itemSize;
	heap->maxPages = maxPages;
	heap->totalCount = 0;
	heap->optPartition = partition;
	heap->freeCount = 0;
	heap->peakCount = 0;
	heap->currentPage = firstPage;
	heap->cursor = firstPage;
	heap->usedInPage = 0;
	heap->pages = pages;
	heap->freeHead = NULL;
	heap->freeTail = NULL;
	heap->blocks = blocks;
	pages[0] = firstPage;
	blocks[0] = NULL;
	heap->pageCount = 1;
	for (i = 0; i < FIXED_HEAP_CPUS; i++) {
		heap->caches[i].max = FIXED_HEAP_CACHE_MAX;
		heap->caches[i].count = 0;
		heap->caches[i].head = NULL;
		heap->caches[i].tail = NULL;
	}
	heap->objectHook = NULL;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	return 0;
}

/* name unknown: the same as the class's setPage */
int FixedHeapSetPage(FixedHeap *heap, void *page)
{
	heap->currentPage = page;
	heap->cursor = page;
	heap->usedInPage = 0;
	return 0;
}

#if !SYSMEM_NSKBL

/*
 * A fixed heap of pid's for objects of itemSize bytes, named after name (or
 * the size). Its tables come from the hook, or else from the kernel heap.
 */
int FixedHeapInitForProcess(FixedHeap *heap, SceUID pid, const char *name, SceUInt32 attr,
			    SceSize itemSize, void *partition, SceSize pageSize,
			    SceUInt16 maxPages)
{
	SceKernelHeapHook *hook = heap->hook;
	struct MemBlockObject *smb;
	void **pages, **blocks, *base;
	char *heapName;
	int ret;

	heap->partition = partition;
	if (hook == NULL) {
		if (heap->heap == NULL) {
			SCE_DBG_ASSERT_FAIL(2376);
			return SCE_KERNEL_ERROR_FIXEDHEAP_ERROR;
		}
		pages = HeapAllocLocked(heap->heap, maxPages * sizeof(void *));
		attr |= FIXED_HEAP_ATTR_PROCESS | FIXED_HEAP_ATTR_NAME_HEAP |
			FIXED_HEAP_ATTR_TABLES_HEAP | FIXED_HEAP_ATTR_FROM_HEAP;
		blocks = HeapAllocLocked(heap->heap, maxPages * sizeof(void *));
		heapName = HeapAllocLocked(heap->heap, 0x20);
	} else {
		attr |= FIXED_HEAP_ATTR_PROCESS | FIXED_HEAP_ATTR_NAME_HOOK |
			FIXED_HEAP_ATTR_TABLES_HOOK | FIXED_HEAP_ATTR_FROM_HOOK;
		pages = hook->funcAlloc(hook->arg, maxPages * sizeof(void *));
		blocks = heap->hook->funcAlloc(heap->hook->arg, maxPages * sizeof(void *));
		heapName = heap->hook->funcAlloc(heap->hook->arg, 0x20);
	}
	if (name != NULL)
		snprintf(heapName, 0x20, "SceFixedHeap%sFor0x%08X", name, pid);
	else
		snprintf(heapName, 0x20, "SceFixedHeap%dBFor0x%08X", itemSize, pid);

	ret = AllocSimpleMemBlock(heapName, SMB_TYPE_4K, partition, 1, &smb);
	if (ret < 0)
		SCE_PANIC(2395);
	SceSysmemForKernel_39D5A3B7(smb, &base);
	heap->uidHeap.cls = s_fixedHeapClass;
	ret = FixedHeapInit(heap, heapName, attr, itemSize, pageSize, base, PartitionGetKernel(),
			    maxPages, pages, blocks);
	if (ret < 0) {
		if (smb != NULL)
			SceSysmemForKernel_A4DB88E1(smb);
		hook = heap->hook;
		if (hook != NULL) {
			if (pages != NULL)
				hook->funcFree(hook->arg, pages);
			if (blocks != NULL)
				heap->hook->funcFree(heap->hook->arg, blocks);
			if (heapName != NULL)
				heap->hook->funcFree(heap->hook->arg, heapName);
		} else if (heap->heap != NULL) {
			if (pages != NULL)
				HeapFreeLocked(heap->heap, pages);
			if (blocks != NULL)
				HeapFreeLocked(heap->heap, blocks);
			if (heapName != NULL)
				HeapFreeLocked(heap->heap, heapName);
		}
		return ret;
	}
	heap->attr |= attr;
	blocks[0] = smb;
	return 0;
}

/* name unknown: free the pages and the tables */
int FixedHeapFinish(FixedHeap *heap)
{
	void **blocks = heap->blocks;
	unsigned int i;

	for (i = 0; i < heap->pageCount; i++) {
		if (blocks[i] != NULL)
			SceSysmemForKernel_A4DB88E1(blocks[i]);
	}
	FixedHeapReleaseTables(heap, 1);
	return 0;
}

/* name unknown */
int FixedHeapRelease(FixedHeap *heap)
{
	FixedHeapFinish(heap);
	return 0;
}

int sceKernelGetFixedHeapInfo(SceUID uid, SceKernelFixedHeapInfo *info)
{
	FixedHeap *heap = SceSysmemForKernel_FA6C098C(uid);

	if ((uintptr_t)info == 0) {
		SCE_DBG_ASSERT_FAIL_RELOAD(2492);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	return FixedHeapGetInfo(heap, info);
}

int sceKernelGetFixedHeapInfoByPointer(void *fixedHeap, void *fixedHeapInfo)
{
	if (fixedHeap != NULL) {
		if ((uintptr_t)fixedHeapInfo != 0)
			return FixedHeapGetInfo(fixedHeap, fixedHeapInfo);
		SCE_DBG_ASSERT_CHECK_FUNC_RELOAD(0, 2508, "sceKernelGetFixedHeapInfoByPointer");
	}
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

/* a kernel fixed heap's GUID */
static inline __attribute__((always_inline)) void FixedHeapSetGUID(SceUID guid, FixedHeap *heap)
{
	void *object;
	int ret;

	ret = GUIDSetCNOA(guid, s_fixedHeapClass, heap->name, heap, 0x1551000);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 2642, "FixedHeapSetUID",
				    "GUIDSetCNOA(guid 0x%08x, %p) failed 0x%08x\n", guid, heap, ret);
	ret = sceGUIDGetObject(guid, &object);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 2647, "FixedHeapSetUID",
				    "sceGUIDGetObject failed 0x%08x\n",
				    ret);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2648, "FixedHeapSetUID");
	}
}

/* the fixed heap class, and GUIDs for the kernel fixed heaps made before it */
int FixedHeapSetUID(struct UIDManager *manager)
{
	void *pages, *blocks;
	int ret;

	s_fixedHeapClass = manager->fixedHeapClass;
	ret = FixedHeapInitClass(s_fixedHeapClass, "SceUIDFixedHeapClass");
	if (ret < 0)
		return ret;
	/* two 0x200-byte page tables each, from the 0x200 heap */
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &pages);
	if (ret < 0)
		return ret;
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &blocks);
	if (ret < 0)
		return ret;
	FixedHeapReplaceTables(g_uidManager->fixedHeap32B, FIXED_HEAP_ATTR_TABLES_KERNEL, pages, blocks,
			       0x80);
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &pages);
	if (ret < 0)
		return ret;
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &blocks);
	if (ret < 0)
		return ret;
	FixedHeapReplaceTables(g_uidManager->fixedHeap64B, FIXED_HEAP_ATTR_TABLES_KERNEL, pages, blocks,
			       0x80);
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &pages);
	if (ret < 0)
		return ret;
	ret = FixedHeapAllocObject(g_uidManager->fixedHeap512B, &blocks);
	if (ret < 0)
		return ret;
	FixedHeapReplaceTables(g_uidManager->fixedHeapL2Object, FIXED_HEAP_ATTR_TABLES_KERNEL, pages, blocks,
			       0x80);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_16B_ID, g_uidManager->fixedHeap16B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_32B_ID, g_uidManager->fixedHeap32B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_48B_ID, g_uidManager->fixedHeap48B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_64B_ID, g_uidManager->fixedHeap64B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_128B_ID, g_uidManager->fixedHeap128B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_256B_ID, g_uidManager->fixedHeap256B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_512B_ID, g_uidManager->fixedHeap512B);
	FixedHeapSetGUID(SCE_KERNEL_FIXED_HEAP_L2_OBJECT_ID, g_uidManager->fixedHeapL2Object);
	return ret;
}
#endif

#if FW_DEBUG
/* FixedHeapStart: this firmware's link keeps only its record (no code reaches it) */
SCE_DBGINFO_DEFINE(static, fixedHeapStartRecord, 2911, "FixedHeapStart");
#endif

/* sceKernelGetKernelFixedHeap */
FixedHeap *SceSysmemForKernel_3717C322(SceSize size)
{
	switch (size) {
	case 0x20:
		return g_uidManager->fixedHeap32B;
	case 0x30:
		return g_uidManager->fixedHeap48B;
	case 0x40:
		return g_uidManager->fixedHeap64B;
	case 0x80:
		return g_uidManager->fixedHeap128B;
	case 0x100:
		return g_uidManager->fixedHeap256B;
	case 0x200:
		return g_uidManager->fixedHeap512B;
	}
	return NULL;
}

/* sceKernelFixedHeapAllocObject */
void *SceSysmemForKernel_ABA5313C(FixedHeap *heap)
{
	void *object;

	if (FixedHeapAllocObject(heap, &object) < 0)
		return NULL;
	return object;
}

/* sceKernelFixedHeapFreeObject */
int SceSysmemForKernel_93344755(FixedHeap *heap, void *object)
{
	return FixedHeapFreeObject(heap, object);
}

#if !SYSMEM_NSKBL
/* sceKernelFixedHeapSetHookByPointer */
int SceSysmemForKernel_811C452B(FixedHeap *heap, FixedHeapObjectHook *hook)
{
	int state;

	state = SceCpuForDriver_4C38CE4D(&heap->lock);
	heap->objectHook = hook;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	return 0;
}

int sceKernelFixedHeapSetHook(SceUID uid, FixedHeapObjectHook *hook)
{
	SceObjectBase *object;
	FixedHeap *heap;
	int state;

	if (sceGUIDGetObjectWithClass(uid, &s_fixedHeapClass->heapClass.cls, &object) < 0) {
		SCE_DBG_PRINTF(0xb, 3112, "There is no 0x%08x\n", uid);
		return SCE_KERNEL_ERROR_INVALID_UID;
	}
	heap = (FixedHeap *)object;
	state = SceCpuForDriver_4C38CE4D(&heap->lock);
	heap->objectHook = hook;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&heap->lock, state);
	return 0;
}
#endif
