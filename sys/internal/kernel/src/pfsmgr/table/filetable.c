/*
 * Unicv entries: a file's icvs in "unicv.db", a record page followed by
 * pages of icvs, all from the database's page allocator.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_UNICV_ENTRY_MAGIC		0x4c42544649454353ULL	/* "SCEIFTBL" */

#define SCE_PFS_ERROR_UNICV_EMPTY	0x80140304	/* name unknown */
#define SCE_PFS_ERROR_UNICV_RANGE	0x80140306	/* name unknown */
#define SCE_PFS_ERROR_UNICV_MAGIC	0x8014030a	/* name unknown */
#define SCE_PFS_ERROR_UNICV_VERSION	0x80140315	/* name unknown */

int pfsUnicvEntryWriteRecord(ScePfsUnicvEntry *entry)
{
	int ret;

	ret = pfsHashPageLoad(&entry->page, entry->offset, 1);
	if (ret != 0)
		return ret;
	memcpy(entry->page.acc.entry->data, &entry->record, sizeof(entry->record));
	entry->page.acc.entry->flags |= PFS_LRU_DIRTY;
	return pfsHashPageRelease(&entry->page);
}

/* hold the page of icvs index is in */
int pfsUnicvEntryGetPage(ScePfsUnicvEntry *entry, SceUInt32 index, ScePfsHashPage **page)
{
	SceUInt32 first = index - index % entry->record.hashesPerPage;
	int ret;

	if (first >= entry->record.count)
		return SCE_PFS_ERROR_UNICV_RANGE;
	if (entry->pageHeld) {
		if (entry->pageIndex == first)
			goto held;
		ret = pfsHashPageRelease(&entry->page);
		if (ret != 0)
			return ret;
		entry->pageHeld = 0;
	}
	ret = pfsHashPageLoad(&entry->page,
			      entry->offset + entry->record.pageSize +
			      (SceUInt64)(first / entry->record.hashesPerPage) * entry->record.pageSize,
			      0);
	if (ret != 0)
		return ret;
held:
	*page = &entry->page;
	entry->pageIndex = first;
	entry->pageHeld = 1;
	return 0;
}

/* find the node at level on the way to icv index, from the root */
int pfsHashNodesWalk(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		     SceUInt32 level, ScePfsHashNodePage *page, SceUInt32 *slot, int last)
{
	ScePfsHash expect, hash;
	SceUInt32 child = nodes->root;
	SceUInt32 cur, div, i;
	int ret;

	icv_copy(&expect, &nodes->rootHash);
	ret = pfsHashNodePageLoad(page, nodes->pages->lru->pageSize * child);
	if (ret != 0)
		return ret;
	if (pfsHashNodeChildren(&page->view) == 0)
		return SCE_PFS_ERROR_NODES_RANGE;
	cur = *page->view.level;
	if (cur < level)
		return SCE_PFS_ERROR_NODES_LEVEL;
	for (;;) {
		if (pfsHashNodeChildren(&page->view) == 0)
			icv_clear(&hash);
		else
			icv_copy(&hash, page->view.hashes.items);
		if (!icv_equal(&hash, &expect))
			return SCE_PFS_ERROR_NODES_HASH;
		if (last) {
			*slot = pfsHashNodeChildren(&page->view) - 1;
		} else if (cur == 0) {
			*slot = index;
		} else {
			div = 1;
			for (i = 0; i != cur; i++)
				div *= nodes->fanout;
			*slot = index / div;
			if (level < i)
				index %= div;
		}
		if (cur == level)
			return 0;
		ret = pfsHashNodeViewGetChild(&page->view, key, *slot, &expect, &child);
		if (ret != 0)
			return ret;
		ret = pfsHashNodePageLoad(page, nodes->pages->lru->pageSize * child);
		if (ret != 0)
			return ret;
		cur = *page->view.level;
	}
}

/* let go of the page held and write the record (name unknown) */
static inline int pfsUnicvEntrySync(ScePfsUnicvEntry *entry)
{
	int ret;

	if (entry->pageHeld) {
		ret = pfsHashPageRelease(&entry->page);
		if (ret != 0)
			return ret;
		entry->pageHeld = 0;
	}
	if (entry->dirty) {
		ret = pfsUnicvEntryWriteRecord(entry);
		if (ret != 0)
			return ret;
		entry->dirty = 0;
	}
	return 0;
}

int _init_top(ScePfsUnicvEntry *entry, ScePfsPageAllocator *pages, SceUInt32 version)
{
	int ret;

	memset(&entry->record, 0, sizeof(entry->record));
	entry->record.magic = PFS_UNICV_ENTRY_MAGIC;
	entry->record.version = version;
	entry->record.pageSize = pages->lru->pageSize;
	entry->pageHeld = 0;
	entry->dirty = 0;
	entry->pageIndex = 0;
	entry->record.hashesPerPage = (pages->lru->pageSize - 0x10) / PFS_HASH_SIZE;
	pfsHashPageInit(&entry->page, pages);
	ret = pfsHashPageAlloc(&entry->page);
	if (ret != 0)
		return ret;
	entry->offset = entry->page.offset;
	memset(entry->page.acc.entry->data, 0, entry->record.pageSize);
	memcpy(entry->page.acc.entry->data, &entry->record, sizeof(entry->record));
	entry->page.acc.entry->flags |= PFS_LRU_DIRTY;
	return pfsHashPageRelease(&entry->page);
}

