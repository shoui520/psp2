/*
 * The page allocator of files kept in pages (unicv.db, files.db): pages are
 * handed out from the end of the file, and freed runs of pages are kept in a
 * ring, in offset order, whose links live in the free pages themselves.
 */

#include "pfsmgr_internal.h"

static inline ScePfsFreeRun *pfsFreeRunOf(ScePfsPageAcc *acc)	/* name unknown */
{
	return acc->entry->data;
}

static inline void pfsPageAccDirty(ScePfsPageAcc *acc)	/* name unknown */
{
	acc->entry->flags |= PFS_LRU_DIRTY;
}

/* take the run acc holds out of the ring; *other is a run left, -1 if none */
int pfsFreeRunUnlink(SceOff *other, ScePfsPageAcc *acc)
{
	ScePfsFreeRun *run = pfsFreeRunOf(acc);
	SceOff self = run->offset;
	SceOff prev, next;
	int ret;

	if (run->prev == self) {
		*other = -1;
		return 0;
	}
	*other = run->prev;
	prev = pfsFreeRunOf(acc)->prev;
	next = pfsFreeRunOf(acc)->next;
	ret = page_acc_lock(acc, prev);
	if (ret != 0)
		return ret;
	pfsPageAccDirty(acc);
	pfsFreeRunOf(acc)->next = next;
	ret = page_acc_lock(acc, next);
	if (ret != 0)
		return ret;
	pfsPageAccDirty(acc);
	pfsFreeRunOf(acc)->prev = prev;
	return page_acc_lock(acc, self);
}

/* put the run acc holds into the ring, in front of the one before holds */
int pfsFreeRunInsertBefore(ScePfsPageAcc *acc, ScePfsPageAcc *before)
{
	ScePfsFreeRun *run, *next = pfsFreeRunOf(before);
	SceOff self = next->offset, prev = next->prev;
	int ret;

	pfsPageAccDirty(acc);
	pfsFreeRunOf(acc)->next = next->offset;
	pfsPageAccDirty(before);
	next->prev = pfsFreeRunOf(acc)->offset;
	if (prev == self) {
		pfsPageAccDirty(acc);
		run = pfsFreeRunOf(acc);
		pfsPageAccDirty(before);
		run->prev = next->offset;
		next->next = pfsFreeRunOf(acc)->offset;
		return 0;
	}
	ret = page_acc_lock(before, prev);
	if (ret != 0)
		return ret;
	pfsPageAccDirty(acc);
	run = pfsFreeRunOf(acc);
	next = pfsFreeRunOf(before);
	pfsPageAccDirty(before);
	run->prev = next->offset;
	next->next = pfsFreeRunOf(acc)->offset;
	return page_acc_lock(before, self);
}

/* add the run next holds, which follows acc's on disk, to acc's */
int pfsFreeRunMerge(ScePfsPageAcc *acc, ScePfsPageAcc *next)
{
	ScePfsFreeRun *run = pfsFreeRunOf(acc), *n;
	SceOff self = run->offset;
	int ret;

	if (pfsFreeRunOf(next)->next == self) {
		pfsPageAccDirty(acc);
		run->count += pfsFreeRunOf(next)->count;
		run->next = self;
		run->prev = self;
		return 0;
	}
	pfsPageAccDirty(acc);
	n = pfsFreeRunOf(next);
	run->count += n->count;
	ret = page_acc_lock(next, n->next);
	if (ret != 0)
		return ret;
	pfsPageAccDirty(acc);
	run = pfsFreeRunOf(acc);
	n = pfsFreeRunOf(next);
	pfsPageAccDirty(next);
	run->next = n->offset;
	n->prev = pfsFreeRunOf(acc)->offset;
	return 0;
}

void pfsPageAllocatorGet(ScePfsPageAllocator *pages, SceUInt64 *freeHead, SceUInt64 *end)
{
	*freeHead = pages->freeHead;
	*end = pages->end;
}

