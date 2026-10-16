/*
 * Icv trees ("SCEICVDB"): the icv of each block of a file, in a tree of
 * hashed nodes kept in pages of the file's "%08x.icv", under a header that
 * holds the root hash.
 */

#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_HASH_TREE_SECTOR	0x200	/* name unknown: what the header is read in */
#define PFS_HASH_TREE_V1_BASE	0x40	/* name unknown: page 0 of version 1 trees */

typedef union ScePfsHashTreeSector {		/* name unknown */
	ScePfsHashTreeHeader header;
	char bytes[PFS_HASH_TREE_SECTOR];
} ScePfsHashTreeSector;

int pfsHashTreeReadHeader(ScePfsHashTree *tree)
{
	char buf[sizeof(ScePfsHashTreeSector) + PFS_DMA_ALIGN - 1];
	ScePfsHashTreeSector *sector = PFS_DMA_ALIGNED(buf);
	SceSize n;
	int ret;

	ret = pfsLowerPread(tree->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_ICV_BROKEN;
	tree->header = sector->header;
	return 0;
}

int pfsHashTreeWriteHeader(ScePfsHashTree *tree)
{
	char buf[sizeof(ScePfsHashTreeSector) + PFS_DMA_ALIGN - 1];
	ScePfsHashTreeSector *sector = PFS_DMA_ALIGNED(buf);
	SceSize n;
	int ret;

	ret = pfsLowerPread(tree->lf, sector, sizeof(*sector), 0, &n);
	if (ret != 0)
		return ret;
	if (n != sizeof(*sector))
		return SCE_PFS_ERROR_ICV_BROKEN;
	sector->header = tree->header;
	return pfsLowerPwrite(tree->lf, sector, sizeof(*sector), 0, &n);
}

void pfsHashTreeInit(ScePfsHashTree *tree, ScePfsLru *lru)
{
	tree->lf = NULL;
	tree->lru = lru;
	tree->dirty = 0;
	tree->header.magic = 0;
}

int pfsHashTreeCreate(ScePfsHashTree *tree, ScePfsLowerFile *lf, const void *key, SceSize blockSize,
		      SceUInt32 version)
{
	int ret;

	memset(&tree->header, 0, sizeof(tree->header));
	tree->header.version = version;
	tree->header.magic = PFS_HASH_TREE_MAGIC;
	tree->header.blockSize = blockSize;
	tree->header.nodeSize = tree->lru->pageSize;
	tree->header.root = 0;
	tree->header.freeHead = 0;
	tree->header.end = 0;
	tree->key = *(const ScePfsHash *)key;
	tree->lf = lf;
	ret = pfsLowerSetSize(lf, tree->header.nodeSize);
	if (ret != 0)
		return ret;
	ret = pfsLowerZero(tree->lf, tree->header.nodeSize);
	if (ret != 0)
		return ret;
	pfsPageAllocatorInit(&tree->pages, tree->lru, lf,
			     tree->header.version == 1 ? PFS_HASH_TREE_V1_BASE :
							 tree->header.nodeSize);
	pfsHashNodesInit(&tree->nodes, &tree->pages);
	ret = pfsHashNodesCreate(&tree->nodes);
	if (ret == 0 &&
	    (ret = pfsHashNodesRootHash(&tree->nodes, &tree->key, &tree->header.rootHash)) == 0)
		tree->dirty = 1;
	return ret;
}

int pfsHashTreeOpen(ScePfsHashTree *tree, ScePfsLowerFile *lf, const void *key, SceUInt32 version)
{
	ScePfsHash hash;
	SceUInt64 size;
	SceSize base;
	int ret;

	tree->lf = lf;
	pfsLowerGetSize(lf, &size);
	if (size < sizeof(ScePfsHashTreeHeader))
		return SCE_PFS_ERROR_ICV_BROKEN;
	ret = pfsHashTreeReadHeader(tree);
	if (ret != 0)
		return ret;
	if (tree->header.magic != PFS_HASH_TREE_MAGIC)
		return SCE_PFS_ERROR_ICV_BROKEN;
	if (tree->header.version != version)
		return SCE_PFS_ERROR_ICV_VERSION;
	base = tree->header.version == 1 ? PFS_HASH_TREE_V1_BASE : tree->header.nodeSize;
	if (tree->lru->pageSize != tree->header.nodeSize)
		return SCE_PFS_ERROR_ICV_NODE_SIZE;
	pfsPageAllocatorInit(&tree->pages, tree->lru, lf, base);
	pfsPageAllocatorSet(&tree->pages, tree->header.freeHead, tree->header.end);
	pfsHashNodesInit(&tree->nodes, &tree->pages);
	ret = pfsHashNodesOpen(&tree->nodes, tree->header.root);
	if (ret != 0)
		return ret;
	tree->key = *(const ScePfsHash *)key;
	ret = pfsHashNodesRootHash(&tree->nodes, &tree->key, &hash);
	if (ret == 0)
		return pfsHashEqual(&hash, &tree->header.rootHash) ? 0 : SCE_PFS_ERROR_ICV_HASH;
	if (ret == SCE_PFS_ERROR_NODES_HASH || ret == SCE_PFS_ERROR_NODE_HASH)
		return SCE_PFS_ERROR_ICV_HASH;
	return ret;
}

int pfsHashTreeFlush(ScePfsHashTree *tree)
{
	int ret;

	ret = pfsHashNodesFlush(&tree->nodes, &tree->key);
	if (ret == 0) {
		if (!tree->dirty)
			return 0;
		pfsPageAllocatorGet(&tree->pages, (SceUInt64 *)&tree->header.freeHead,
				    (SceUInt64 *)&tree->header.end);
		tree->header.root = tree->nodes.root;
		ret = pfsHashNodesRootHash(&tree->nodes, &tree->key, &tree->header.rootHash);
		if (ret == 0) {
			ret = pfsHashTreeWriteHeader(tree);
			if (ret != 0)
				return ret;
			tree->dirty = 0;
			return 0;
		}
	}
	if (ret == SCE_PFS_ERROR_NODES_HASH || ret == SCE_PFS_ERROR_NODE_HASH)
		return SCE_PFS_ERROR_ICV_HASH;
	return ret;
}

int pfsHashTreeGet(ScePfsHashTree *tree, SceUInt32 index, ScePfsHash *icv)
{
	int ret;

	ret = pfsHashNodesGet(&tree->nodes, &tree->key, index, icv);
	switch (ret) {
	case 0:
		return 0;
	case SCE_PFS_ERROR_NODES_RANGE:
	case SCE_PFS_ERROR_NODE_RANGE:
		return SCE_PFS_ERROR_ICV_RANGE;
	case SCE_PFS_ERROR_NODES_HASH:
	case SCE_PFS_ERROR_NODE_HASH:
		return SCE_PFS_ERROR_ICV_HASH;
	default:
		return ret;
	}
}

int pfsHashTreeSet(ScePfsHashTree *tree, SceUInt32 index, const ScePfsHash *icv)
{
	int changed, ret;

	ret = pfsHashNodesSet(&tree->nodes, &tree->key, index, icv, &changed);
	switch (ret) {
	case 0:
		if (changed)
			tree->dirty = 1;
		return 0;
	case SCE_PFS_ERROR_NODES_RANGE:
	case SCE_PFS_ERROR_NODE_RANGE:
		return SCE_PFS_ERROR_ICV_RANGE;
	case SCE_PFS_ERROR_NODES_HASH:
	case SCE_PFS_ERROR_NODE_HASH:
		return SCE_PFS_ERROR_ICV_HASH;
	default:
		return ret;
	}
}

int pfsHashTreeAppend(ScePfsHashTree *tree, const ScePfsHash *icv)
{
	int ret;

	ret = pfsHashNodesAppend(&tree->nodes, &tree->key, icv);
	if (ret == 0) {
		tree->dirty = 1;
		tree->header.count++;
	} else if (ret == SCE_PFS_ERROR_NODES_HASH || ret == SCE_PFS_ERROR_NODE_HASH) {
		return SCE_PFS_ERROR_ICV_HASH;
	}
	return ret;
}

int pfsHashTreeRemoveLast(ScePfsHashTree *tree)
{
	int ret;

	ret = pfsHashNodesRemoveLast(&tree->nodes, &tree->key);
	if (ret == 0) {
		tree->dirty = 1;
		tree->header.count--;
		return 0;
	}
	switch (ret) {
	case SCE_PFS_ERROR_NODE_EMPTY:
		return SCE_PFS_ERROR_ICV_EMPTY;
	case SCE_PFS_ERROR_NODES_HASH:
	case SCE_PFS_ERROR_NODE_HASH:
		return SCE_PFS_ERROR_ICV_HASH;
	default:
		return ret;
	}
}

int pfsHashTreeRootHash(ScePfsHashTree *tree, void *hash)
{
	int ret;

	ret = pfsHashTreeFlush(tree);
	if (ret == 0)
		pfsHmacSha1Dma(hash, &tree->key, &tree->header, sizeof(tree->header));
	return ret;
}