int pfsUnicvEntryOpen(ScePfsUnicvEntry *entry, ScePfsPageAllocator *pages, SceOff offset,
		      SceUInt32 version)
{
	int ret;

	entry->pageHeld = 0;
	entry->dirty = 0;
	entry->pageIndex = 0;
	entry->offset = offset;
	pfsHashPageInit(&entry->page, pages);
	ret = pfsHashPageLoad(&entry->page, entry->offset, 1);
	if (ret != 0)
		return ret;
	memcpy(&entry->record, entry->page.acc.entry->data, sizeof(entry->record));
	ret = pfsHashPageRelease(&entry->page);
	if (ret != 0)
		return ret;
	if (entry->record.magic != PFS_UNICV_ENTRY_MAGIC)
		return SCE_PFS_ERROR_UNICV_MAGIC;
	if (entry->record.version != version)
		return SCE_PFS_ERROR_UNICV_VERSION;
	if (entry->record.pageSize != pages->lru->pageSize)
		return SCE_PFS_ERROR_UNICV_PAGE_SIZE;
	return 0;
}

int pfsUnicvEntryFlush(ScePfsUnicvEntry *entry)
{
	return pfsUnicvEntrySync(entry);
}

int pfsUnicvEntrySetBlockSize(ScePfsUnicvEntry *entry, SceSize blockSize)
{
	entry->record.blockSize = blockSize;
	entry->dirty = 1;
	return 0;
}

int pfsUnicvEntryClose(ScePfsUnicvEntry *entry)
{
	int ret;

	ret = pfsUnicvEntrySync(entry);
	if (ret != 0)
		return ret;
	return pfsHashPageTerm(&entry->page);
}

int pfsUnicvEntryGet(ScePfsUnicvEntry *entry, SceUInt32 index, ScePfsHash *icv)
{
	ScePfsHashPage *page;
	int ret;

	ret = pfsUnicvEntryGetPage(entry, index, &page);
	if (ret != 0)
		return ret;
	return pfsHashListGet(&page->list, index % entry->record.hashesPerPage, icv);
}

int pfsUnicvEntrySet(ScePfsUnicvEntry *entry, SceUInt32 index, const ScePfsHash *icv)
{
	ScePfsHashPage *page;
	int ret;

	ret = pfsUnicvEntryGetPage(entry, index, &page);
	if (ret != 0)
		return ret;
	ret = pfsHashListSet(&page->list, index % entry->record.hashesPerPage, icv);
	if (ret == 0)
		page->acc.entry->flags |= PFS_LRU_DIRTY;
	return ret;
}

int pfsUnicvEntryAppend(ScePfsUnicvEntry *entry, const ScePfsHash *icv)
{
	ScePfsHashPage *page;
	SceUInt32 count = entry->record.count;
	int ret;

	if (count % entry->record.hashesPerPage == 0) {
		/* the last page is full */
		page = &entry->page;
		if (entry->pageHeld) {
			ret = pfsHashPageRelease(page);
			if (ret != 0)
				return ret;
			entry->pageHeld = 0;
		}
		ret = pfsHashPageAlloc(page);
		if (ret != 0)
			return ret;
		if (entry->offset == -1)
			entry->offset = entry->page.offset;
		entry->pageHeld = 1;
		ret = pfsHashListAppend(&page->list, icv);
	} else {
		ret = pfsUnicvEntryGetPage(entry, count, &page);
		if (ret != 0)
			return ret;
		ret = pfsHashListAppend(&page->list, icv);
	}
	if (ret == 0) {
		entry->dirty = 1;
		entry->record.count++;
		page->acc.entry->flags |= PFS_LRU_DIRTY;
	}
	return ret;
}

int pfsUnicvEntryRemoveLast(ScePfsUnicvEntry *entry)
{
	ScePfsHashPage *page;
	int ret;

	if (entry->record.count == 0)
		return SCE_PFS_ERROR_UNICV_EMPTY;
	ret = pfsUnicvEntryGetPage(entry, entry->record.count - 1, &page);
	if (ret != 0)
		return ret;
	if (entry->record.count % entry->record.hashesPerPage == 1) {
		/* the last page empties */
		ret = pfsHashPageFree(page);
		if (ret != 0)
			return ret;
		entry->pageHeld = 0;
	} else {
		ret = pfsHashListRemoveLast(&page->list);
		if (ret != 0)
			return ret;
		page->acc.entry->flags |= PFS_LRU_DIRTY;
	}
	entry->dirty = 1;
	entry->record.count--;
	return 0;
}
