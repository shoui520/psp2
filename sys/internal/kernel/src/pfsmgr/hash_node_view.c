/*
 * A node of an icv tree as it lies in its page: the level, the array of
 * hashes, and the pages of the children.
 */

#include "pfsmgr_internal.h"

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
