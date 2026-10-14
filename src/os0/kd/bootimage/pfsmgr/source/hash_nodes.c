/*
 * The nodes of an icv key: pages of icvs, the leaves holding the icvs of
 * the blocks, each node above holding the hashes of its children. The node
 * last used is kept held, and its hash is carried up to the root on flush.
 */

#include "pfsmgr_internal.h"

#define PFS_HASH_NODE_FANOUT		23	/* name unknown: children of a 0x400 byte node */

#define SCE_PFS_ERROR_NODES_NONE	0x80140407	/* name unknown: no root */

/* the hash of a node: its first icv, zero when empty (name unknown) */
static inline void pfsHashNodeHash(ScePfsHash *hash, ScePfsHashNodeView *view)
{
	if (pfsHashNodeChildren(view) == 0)
		pfsHashZero(hash);
	else
		pfsHashCopy(hash, view->hashes.items);
}

static inline SceUInt32 pfsHashNodePageIndex(ScePfsHashNodes *nodes, ScePfsHashNodePage *page)	/* name unknown */
{
	return page->offset / nodes->pages->lru->pageSize;
}

/* load every node under page */
int pfsHashNodesVisit(ScePfsHashNodes *nodes, SceUInt32 page)
{
	ScePfsHashNodePage node;
	ScePfsHash hash;
	SceUInt32 child, count, i;
	int ret;

	pfsHashNodePageInit(&node, nodes->pages);
	ret = pfsHashNodePageLoad(&node, nodes->pages->lru->pageSize * page);
	if (ret != 0)
		return ret;
	if (*node.view.level != 0) {
		count = pfsHashNodeChildren(&node.view);
		for (i = 0; i != count; i++) {
			ret = pfsHashNodeViewChild(&node.view, i, &hash, &child);
			if (ret != 0)
				return ret;
			ret = pfsHashNodesVisit(nodes, child);
			if (ret != 0)
				return ret;
		}
	}
	return pfsHashNodePageRelease(&node);
}

/* carry the hash of page, the last node at its level, up to the root */
int pfsHashNodesUpdateLast(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHashNodePage *page)
{
	ScePfsHash hash;
	SceUInt32 level = *page->view.level + 1;
	SceUInt32 child, slot;
	int changed, ret;

	if (nodes->height < level)
		return SCE_PFS_ERROR_NODES_LEVEL;
	child = pfsHashNodePageIndex(nodes, page);
	pfsHashNodeHash(&hash, &page->view);
	for (; level < nodes->height; level++) {
		ret = pfsHashNodesWalk(nodes, key, 0, level, page, &slot, 1);
		if (ret != 0)
			return ret;
		ret = pfsHashNodeViewSetChild(&page->view, key, slot, &hash, child, &changed);
		if (ret != 0)
			return ret;
		child = pfsHashNodePageIndex(nodes, page);
		if (!changed)
			return 0;
		page->acc.entry->flags |= PFS_LRU_DIRTY;
		pfsHashNodeHash(&hash, &page->view);
	}
	nodes->root = child;
	pfsHashCopy(&nodes->rootHash, &hash);
	return 0;
}

int pfsHashNodesCreate(ScePfsHashNodes *nodes)
{
	ScePfsHashNodePage page;
	int ret;

	pfsHashNodePageInit(&page, nodes->pages);
	if (nodes->root != -1) {
		ret = pfsHashNodesVisit(nodes, nodes->root);
		if (ret != 0)
			return ret;
	}
	nodes->root = 0;
	ret = pfsHashNodePageAlloc(&page);
	if (ret != 0)
		return ret;
	*page.view.level = 0;
	page.acc.entry->flags |= PFS_LRU_DIRTY;
	nodes->root = pfsHashNodePageIndex(nodes, &page);
	pfsHashNodeHash(&nodes->rootHash, &page.view);
	nodes->height = *page.view.level + 1;
	return pfsHashNodePageRelease(&page);
}

int pfsHashNodesOpen(ScePfsHashNodes *nodes, SceUInt32 root)
{
	ScePfsHashNodePage page;
	int ret;

	pfsHashNodePageInit(&page, nodes->pages);
	ret = pfsHashNodePageLoad(&page, nodes->pages->lru->pageSize * root);
	if (ret != 0)
		return ret;
	nodes->root = root;
	pfsHashNodeHash(&nodes->rootHash, &page.view);
	nodes->height = *page.view.level + 1;
	return pfsHashNodePageRelease(&page);
}

void pfsHashNodesInit(ScePfsHashNodes *nodes, ScePfsPageAllocator *pages)
{
	nodes->pages = pages;
	pfsHashZero(&nodes->rootHash);
	nodes->root = -1;
	nodes->height = 0;
	nodes->held = 0;
	nodes->dirty = 0;
	pfsHashNodePageInit(&nodes->page, pages);
	nodes->tail = 0;
	nodes->base = 0;
	nodes->fanout = pfsHashNodeFanout(pages->lru->pageSize);
}

