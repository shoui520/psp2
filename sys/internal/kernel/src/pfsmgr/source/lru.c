/*
 * An LRU cache of fixed-size pages of underlying files, hashed by file and
 * offset. Entries in use are off the LRU list; free ones are taken from its
 * head, and valid ones go back at its tail.
 */

#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_LRU_FULL		0x80140504	/* name unknown */
#define SCE_PFS_ERROR_LRU_OTHER_FILE	0x80140506	/* name unknown */
#define SCE_PFS_ERROR_LRU_ALIGN		0x80140509	/* name unknown */
#define SCE_PFS_ERROR_LRU_READ		0x8014050a	/* name unknown */
#define SCE_PFS_ERROR_LRU_NOT_BUSY	0x8014050c	/* name unknown */

ScePfsBufferList g_pfsLruPoolBufferList;
ScePfsBufferList g_pfsLruTblHdrBufferList;

static inline SceUInt32 pfsLruBucket(ScePfsLru *lru, SceUInt32 cookie, SceOff offset)
{
	return (SceUInt32)(offset / lru->pageSize + cookie) % lru->bucketCount;
}

static inline void pfsLruUnlink(ScePfsLru *lru, ScePfsLruEntry *e)	/* name unknown */
{
	if (lru->head == lru->tail) {
		lru->head = NULL;
		lru->tail = NULL;
	} else if (lru->head == e) {
		lru->head = e->lruNext;
		lru->head->lruPrev = NULL;
	} else if (lru->tail == e) {
		lru->tail = e->lruPrev;
		lru->tail->lruNext = NULL;
	} else {
		e->lruPrev->lruNext = e->lruNext;
		e->lruNext->lruPrev = e->lruPrev;
	}
	e->lruPrev = NULL;
	e->lruNext = NULL;
}

void pfsLruInitTable(ScePfsLru *lru, SceUInt32 bucketCount, SceSize pageSize,
		     SceUInt32 entryCount, ScePfsLruEntry **buckets, void *pool)
{
	ScePfsLruEntry *e;
	SceUInt32 i;

	lru->bucketCount = bucketCount;
	lru->pageSize = pageSize;
	lru->entryCount = entryCount;
	lru->pool = pool;
	lru->poolStart = pool;
	lru->head = NULL;
	lru->tail = NULL;
	lru->poolEnd = (char *)pool + entryCount * pageSize;
	lru->buckets = buckets;
	lru->entries = (ScePfsLruEntry *)(buckets + bucketCount);

	for (i = 0; i < entryCount; i++) {
		e = &lru->entries[i];
		e->prev = NULL;
		e->next = NULL;
		e->lruPrev = NULL;
		e->lruNext = NULL;
		e->cookie = 0;
		e->offset = 0;
		e->data = NULL;
		e->flags = 0;
	}
	for (i = 0; i < entryCount; i++)
		lru->entries[i].data = (char *)pool + i * pageSize;
	for (i = 0; i < bucketCount; i++)
		lru->buckets[i] = NULL;

	lru->head = &lru->entries[0];
	lru->entries[0].lruPrev = NULL;
	for (i = 1; i < entryCount; i++) {
		lru->entries[i - 1].lruNext = &lru->entries[i];
		lru->entries[i].lruPrev = &lru->entries[i - 1];
	}
	lru->tail = &lru->entries[entryCount - 1];
	lru->tail->lruNext = NULL;
}

void pfsLruTermTable(ScePfsLru *lru, ScePfsLruEntry ***buckets, void **pool)
{
	if (buckets != NULL)
		*buckets = lru->buckets;
	if (pool != NULL)
		*pool = lru->pool;
	lru->entries = NULL;
	lru->pool = NULL;
	lru->buckets = NULL;
	lru->bucketCount = 0;
	lru->pageSize = 0;
	lru->entryCount = 0;
	lru->head = NULL;
	lru->tail = NULL;
}

static inline int pfsLruPutBack(ScePfsLru *lru, ScePfsLruEntry *e)	/* name unknown */
{
	ScePfsLruEntry *end;

	if (!(e->flags & PFS_LRU_BUSY))
		return SCE_PFS_ERROR_LRU_NOT_BUSY;
	e->flags ^= PFS_LRU_BUSY;
	if (!(e->flags & PFS_LRU_VALID)) {
		end = lru->head;
		if (end != NULL) {
			end->lruPrev = e;
			e->lruPrev = NULL;
			e->lruNext = end;
			lru->head = e;
			return 0;
		}
	} else {
		end = lru->tail;
		if (end != NULL) {
			end->lruNext = e;
			e->lruPrev = end;
			e->lruNext = NULL;
			lru->tail = e;
			return 0;
		}
	}
	e->lruPrev = NULL;
	e->lruNext = NULL;
	lru->head = e;
	lru->tail = e;
	return 0;
}

int pfsLruRelease(ScePfsLru *lru, ScePfsLruEntry *e)
{
	return pfsLruPutBack(lru, e);
}

