/*
 * bufmgr/page_acc.c: holds one page of a file at a time through an ScePfsLru;
 * locking another page releases the one held.
 */

#include <sce/debug.h>
#include "pfsmgr_internal.h"

void page_acc_init(ScePfsPageAcc *acc, ScePfsLru *lru, ScePfsLowerFile *lf, SceOff base)
{
	acc->base = base;
	acc->lru = lru;
	acc->lf = lf;
	acc->entry = NULL;
	acc->offset = 0;
}

void page_acc_fini(ScePfsPageAcc *acc)
{
	acc->lru = NULL;
	acc->lf = NULL;
	acc->entry = NULL;
	acc->offset = 0;
	acc->base = 0;
}

int page_acc_unlock(ScePfsPageAcc *acc)
{
	int ret;

	if (acc->lru->buckets == NULL)
		SCE_PANIC(0x59);
	if (acc->entry == NULL)
		SCE_PANIC(0x5e);
	ret = pfsLruRelease(acc->lru, acc->entry);
	if (ret == 0)
		acc->entry = NULL;
	return ret;
}

int page_acc_lock(ScePfsPageAcc *acc, SceOff offset)
{
	int ret;

	if (offset < 0)
		return SCE_PFS_ERROR_PAGE_OFFSET;
	if (acc->lru->buckets == NULL)
		SCE_PANIC(0x35);
	acc->offset = offset - offset % acc->lru->pageSize;
	if (acc->entry != NULL) {
		if ((acc->entry->flags & PFS_LRU_VALID) &&
		    acc->entry->offset == acc->base + acc->offset)
			return 0;
		ret = page_acc_unlock(acc);
		if (ret != 0)
			return ret;
	}
	return pfsLruGet(acc->lru, &acc->entry, acc->lf, 0, acc->base, acc->offset);
}
