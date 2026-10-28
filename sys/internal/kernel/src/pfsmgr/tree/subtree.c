/*
 * Arrays of hashes at the start of a page: a header, then the items.
 */

#include "pfsmgr_internal.h"


/* the depth of the item x - 1 of a complete binary tree, and its place in that row */
void pfsHashLog2(SceUInt32 x, SceUInt32 *depth, SceUInt32 *rest)
{
	SceUInt32 bits, top, n;

	*depth = 0;
	if (x == 0) {
		bits = -1;
		top = 0;
	} else {
		bits = 0;
		for (n = x >> 1; n != 0; n >>= 1)
			bits++;
		top = 1 << bits;
	}
	*depth = bits;
	*rest = x ^ top;
}


__attribute__((noinline)) void pfsHashArrayAttach(ScePfsHashArray *array, void *data)
{
	array->header = data;
	array->items = (void *)((char *)data + sizeof(ScePfsHashArrayHeader));
}


__attribute__((noinline)) void pfsHashArrayInit(ScePfsHashArray *array, SceSize itemSize, void *data, SceSize size)
{
	ScePfsHashArrayHeader *header = data;

	array->header = header;
	array->items = (void *)(header + 1);
	header->count = 0;
	header->size = size;
	header->itemSize = itemSize;
}


/* the depth of the last item, and the items in the last row */
void pfsHashArrayDepth(ScePfsHashArray *array, SceUInt32 *depth, SceUInt32 *rest)
{
	SceUInt32 count = array->header->count;
	SceUInt32 bits, top, n;

	*depth = 0;
	if (count == 0) {
		bits = -1;
		top = 0;
	} else {
		bits = 0;
		for (n = count >> 1; n != 0; n >>= 1)
			bits++;
		top = 1 << bits;
	}
	*depth = bits;
	*rest = (count ^ top) + 1;
}


/* make room for a leaf: the leaf and the parent it gets */
int pfsHashArrayGrow(ScePfsHashArray *array, SceSize size, SceSize itemSize)
{
	ScePfsHashArrayHeader *header = array->header;
	SceUInt32 max = (size - sizeof(ScePfsHashArrayHeader)) / itemSize;
	SceUInt32 count = header->count;

	if (max - count < 2)
		return SCE_PFS_ERROR_HASH_ARRAY_FULL;
	if (count != 0) {
		header->count = count + 2;
		return 0;
	}
	header->count = 1;
	return 0;
}


int pfsHashArrayShrink(ScePfsHashArray *array)
{
	ScePfsHashArrayHeader *header = array->header;
	SceUInt32 count = header->count;

	if (count == 0)
		return SCE_PFS_ERROR_NODE_EMPTY;
	if (count == 1)
		header->count = 0;
	else
		header->count = count - 2;
	return 0;
}


int pfsHashArrayPush(ScePfsHashArray *array, SceSize size, SceSize itemSize)
{
	ScePfsHashArrayHeader *header = array->header;
	SceUInt32 max = (size - sizeof(ScePfsHashArrayHeader)) / itemSize;

	if (header->count == max)
		return SCE_PFS_ERROR_HASH_ARRAY_FULL;
	header->count++;
	return 0;
}


int pfsHashArrayPop(ScePfsHashArray *array)
{
	if (array->header->count == 0)
		return SCE_PFS_ERROR_NODE_EMPTY;
	array->header->count--;
	return 0;
}


/* the leaves of a node of size bytes */
SceUInt32 pfsHashNodeFanout(SceSize size)
{
	SceUInt32 n = ((size - sizeof(ScePfsHashArrayHeader)) / PFS_HASH_SIZE + 1) / 2;

	while (size < n * (2 * PFS_HASH_SIZE + 4) + 0xc)
		n--;
	return n;
}


void pfsHashNodeViewAttach(ScePfsHashNodeView *view, void *data)
{
	view->level = data;
	pfsHashArrayAttach(&view->hashes, (char *)data + PFS_HASH_NODE_DATA);
	view->children = (void *)((char *)data + PFS_HASH_NODE_CHILDREN);
}


void pfsHashNodeViewInit(ScePfsHashNodeView *view, void *data, SceSize size)
{
	SceUInt32 n = ((size - sizeof(ScePfsHashArrayHeader)) / PFS_HASH_SIZE + 1) / 2;
	SceSize bytes = (2 * n - 1) * PFS_HASH_SIZE + sizeof(ScePfsHashArrayHeader);

	while (size < n * 4 + PFS_HASH_NODE_DATA + bytes) {
		n--;
		bytes = (2 * n - 1) * PFS_HASH_SIZE + sizeof(ScePfsHashArrayHeader);
	}
	view->level = data;
	pfsHashArrayInit(&view->hashes, PFS_HASH_SIZE, (char *)data + PFS_HASH_NODE_DATA, bytes);
	view->children = (void *)((char *)data + PFS_HASH_NODE_CHILDREN);
}


int pfsHashNodeViewGet(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
		       ScePfsHash *icv)
{
	return pfsHashArrayGet(&view->hashes, key, slot, icv, PFS_HASH_SIZE);
}


int pfsHashNodeViewGetChild(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
			    ScePfsHash *hash, SceUInt32 *child)
{
	*child = view->children[slot];
	return pfsHashArrayGet(&view->hashes, key, slot, hash, PFS_HASH_SIZE);
}


int pfsHashNodeViewChild(ScePfsHashNodeView *view, SceUInt32 slot, ScePfsHash *hash,
			 SceUInt32 *child)
{
	*child = view->children[slot];
	return pfsHashArrayPeek(&view->hashes, slot, hash, PFS_HASH_SIZE);
}


int pfsHashNodeViewSet(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
		       const ScePfsHash *icv, int *changed)
{
	return pfsHashArraySet(&view->hashes, key, slot, icv, PFS_HASH_SIZE, changed);
}


int pfsHashNodeViewSetChild(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
			    const ScePfsHash *hash, SceUInt32 child, int *changed)
{
	view->children[slot] = child;
	return pfsHashArraySet(&view->hashes, key, slot, hash, PFS_HASH_SIZE, changed);
}


int pfsHashNodeViewAppend(ScePfsHashNodeView *view, const ScePfsHash *key,
			  const ScePfsHash *hash, SceUInt32 child)
{
	int ret;

	ret = pfsHashArrayAppend(&view->hashes, key, hash, PFS_HASH_NODE_ARRAY_SIZE, PFS_HASH_SIZE);
	if (ret == 0)
		view->children[pfsHashNodeChildren(view) - 1] = child;
	return ret;
}


int pfsHashNodeViewRemoveLast(ScePfsHashNodeView *view, const ScePfsHash *key)
{
	return pfsHashArrayRemoveLast(&view->hashes, key, PFS_HASH_SIZE);
}


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