int pfsHashNodesFlush(ScePfsHashNodes *nodes, const ScePfsHash *key)
{
	ScePfsHash hash;
	SceUInt32 level, slot;
	int changed, ret;

	if (nodes->root == -1)
		return SCE_PFS_ERROR_NODES_NONE;
	if (nodes->held && nodes->dirty) {
		if (nodes->tail) {
			ret = pfsHashNodesUpdateLast(nodes, key, &nodes->page);
			if (ret != 0)
				return ret;
		} else {
			level = *nodes->page.view.level + 1;
			if (nodes->height < level)
				return SCE_PFS_ERROR_NODES_LEVEL;
			pfsHashNodeHash(&hash, &nodes->page.view);
			for (; level < nodes->height; level++) {
				ret = pfsHashNodesWalk(nodes, key, nodes->base, level, &nodes->page,
						       &slot, 0);
				if (ret != 0)
					return ret;
				ret = pfsHashNodeViewSet(&nodes->page.view, key, slot, &hash, &changed);
				if (ret != 0)
					return ret;
				if (!changed)
					goto done;
				nodes->page.acc.entry->flags |= PFS_LRU_DIRTY;
				pfsHashNodeHash(&hash, &nodes->page.view);
			}
			pfsHashCopy(&nodes->rootHash, &hash);
		}
	}
done:
	if (nodes->held) {
		ret = pfsHashNodePageRelease(&nodes->page);
		if (ret != 0)
			return ret;
	}
	nodes->held = 0;
	nodes->dirty = 0;
	return 0;
}

int pfsHashNodesRootHash(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHash *hash)
{
	int ret;

	ret = pfsHashNodesFlush(nodes, key);
	if (ret == 0)
		pfsHashCopy(hash, &nodes->rootHash);
	return ret;
}

/* hold the leaf icv index is in */
int pfsHashNodesGetLeaf(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
			ScePfsHashNodePage **page, SceUInt32 *slot)
{
	int ret;

	if (nodes->held) {
		if (!nodes->tail && nodes->base <= index && index < nodes->base + nodes->fanout) {
			*slot = index - nodes->base;
			*page = &nodes->page;
			return 0;
		}
		ret = pfsHashNodesFlush(nodes, key);
		if (ret != 0)
			return ret;
	}
	ret = pfsHashNodesWalk(nodes, key, index, 0, &nodes->page, slot, 0);
	if (ret != 0)
		return ret;
	nodes->dirty = 0;
	nodes->tail = 0;
	nodes->held = 1;
	nodes->base = index - *slot;
	*page = &nodes->page;
	return 0;
}

int pfsHashNodesGet(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		    ScePfsHash *icv)
{
	ScePfsHashNodePage *page;
	SceUInt32 slot;
	int ret;

	if (nodes->root == -1)
		return SCE_PFS_ERROR_NODES_NONE;
	ret = pfsHashNodesGetLeaf(nodes, key, index, &page, &slot);
	if (ret != 0)
		return ret;
	return pfsHashNodeViewGet(&page->view, key, slot, icv);
}

/* hold the last leaf */
int pfsHashNodesGetLast(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHashNodePage **page)
{
	SceUInt32 slot;
	int ret;

	if (nodes->held) {
		if (nodes->tail)
			goto held;
		ret = pfsHashNodesFlush(nodes, key);
		if (ret != 0)
			return ret;
	}
	ret = pfsHashNodesWalk(nodes, key, 0, 0, &nodes->page, &slot, 1);
	if (ret != SCE_PFS_ERROR_NODES_RANGE && ret != 0) {
		nodes->held = 0;
		nodes->dirty = 0;
		nodes->tail = 0;
		nodes->base = 0;
		return ret;
	}
	if (ret == SCE_PFS_ERROR_NODES_RANGE) {
		/* an empty root */
		ret = pfsHashNodePageLoad(&nodes->page, nodes->pages->lru->pageSize * nodes->root);
		if (ret != 0)
			return ret;
	}
	nodes->held = 1;
	nodes->tail = 1;
	nodes->dirty = 0;
	nodes->base = 0;
held:
	*page = &nodes->page;
	return 0;
}

int pfsHashNodesSet(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		    const ScePfsHash *icv, int *changed)
{
	ScePfsHashNodePage *page;
	SceUInt32 slot;
	int ret;

	if (nodes->root == -1)
		return SCE_PFS_ERROR_NODES_NONE;
	ret = pfsHashNodesGetLeaf(nodes, key, index, &page, &slot);
	if (ret != 0)
		return ret;
	ret = pfsHashNodeViewSet(&page->view, key, slot, icv, changed);
	if (ret == 0 && *changed) {
		page->acc.entry->flags |= PFS_LRU_DIRTY;
		nodes->dirty = 1;
	}
	return ret;
}