void pfsPageAllocatorSet(ScePfsPageAllocator *pages, SceUInt64 freeHead, SceUInt64 end)
{
	pages->freeHead = freeHead;
	pages->end = end;
}

void pfsPageAllocatorInit(ScePfsPageAllocator *pages, ScePfsLru *lru, ScePfsLowerFile *lf,
			  SceOff base)
{
	pages->base = base;
	pages->lru = lru;
	pages->lf = lf;
	pages->freeHead = -1;
	pages->end = 0;
}

void pfsPageAllocatorTerm(ScePfsPageAllocator *pages)
{
	pages->lru = NULL;
	pages->lf = NULL;
	pages->freeHead = -1;
	pages->end = 0;
	pages->base = 0;
}

/* whether the page at offset is handed out */
int pfsPageAllocatorIsUsed(ScePfsPageAllocator *pages, int *used, SceOff offset)
{
	ScePfsPageAcc acc;
	ScePfsFreeRun *run;
	SceOff cur;
	int ret;

	offset -= offset % pages->lru->pageSize;
	if (offset >= pages->end) {
		*used = 0;
		return 0;
	}
	*used = 1;
	cur = pages->freeHead;
	if (cur == -1 || offset < cur)
		return 0;
	page_acc_init(&acc, pages->lru, pages->lf, pages->base);
	for (;;) {
		ret = page_acc_lock(&acc, cur);
		if (ret != 0)
			return ret;
		run = pfsFreeRunOf(&acc);
		if (offset >= run->offset &&
		    offset < run->offset + pages->lru->pageSize * run->count) {
			*used = 0;
			break;
		}
		cur = run->next;
		if (offset < cur || cur == pages->freeHead)
			break;
	}
	page_acc_unlock(&acc);
	return 0;
}

/* count pages in a row: the first free run large enough, else the end of the file */
int pfsPageAllocatorAlloc(ScePfsPageAllocator *pages, SceOff *offset, SceUInt32 count)
{
	ScePfsPageAcc acc, rest;
	ScePfsFreeRun *run;
	SceOff cur, restOffset, head;
	int ret;

	cur = pages->freeHead;
	*offset = -1;
	page_acc_init(&acc, pages->lru, pages->lf, pages->base);
	if (cur != -1) {
		do {
			ret = page_acc_lock(&acc, cur);
			if (ret != 0)
				return ret;
			run = pfsFreeRunOf(&acc);
			if (count > run->count) {
				cur = run->next;
				continue;
			}
			*offset = cur;
			ret = pfsFreeRunUnlink(&cur, &acc);
			if (ret != 0)
				goto unlock;
			if (count < pfsFreeRunOf(&acc)->count) {
				/* the rest of the run stays free */
				restOffset = *offset + pages->lru->pageSize * count;
				page_acc_init(&rest, pages->lru, pages->lf, pages->base);
				ret = page_acc_lock(&rest, restOffset);
				if (ret != 0)
					goto unlock;
				run = pfsFreeRunOf(&rest);
				pfsPageAccDirty(&rest);
				run->count = pfsFreeRunOf(&acc)->count - count;
				run->offset = restOffset;
				run->prev = restOffset;
				run->next = restOffset;
				if (cur == -1) {
					pages->freeHead = restOffset;
				} else {
					ret = page_acc_lock(&acc, pfsFreeRunOf(&acc)->prev);
					if (ret != 0 || (ret = pfsFreeRunInsertBefore(&acc, &rest)) != 0) {
						page_acc_unlock(&acc);
						page_acc_unlock(&rest);
						return ret;
					}
				}
				page_acc_unlock(&rest);
			}
			if (pages->freeHead == *offset) {
				head = cur;
				if (cur != -1) {
					ret = page_acc_lock(&acc, cur);
					if (ret != 0)
						goto unlock;
					head = pfsFreeRunOf(&acc)->next;
				}
				pages->freeHead = head;
			}
			break;
		} while (cur != pages->freeHead);
		page_acc_unlock(&acc);
	}
	if (*offset == -1) {
		*offset = pages->end;
		pages->end += pages->lru->pageSize * count;
		return file_access_truncate(pages->lf, pages->end + pages->base);
	}
	return 0;

unlock:
	page_acc_unlock(&acc);
	return ret;
}

