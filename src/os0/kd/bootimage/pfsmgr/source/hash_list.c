/*
 * A page of icvs: the array of icvs that fills it.
 */

#include "pfsmgr_internal.h"

#define PFS_HASH_PAGE_SIZE		0x400	/* name unknown: the page size icv pages are made for */

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
	pfsHashCopy(icv, &list->hashes.items[index]);
	return 0;
}

int pfsHashListSet(ScePfsHashList *list, SceUInt32 index, const ScePfsHash *icv)
{
	if (index >= list->hashes.header->count)
		return SCE_PFS_ERROR_HASH_LIST_INDEX;
	pfsHashCopy(&list->hashes.items[index], icv);
	return 0;
}

int pfsHashListAppend(ScePfsHashList *list, const ScePfsHash *icv)
{
	int ret;

	ret = pfsHashArrayPush(&list->hashes, PFS_HASH_PAGE_SIZE, PFS_HASH_SIZE);
	if (ret == 0)
		pfsHashCopy(&list->hashes.items[list->hashes.header->count - 1], icv);
	return ret;
}

int pfsHashListRemoveLast(ScePfsHashList *list)
{
	return pfsHashArrayPop(&list->hashes);
}
