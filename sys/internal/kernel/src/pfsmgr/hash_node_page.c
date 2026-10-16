/*
 * Pages that hold the nodes of icv trees, from a page allocator.
 */

#include "pfsmgr_internal.h"

int pfsHashNodePageCheck(ScePfsHashNodePage *page)
{
	if (page->view.hashes.header->itemSize == PFS_HASH_SIZE &&
	    page->view.hashes.header->size ==
	    (2 * pfsHashNodeFanout(0x400) - 1) * PFS_HASH_SIZE + sizeof(ScePfsHashArrayHeader))
		return 0;
	return SCE_PFS_ERROR_NODE_PAGE;
}

void pfsHashNodePageInit(ScePfsHashNodePage *page, ScePfsPageAllocator *pages)
{
	page->pages = pages;
	page->offset = -1;
	page_acc_init(&page->acc, pages->lru, pages->lf, pages->base);
}

int pfsHashNodePageAlloc(ScePfsHashNodePage *page)
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
	pfsHashNodeViewInit(&page->view, page->acc.entry->data, page->acc.lru->pageSize);
	page->acc.entry->flags |= PFS_LRU_DIRTY;
	ret = pfsHashNodePageCheck(page);
	if (ret != 0) {
		page_acc_unlock(&page->acc);
		pfsPageAllocatorFree(page->pages, page->offset, 1);
		page->offset = -1;
	}
	return ret;
}

int pfsHashNodePageLoad(ScePfsHashNodePage *page, SceOff offset)
{
	int ret;

	ret = page_acc_lock(&page->acc, offset);
	if (ret != 0)
		return ret;
	page->offset = offset;
	pfsHashNodeViewAttach(&page->view, page->acc.entry->data);
	ret = pfsHashNodePageCheck(page);
	if (ret != 0) {
		page_acc_unlock(&page->acc);
		page->offset = -1;
		return ret;
	}
	return 0;
}

int pfsHashNodePageRelease(ScePfsHashNodePage *page)
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

int pfsHashNodePageFree(ScePfsHashNodePage *page)
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