/* free count pages at offset, merging them with free runs next to them */
int pfsPageAllocatorFree(ScePfsPageAllocator *pages, SceOff offset, SceUInt32 count)
{
	ScePfsPageAcc acc, other;
	ScePfsFreeRun *run;
	SceOff cur, prev, head;
	int ret;

	page_acc_init(&acc, pages->lru, pages->lf, pages->base);
	ret = page_acc_lock(&acc, offset);
	if (ret != 0)
		return ret;
	cur = pages->freeHead;
	pfsPageAccDirty(&acc);
	run = pfsFreeRunOf(&acc);
	run->offset = offset;
	run->prev = offset;
	run->next = offset;
	run->count = count;
	if (cur == -1) {
		pages->freeHead = offset;
		goto trim;
	}

	page_acc_init(&other, pages->lru, pages->lf, pages->base);
	while ((ret = page_acc_lock(&other, cur)) == 0) {
		prev = pfsFreeRunOf(&other)->prev;
		if ((offset < cur && prev < offset) ||
		    (prev >= cur && (offset < cur || prev < offset))) {
			ret = pfsFreeRunInsertBefore(&acc, &other);
			if (ret != 0) {
				page_acc_unlock(&acc);
				page_acc_unlock(&other);
				return ret;
			}
			page_acc_unlock(&other);
			if (offset < pages->freeHead)
				pages->freeHead = offset;
			if (prev < offset) {
				ret = page_acc_lock(&other, prev);
				if (ret != 0)
					break;
				run = pfsFreeRunOf(&other);
				if (run->offset + pages->lru->pageSize * run->count ==
				    pfsFreeRunOf(&acc)->offset) {
					ret = pfsFreeRunMerge(&other, &acc);
					offset = prev;
					if (ret != 0) {
						page_acc_unlock(&acc);
						page_acc_unlock(&other);
						return ret;
					}
				}
				page_acc_unlock(&other);
			}
			if (cur == prev)
				goto trim;
			ret = page_acc_lock(&acc, offset);
			if (ret != 0) {
				page_acc_unlock(&acc);
				return ret;
			}
			ret = page_acc_lock(&other, cur);
			if (ret != 0)
				goto unlock;
			run = pfsFreeRunOf(&acc);
			if (run->offset + pages->lru->pageSize * run->count ==
			    pfsFreeRunOf(&other)->offset &&
			    (ret = pfsFreeRunMerge(&acc, &other)) != 0) {
				page_acc_unlock(&acc);
				page_acc_unlock(&other);
				return ret;
			}
			page_acc_unlock(&other);
			goto trim;
		}
		cur = pfsFreeRunOf(&other)->next;
	}
	page_acc_unlock(&acc);
	return ret;

trim:
	/* a free run at the end of the file is given back */
	page_acc_unlock(&acc);
	ret = page_acc_lock(&acc, offset);
	if (ret != 0)
		return ret;
	run = pfsFreeRunOf(&acc);
	count = run->count;
	if (run->offset + pages->lru->pageSize * count == pages->end) {
		ret = pfsFreeRunUnlink(&head, &acc);
		if (ret != 0)
			goto unlock;
		if (head == -1)
			pages->freeHead = head;
		pages->end -= pages->lru->pageSize * count;
		ret = file_access_truncate(pages->lf, pages->base + pages->end);
		if (ret != 0)
			goto unlock;
	}
	page_acc_unlock(&acc);
	return 0;

unlock:
	page_acc_unlock(&acc);
	return ret;
}
