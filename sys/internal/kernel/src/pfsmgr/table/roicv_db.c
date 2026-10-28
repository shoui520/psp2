/*
 * "unicv.db": the icv trees of all files of a mount in one file. Its first
 * page holds a header; each file owns a chain of pages, found by its index,
 * holding one icv per block of the file. Two kinds of tree use it: kind 0
 * keeps the icvs, kind 3 only counts blocks.
 */

#include "pfsmgr_internal.h"

#define PFS_UNICV_PAGE_SIZE	0x200	/* name unknown: what the header is read in */
#define PFS_UNICV_HEADER_PAGE	0x400	/* name unknown */

typedef union ScePfsUnicvPage {			/* name unknown */
	ScePfsUnicvHeader header;
	SceUInt8 bytes[PFS_UNICV_PAGE_SIZE];
} ScePfsUnicvPage;

int pfsUnicvReadHeader(ScePfsLowerFile *lf, ScePfsUnicvHeader *header)
{
	ScePfsUnicvPage page __attribute__((aligned(0x40)));
	SceSize n;
	int ret;

	ret = pfsLowerPread(lf, &page, sizeof(page), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(page))
		return SCE_PFS_ERROR_ICV_BROKEN;
	__builtin_memcpy(header, page.bytes, sizeof(*header));
	return 0;
}

int pfsUnicvWriteHeader(ScePfsLowerFile *lf, const ScePfsUnicvHeader *header)
{
	ScePfsUnicvPage page __attribute__((aligned(0x40)));
	SceSize n;
	int ret;

	ret = pfsLowerPread(lf, &page, sizeof(page), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(page))
		return SCE_PFS_ERROR_ICV_BROKEN;
	__builtin_memcpy(page.bytes, header, sizeof(*header));
	return pfsLowerPwrite(lf, &page, n, 0, &n);
}

int pfsUnicvInit(ScePfsUnicv *unicv, ScePfsLru *lru, ScePfsLowerFile *lf, SceUInt32 version)
{
	SceSize pageSize = lru->pageSize;
	SceUInt64 size, zero;
	SceSize n;
	int ret;

	pfsPageAllocatorInit(&unicv->pages, lru, lf, pageSize);
	ret = file_access_length(lf, &size);
	if (ret != 0)
		return ret;

	if (size == 0) {
		unicv->header.version = version;
		unicv->header.pageSize = pageSize;
		unicv->header.end = size;
		unicv->header.magic = PFS_UNICV_MAGIC;
		unicv->header.freeHead = (SceUInt64)-1;
		zero = size;
		ret = pfsLowerPwrite(lf, &zero, sizeof(zero), pageSize - sizeof(zero), &n);
		if (ret != 0)
			return ret;
		return pfsLowerPwrite(lf, &unicv->header, sizeof(unicv->header), 0, &n);
	}

	ret = pfsUnicvReadHeader(lf, &unicv->header);
	if (ret != 0)
		return ret;
	if (unicv->header.magic != PFS_UNICV_MAGIC)
		return SCE_PFS_ERROR_ICV_BROKEN;
	if (unicv->header.version != version)
		return SCE_PFS_ERROR_ICV_VERSION;
	if (pageSize != unicv->header.pageSize || unicv->header.freeHead != (SceUInt64)-1 ||
	    unicv->header.end != size - PFS_UNICV_HEADER_PAGE)
		return SCE_PFS_ERROR_ICV_BROKEN;
	pfsPageAllocatorSet(&unicv->pages, unicv->header.freeHead, unicv->header.end);
	return ret;
}

/* write the allocator's state back to the header, if it changed */
int pfsUnicvSync(ScePfsUnicv *unicv)
{
	SceUInt64 freeHead, end;

	pfsPageAllocatorGet(&unicv->pages, &freeHead, &end);
	if (unicv->header.freeHead == freeHead && unicv->header.end == end)
		return 0;
	unicv->header.end = end;
	unicv->header.freeHead = freeHead;
	return pfsUnicvWriteHeader(unicv->pages.lf, &unicv->header);
}

int pfsUnicvTerm(ScePfsUnicv *unicv)
{
	int ret;

	ret = pfsUnicvSync(unicv);
	if (ret == 0)
		pfsPageAllocatorTerm(&unicv->pages);
	return ret;
}

int pfsUnicvOpenEntry(ScePfsUnicvEntry *entry, ScePfsUnicv *unicv, SceOff offset)
{
	return pfsUnicvEntryOpen(entry, &unicv->pages, offset, unicv->header.version);
}

int pfsUnicvCloseEntry(ScePfsUnicvEntry *entry)
{
	return pfsUnicvEntryClose(entry);
}

/* a new file's chain of pages; its index is where the chain starts, in pages */
int pfsUnicvAllocIndex(ScePfsUnicv *unicv, SceUInt32 *index)
{
	ScePfsUnicvEntry entry;
	SceUInt32 pageSize;
	SceOff offset;
	int ret;

	ret = _init_top(&entry, &unicv->pages, unicv->header.version);
	if (ret != 0)
		return ret;
	offset = entry.offset;
	pageSize = unicv->header.pageSize;
	ret = pfsUnicvEntryClose(&entry);
	if (ret == 0)
		*index = (offset + pageSize) / (SceOff)unicv->header.pageSize;
	return ret;
}
