/*
 * The binary hash tree a node keeps in its array: item 0 is the root, the
 * children of item i are items 2i + 1 and 2i + 2, and the leaves are the
 * icvs of the node.
 */

#include "pfsmgr_internal.h"

/* the item of leaf slot */
SceUInt32 pfsHashArrayLeaf(ScePfsHashArray *array, SceUInt32 slot)
{
	SceUInt32 index = slot * 2;
	SceUInt32 depth, rest, leafDepth, leafRest, span;

	pfsHashArrayDepth(array, &depth, &rest);
	pfsHashLog2(index + 1, &leafDepth, &leafRest);
	if (depth - leafDepth == 0)
		return index;
	span = 1 << (depth - leafDepth);
	if (index * span + (span - 1) >= array->header->count)
		return index * (span >> 1) + ((span >> 1) - 1);
	return index * span + (span - 1);
}

/* hash the items above index again */
void pfsHashArrayRehash(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 index,
			SceSize itemSize)
{
	ScePfsHash pair[2];
	SceUInt32 sibling, parent;

	if (array->header->count + 1 < 2)
		return;
	for (; index != 0; index = parent) {
		sibling = ((index + 1) ^ 1) - 1;
		parent = (index + 1) / 2 - 1;
		if (index < sibling) {
			pair[0] = *(ScePfsHash *)pfsHashArrayItem(array, index, itemSize);
			pair[1] = *(ScePfsHash *)pfsHashArrayItem(array, sibling, itemSize);
		} else {
			pair[0] = *(ScePfsHash *)pfsHashArrayItem(array, sibling, itemSize);
			pair[1] = *(ScePfsHash *)pfsHashArrayItem(array, index, itemSize);
		}
		pfsHmacSha1(pfsHashArrayItem(array, parent, itemSize), key, pair, sizeof(pair));
	}
}

/* whether the items above index hash right */
int pfsHashArrayCheck(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 index,
		      SceSize itemSize)
{
	ScePfsHash pair[2], hash;
	SceUInt32 sibling, parent;

	if (array->header->count + 1 < 2)
		return 1;
	for (; index != 0; index = parent) {
		sibling = ((index + 1) ^ 1) - 1;
		parent = (index + 1) / 2 - 1;
		pair[0] = *(ScePfsHash *)pfsHashArrayItem(array, index < sibling ? index : sibling,
							   itemSize);
		pair[1] = *(ScePfsHash *)pfsHashArrayItem(array, index < sibling ? sibling : index,
							   itemSize);
		if (pfsHmacSha1(&hash, key, pair, sizeof(pair)) != 0 ||
		    !pfsHashEqual(pfsHashArrayItem(array, parent, itemSize), &hash))
			return 0;
	}
	return 1;
}

int pfsHashArraySet(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 slot,
		    const ScePfsHash *hash, SceSize itemSize, int *changed)
{
	SceUInt32 index;

	*changed = 0;
	if (slot >= (array->header->count + 1) / 2)
		return SCE_PFS_ERROR_NODE_RANGE;
	index = pfsHashArrayLeaf(array, slot);
	if (!pfsHashArrayCheck(array, key, index, itemSize))
		return SCE_PFS_ERROR_NODE_HASH;
	if (pfsHashEqual(pfsHashArrayItem(array, index, itemSize), hash))
		return 0;
	pfsHashCopy(pfsHashArrayItem(array, index, itemSize), hash);
	pfsHashArrayRehash(array, key, index, itemSize);
	*changed = 1;
	return 0;
}

int pfsHashArrayGet(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 slot,
		    ScePfsHash *hash, SceSize itemSize)
{
	SceUInt32 index;

	if (slot >= (array->header->count + 1) / 2)
		return SCE_PFS_ERROR_NODE_RANGE;
	index = pfsHashArrayLeaf(array, slot);
	if (!pfsHashArrayCheck(array, key, index, itemSize))
		return SCE_PFS_ERROR_NODE_HASH;
	pfsHashCopy(hash, pfsHashArrayItem(array, index, itemSize));
	return 0;
}

/* leaf slot, unchecked */
int pfsHashArrayPeek(ScePfsHashArray *array, SceUInt32 slot, ScePfsHash *hash, SceSize itemSize)
{
	if (slot >= (array->header->count + 1) / 2)
		return SCE_PFS_ERROR_NODE_RANGE;
	pfsHashCopy(hash, pfsHashArrayItem(array, pfsHashArrayLeaf(array, slot), itemSize));
	return 0;
}

int pfsHashArrayAppend(ScePfsHashArray *array, const ScePfsHash *key, const ScePfsHash *hash,
		       SceSize size, SceSize itemSize)
{
	SceUInt32 count = array->header->count;
	SceUInt32 last = count - (count + 1) / 2;
	SceUInt32 index;
	int ret;

	if (!pfsHashArrayCheck(array, key, last, itemSize))
		return SCE_PFS_ERROR_NODE_HASH;
	ret = pfsHashArrayGrow(array, size, itemSize);
	if (ret != 0)
		return ret;
	index = array->header->count - 1;
	if (index != 0)
		pfsHashCopy(pfsHashArrayItem(array, (array->header->count ^ 1) - 1, itemSize),
			    pfsHashArrayItem(array, last, itemSize));
	pfsHashCopy(pfsHashArrayItem(array, index, itemSize), hash);
	pfsHashArrayRehash(array, key, index, itemSize);
	return 0;
}

int pfsHashArrayRemoveLast(ScePfsHashArray *array, const ScePfsHash *key, SceSize itemSize)
{
	SceUInt32 count = array->header->count;
	SceUInt32 parent;
	int ret;

	if (count + 1 < 2)
		return SCE_PFS_ERROR_NODE_EMPTY;
	if (!pfsHashArrayCheck(array, key, count - 1, itemSize))
		return SCE_PFS_ERROR_NODE_HASH;
	if (count - 1 == 0)
		return pfsHashArrayShrink(array);
	parent = count / 2 - 1;
	pfsHashCopy(pfsHashArrayItem(array, parent, itemSize),
		    pfsHashArrayItem(array, (count ^ 1) - 1, itemSize));
	ret = pfsHashArrayShrink(array);
	if (ret != 0)
		return ret;
	pfsHashArrayRehash(array, key, parent, itemSize);
	return 0;
}

