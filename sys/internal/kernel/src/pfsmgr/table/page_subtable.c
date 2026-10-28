/*
 * A page of icvs: the array of icvs that fills it.
 */

#include "pfsmgr_internal.h"

#define PFS_HASH_PAGE_SIZE		0x400	/* name unknown: the page size icv pages are made for */

#define PFS_HASH_LIST_SIZE		0x3f8	/* name unknown: bytes the icvs of a 0x400 byte page use */


void pfsHashListAttach(ScePfsHashList *list, void *data)
{
	list->data = data;
	pfsHashArrayAttach(&list->hashes, data);
}


void pfsHashListInit(ScePfsHashList *list, void *data, SceSize size)
{
	list->data = data;
	pfsHashArrayInit(&list->hashes, PFS_HASH_SIZE, data,
			 (size - sizeof(ScePfsHashArrayHeader)) / PFS_HASH_SIZE * PFS_HASH_SIZE +
			 sizeof(ScePfsHashArrayHeader));
}


int pfsHashListGet(ScePfsHashList *list, SceUInt32 index, ScePfsHash *icv)
{
	if (index >= list->hashes.header->count)
		return SCE_PFS_ERROR_HASH_LIST_INDEX;
	icv_copy(icv, &list->hashes.items[index]);
	return 0;
}


int pfsHashListSet(ScePfsHashList *list, SceUInt32 index, const ScePfsHash *icv)
{
	if (index >= list->hashes.header->count)
		return SCE_PFS_ERROR_HASH_LIST_INDEX;
	icv_copy(&list->hashes.items[index], icv);
	return 0;
}


int pfsHashListAppend(ScePfsHashList *list, const ScePfsHash *icv)
{
	int ret;

	ret = pfsHashArrayPush(&list->hashes, PFS_HASH_PAGE_SIZE, PFS_HASH_SIZE);
	if (ret == 0)
		icv_copy(&list->hashes.items[list->hashes.header->count - 1], icv);
	return ret;
}


int pfsHashListRemoveLast(ScePfsHashList *list)
{
	return pfsHashArrayPop(&list->hashes);
}


static inline int pfsHashListValid(ScePfsHashList *list)	/* name unknown */
{
	return list->hashes.header->size == PFS_HASH_LIST_SIZE &&
	       list->hashes.header->itemSize == PFS_HASH_SIZE;
}


void pfsHashPageInit(ScePfsHashPage *page, ScePfsPageAllocator *pages)
{
	page->pages = pages;
	page->offset = -1;
	page_acc_init(&page->acc, pages->lru, pages->lf, pages->base);
}


int pfsHashPageAlloc(ScePfsHashPage *page)
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
	pfsHashListInit(&page->list, page->acc.entry->data, page->acc.lru->pageSize);
	page->acc.entry->flags |= PFS_LRU_DIRTY;
	if (!pfsHashListValid(&page->list)) {
		page_acc_unlock(&page->acc);
		pfsPageAllocatorFree(page->pages, page->offset, 1);
		page->offset = -1;
		return SCE_PFS_ERROR_UNICV_PAGE_SIZE;
	}
	return 0;
}


int pfsHashPageLoad(ScePfsHashPage *page, SceOff offset, int raw)
{
	int ret;

	ret = page_acc_lock(&page->acc, offset);
	if (ret != 0)
		return ret;
	page->offset = offset;
	pfsHashListAttach(&page->list, page->acc.entry->data);
	if (!raw && !pfsHashListValid(&page->list)) {
		page_acc_unlock(&page->acc);
		page->offset = -1;
		return SCE_PFS_ERROR_UNICV_PAGE_SIZE;
	}
	return 0;
}


int pfsHashPageRelease(ScePfsHashPage *page)
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


int pfsHashPageTerm(ScePfsHashPage *page)
{
	int ret;

	ret = pfsHashPageRelease(page);
	if (ret == 0)
		page_acc_fini(&page->acc);
	return ret;
}


int pfsHashPageFree(ScePfsHashPage *page)
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
