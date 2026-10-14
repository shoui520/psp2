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

void pfsHashArrayAttach(ScePfsHashArray *array, void *data)
{
	array->header = data;
	array->items = (void *)((char *)data + sizeof(ScePfsHashArrayHeader));
}

void pfsHashArrayInit(ScePfsHashArray *array, SceSize itemSize, void *data, SceSize size)
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