int pfsHashNodesAppend(ScePfsHashNodes *nodes, const ScePfsHash *key, const ScePfsHash *icv)
{
	ScePfsHashNodePage *last, node;
	ScePfsHash hash;
	SceUInt32 child, level, slot;
	int ret;

	if (nodes->root == -1)
		return SCE_PFS_ERROR_NODES_NONE;
	ret = pfsHashNodesGetLast(nodes, key, &last);
	if (ret != 0)
		return ret;
	if (pfsHashNodeChildren(&last->view) < PFS_HASH_NODE_FANOUT) {
		ret = pfsHashNodeViewAppend(&last->view, key, icv, -1);
		if (ret == 0) {
			last->acc.entry->flags |= PFS_LRU_DIRTY;
			nodes->dirty = 1;
		}
		return ret;
	}

	/* the last leaf is full: start new nodes up to the first that has room */
	ret = pfsHashNodesFlush(nodes, key);
	if (ret != 0)
		return ret;
	child = -1;
	pfsHashCopy(&hash, icv);
	pfsHashNodePageInit(&node, nodes->pages);
	for (level = 0; level < nodes->height; level++) {
		ret = pfsHashNodesWalk(nodes, key, 0, level, &node, &slot, 1);
		if (ret != 0)
			return ret;
		node.acc.entry->flags |= PFS_LRU_DIRTY;
		if (pfsHashNodeChildren(&node.view) < PFS_HASH_NODE_FANOUT) {
			ret = pfsHashNodeViewAppend(&node.view, key, &hash, child);
			if (ret != 0)
				return ret;
			break;
		}
		ret = pfsHashNodePageAlloc(&node);
		if (ret != 0)
			return ret;
		ret = pfsHashNodeViewAppend(&node.view, key, &hash, child);
		if (ret != 0)
			return ret;
		*node.view.level = level;
		pfsHashNodeHash(&hash, &node.view);
		child = pfsHashNodePageIndex(nodes, &node);
	}
	if (level < nodes->height) {
		ret = pfsHashNodesUpdateLast(nodes, key, &node);
		if (ret != 0)
			return ret;
		goto release;
	}

	/* a new root over the old one */
	ret = pfsHashNodePageAlloc(&node);
	if (ret != 0)
		return ret;
	ret = pfsHashNodeViewAppend(&node.view, key, &nodes->rootHash, nodes->root);
	if (ret != 0)
		return ret;
	ret = pfsHashNodeViewAppend(&node.view, key, &hash, child);
	if (ret != 0)
		return ret;
	nodes->root = pfsHashNodePageIndex(nodes, &node);
	pfsHashNodeHash(&nodes->rootHash, &node.view);
	*node.view.level = nodes->height;
	nodes->height++;
release:
	return pfsHashNodePageRelease(&node);
}

int pfsHashNodesRemoveLast(ScePfsHashNodes *nodes, const ScePfsHash *key)
{
	ScePfsHashNodePage *last, node;
	ScePfsHash hash;
	SceUInt32 child, level, slot;
	int ret;

	if (nodes->root == -1)
		return SCE_PFS_ERROR_NODES_NONE;
	ret = pfsHashNodesGetLast(nodes, key, &last);
	if (ret != 0)
		return ret;
	if (pfsHashNodeChildren(&last->view) >= 2) {
		ret = pfsHashNodeViewRemoveLast(&last->view, key);
		if (ret == 0) {
			last->acc.entry->flags |= PFS_LRU_DIRTY;
			nodes->dirty = 1;
		}
		return ret;
	}

	/* the last leaf empties: drop nodes up to the first that keeps a child */
	ret = pfsHashNodesFlush(nodes, key);
	if (ret != 0)
		return ret;
	pfsHashNodePageInit(&node, nodes->pages);
	for (level = 0; level < nodes->height; level++) {
		ret = pfsHashNodesWalk(nodes, key, 0, level, &node, &slot, 1);
		if (ret != 0)
			return ret;
		node.acc.entry->flags |= PFS_LRU_DIRTY;
		if (pfsHashNodeChildren(&node.view) != 1 || nodes->height < 2) {
			ret = pfsHashNodeViewRemoveLast(&node.view, key);
			if (ret != 0)
				return ret;
			break;
		}
		ret = pfsHashNodePageFree(&node);
		if (ret != 0)
			return ret;
	}
	if (level == nodes->height - 1 && pfsHashNodeChildren(&node.view) == 1 &&
	    nodes->height >= 2) {
		/* the root is left with one child, which becomes the root */
		ret = pfsHashNodeViewGetChild(&node.view, key, 0, &hash, &child);
		if (ret != 0)
			return ret;
		nodes->root = child;
		pfsHashCopy(&nodes->rootHash, &hash);
		ret = pfsHashNodePageFree(&node);
		if (ret != 0)
			return ret;
		nodes->height--;
	} else {
		ret = pfsHashNodesUpdateLast(nodes, key, &node);
		if (ret != 0)
			return ret;
	}
	return pfsHashNodePageRelease(&node);
}