/* the page at base + offset of lf, read in if not cached; it stays busy until released */
int pfsLruGet(ScePfsLru *lru, ScePfsLruEntry **out, ScePfsLowerFile *lf, SceUInt32 unused,
	      SceOff base, SceOff offset)
{
	ScePfsLruEntry *e, *next;
	SceUInt32 cookie, b;
	SceUInt64 size;
	SceSize n;
	SceOff key;
	int ret;

	if (offset % lru->pageSize != 0)
		return SCE_PFS_ERROR_LRU_ALIGN;
	key = offset + base;
	cookie = pfsLowerCookie(lf);
again:
	for (e = lru->buckets[pfsLruBucket(lru, cookie, key)]; e != NULL; e = e->next) {
		if (cookie != e->cookie || e->offset != key)
			continue;
		if (e->flags & PFS_LRU_BUSY)
			goto again;
		e->flags |= PFS_LRU_BUSY;
		pfsLruUnlink(lru, e);
		if (e->flags & PFS_LRU_VALID)
			goto found;
		goto read;
	}

	e = lru->head;
	if (e == NULL)
		return SCE_PFS_ERROR_LRU_FULL;
	pfsLruUnlink(lru, e);
	if ((e->flags & (PFS_LRU_VALID | PFS_LRU_DIRTY)) == (PFS_LRU_VALID | PFS_LRU_DIRTY)) {
		if (e->cookie != pfsLowerCookie(lf))
			return SCE_PFS_ERROR_LRU_OTHER_FILE;
		pfsLowerGetSize(lf, &size);
		if ((SceUInt64)e->offset < size) {
			ret = pfsLowerPwrite(lf, e->data, lru->pageSize, e->offset, &n);
			if (ret != 0)
				return ret;
		}
	}
	if (e->flags != 0) {
		if (e->prev == NULL) {
			b = pfsLruBucket(lru, e->cookie, e->offset);
			if (lru->buckets[b] == e) {
				lru->buckets[b] = e->next;
				if (lru->buckets[b] != NULL)
					lru->buckets[b]->prev = NULL;
			}
		} else if (e->next == NULL) {
			e->prev->next = NULL;
		} else {
			e->prev->next = e->next;
			e->next->prev = e->prev;
		}
		e->prev = NULL;
		e->next = NULL;
	}
	e->flags = PFS_LRU_BUSY;
	e->cookie = cookie;
	e->offset = key;
	b = pfsLruBucket(lru, cookie, key);
	next = lru->buckets[b];
	lru->buckets[b] = e;
	e->prev = NULL;
	e->next = next;
	if (next != NULL)
		next->prev = e;

read:
	if ((char *)e->data < (char *)lru->poolStart ||
	    (char *)e->data + lru->pageSize > (char *)lru->poolEnd)
		return SCE_PFS_ERROR_LRU_READ;
	ret = pfsLowerPread(lf, e->data, lru->pageSize, e->offset, &n);
	if (ret != 0) {
		if (e->flags & PFS_LRU_BUSY)
			pfsLruPutBack(lru, e);
		return ret;
	}
	if (n < lru->pageSize) {
		if (e->flags & PFS_LRU_BUSY)
			pfsLruPutBack(lru, e);
		return SCE_PFS_ERROR_LRU_READ;
	}
	e->flags |= PFS_LRU_VALID;
found:
	*out = e;
	return 0;
}

/* write back lf's dirty pages, and with invalidate drop them */
int pfsLruFlush(ScePfsLru *lru, ScePfsLowerFile *lf, int invalidate)
{
	ScePfsLruEntry *e, *next;
	SceUInt64 size;
	SceSize n;
	int ret;

	for (e = lru->head; e != NULL; e = next) {
		next = e->lruNext;
		if (e->cookie != pfsLowerCookie(lf) || !(e->flags & PFS_LRU_VALID))
			continue;
		if (e->flags & PFS_LRU_DIRTY) {
			pfsLowerGetSize(lf, &size);
			if ((SceUInt64)e->offset < size) {
				ret = pfsLowerPwrite(lf, e->data, lru->pageSize, e->offset, &n);
				if (ret != 0)
					return ret;
			}
			e->flags ^= PFS_LRU_DIRTY;
		}
		if (invalidate) {
			pfsLruUnlink(lru, e);
			e->flags = 0;
			if (lru->head == NULL) {
				lru->head = e;
				lru->tail = e;
			} else {
				lru->head->lruPrev = e;
				e->lruPrev = NULL;
				e->lruNext = lru->head;
				lru->head = e;
			}
		}
	}
	return 0;
}

int pfsLruInit(void)
{
	int ret;

	ret = pfsBufferListInit(&g_pfsLruTblHdrBufferList, "ScePfsLruTblHdrBufferList",
				PFS_LRU_TBL_HDR_SIZE, 4, PFS_LRU_COUNT);
	if (ret != 0)
		return ret;
	ret = pfsBufferListInit(&g_pfsLruPoolBufferList, "ScePfsLruPoolBufferList",
				PFS_LRU_POOL_SIZE, 0x40, PFS_LRU_COUNT);
	if (ret != 0) {
		pfsBufferListTerm(&g_pfsLruTblHdrBufferList);
		return ret;
	}
	return 0;
}

void pfsLruTerm(void)
{
	pfsBufferListTerm(&g_pfsLruTblHdrBufferList);
	pfsBufferListTerm(&g_pfsLruPoolBufferList);
}
