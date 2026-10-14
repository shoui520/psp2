/*
 * Pages that hold the nodes of files.db trees, from a page allocator.
 */

#include "pfsmgr_internal.h"

void pfsDbNodePageInit(ScePfsDbNodePage *page, ScePfsPageAllocator *pages)
{
	page->pages = pages;
	page->offset = -1;
	page_acc_init(&page->acc, pages->lru, pages->lf, pages->base);
}

int pfsDbNodePageAlloc(ScePfsDbNodePage *page)
{
	SceOff offset;
	int ret;

	ret = pfsPageAllocatorAlloc(page->pages, &offset, 1);
	if (ret != 0)
		return ret;
	ret = page_acc_lock(&page->acc, offset);
	if (ret != 0) {
		pfsPageAllocatorFree(page->pages, offset, 1);
		return ret;
	}
	page->offset = offset;
	memset(page->acc.entry->data, 0, page->acc.lru->pageSize);
	page->acc.entry->flags |= PFS_LRU_DIRTY;
	return 0;
}

int pfsDbNodePageLoad(ScePfsDbNodePage *page, SceOff offset, SceUInt32 order)
{
	ScePfsDbNode *node;
	int used, ret;

	ret = pfsPageAllocatorIsUsed(page->pages, &used, offset);
	if (ret != 0)
		return ret;
	if (!used)
		return SCE_PFS_ERROR_PAGE_UNUSED;
	ret = page_acc_lock(&page->acc, offset);
	if (ret != 0)
		return ret;
	page->offset = offset;
	node = page->acc.entry->data;
	if (node->internal < 2) {
		if (node->count == 0) {
			if (node->parent == -1)
				return 0;
		} else if (node->count < order) {
			return 0;
		}
	}
	page_acc_unlock(&page->acc);
	page->offset = -1;
	return SCE_PFS_ERROR_DB_NODE;
}

int pfsDbNodePageRelease(ScePfsDbNodePage *page)
{
	int ret;

	if (page->offset == -1)
		return 0;
	ret = page_acc_unlock(&page->acc);
	if (ret != 0)
		return ret;
	page->offset = -1;
	return 0;
}

int pfsDbNodePageFree(ScePfsDbNodePage *page)
{
	int ret;

	if (page->offset == -1)
		return 0;
	page->acc.entry->flags &= ~PFS_LRU_VALID;
	ret = page_acc_unlock(&page->acc);
	if (ret != 0)
		return ret;
	ret = pfsPageAllocatorFree(page->pages, page->offset, 1);
	if (ret == 0) {
		page->offset = -1;
		return 0;
	}
	page_acc_lock(&page->acc, page->offset);
	return ret;
}
