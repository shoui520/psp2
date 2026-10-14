/*
 * The tree of files.db: a B+ tree in the shape of the public bpt.c, kept in
 * pages of the file, whose nodes carry the hash of each child and whose root
 * hash the tree keeps, so that every walk from a leaf to the root checks it.
 */

#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

/* the name of a key, copied as a whole (name unknown) */
typedef struct ScePfsDbKeyName {
	char name[0x40];
} ScePfsDbKeyName;

static inline ScePfsDbNode *pfsDbNodeOf(ScePfsDbNodePage *page)	/* name unknown */
{
	return page->acc.entry->data;
}

static inline void pfsDbNodePageDirty(ScePfsDbNodePage *page)	/* name unknown */
{
	page->acc.entry->flags |= PFS_LRU_DIRTY;
}

/* the page index of the node page holds */
static inline SceUInt32 pfsDbTreePageIndex(ScePfsDbTree *tree, ScePfsDbNodePage *page)	/* name unknown */
{
	return page->offset / tree->pages->lru->pageSize;
}

static inline SceOff pfsDbTreePageOffset(ScePfsDbTree *tree, SceUInt32 index)	/* name unknown */
{
	return tree->pages->lru->pageSize * index;
}

static inline void pfsDbKeyClear(ScePfsDbRecordKey *rkey)	/* name unknown */
{
	memset(rkey, 0, sizeof(*rkey));
	rkey->id.name[0] = '\0';
}

static inline void pfsDbKeyCopy(ScePfsDbRecordKey *dst, const ScePfsDbRecordKey *src)	/* name unknown */
{
	dst->id.parentIndex = src->id.parentIndex;
	*(ScePfsDbKeyName *)dst->id.name = *(const ScePfsDbKeyName *)src->id.name;
}

static inline void pfsDbRecordClear(ScePfsDbRecord *record)	/* name unknown */
{
	memset(record, 0, sizeof(*record));
	record->index = -1;
}

/* bpt.c's cut(): where a node of length entries is split */
static inline SceUInt32 pfsDbCut(SceUInt32 length)	/* name unknown */
{
	if (length % 2 == 0)
		return length / 2;
	return length / 2 + 1;
}

/* a key as the tree compares it, its name decoded */
static inline int pfsDbTreeDecodeKey(ScePfsDbTree *tree, ScePfsDbEntryName *id,
				     const ScePfsDbRecordKey *rkey)	/* name unknown */
{
	const char *path2, *path3;
	SceSize len;
	int ret;

	if (rkey == NULL)
		return SCE_PFS_ERROR_DB_INVALID_ARG;
	id->parentIndex = rkey->id.parentIndex;
	*(ScePfsDbKeyName *)id->name = *(const ScePfsDbKeyName *)rkey->id.name;
	ret = ksceVfsOpDecodePathElem(tree->mnt, rkey->id.name, &path2, &path3, id->name,
				      sizeof(id->name), &len);
	if (ret != 0)
		return ret;
	if (len >= sizeof(id->name))
		return SCE_PFS_ERROR_DB_NAME_TOO_LONG;
	id->parentIndex = rkey->id.parentIndex;
	return 0;
}

static inline int pfsDbKeyLess(const ScePfsDbEntryName *a, const ScePfsDbEntryName *b)	/* name unknown */
{
	if (a->parentIndex == b->parentIndex)
		return strncmp(a->name, b->name, sizeof(a->name)) < 0;
	return a->parentIndex < b->parentIndex;
}

/* free the subtree under the node at page index */
int pfsDbTreeDestroy(ScePfsDbTree *tree, SceUInt32 index)
{
	ScePfsDbNodePage page;
	ScePfsDbNode *node;
	SceUInt32 i;
	int ret, err;

	pfsDbNodePageInit(&page, tree->pages);
	ret = pfsDbNodePageLoad(&page, pfsDbTreePageOffset(tree, index), tree->order);
	if (ret != 0)
		return ret;
	node = pfsDbNodeOf(&page);
	if (node == NULL) {
		ret = pfsDbNodePageRelease(&page);
		if (ret == 0)
			ret = SCE_PFS_ERROR_DB_NODE;
		return ret;
	}
	if (node->internal) {
		for (i = 0; i < node->count + 1; i++) {
			err = pfsDbTreeDestroy(tree, pfsDbNodeRecords(node, tree->order)[i].index);
			if (err != 0) {
				ret = pfsDbNodePageRelease(&page);
				if (ret == 0)
					ret = err;
				return ret;
			}
		}
	}
	ret = pfsDbNodePageFree(&page);
	switch (ret) {
	case 0:
		return 0;
	case SCE_PFS_ERROR_DB_INVALID_ARG:
		return SCE_PFS_ERROR_DB_NODE;
	default:
		return ret;
	}
}

/* the hash of a node: over its children's hashes, or over the whole node */
void pfsDbNodeHash(ScePfsDbNode *node, const ScePfsHash *key, ScePfsHash *hash, ScePfsDbTree *tree)
{
	ScePfsHash data[2];
	SceUInt32 count, i;

	if (tree->type == 5) {
		pfsHmacSha1(hash, key, &node->internal,
			    pfsDbNodeSize(tree->order) - sizeof(node->parent));
		return;
	}
	if (node->internal)
		count = node->count + 1;
	else
		count = node->count;
	pfsHashZero(hash);
	for (i = 0; i != count; i++) {
		data[0] = *hash;
		data[1] = pfsDbNodeHashes(node, tree->order)[i];
		pfsHmacSha1(hash, key, data, sizeof(data));
	}
}

/* rehash the node page holds and its ancestors, up to the tree's root hash */
int pfsDbTreeUpdateHashes(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page)
{
	ScePfsHash hash;
	ScePfsDbNode *node;
	ScePfsDbRecord *records;
	SceUInt32 index, slot;
	SceOff offset;
	int ret;

	pfsHashZero(&hash);
	offset = page->offset;
	if (offset >= 0) {
		node = pfsDbNodeOf(page);
		pfsDbNodeHash(node, key, &hash, tree);
		while (node->parent != -1) {
			ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree, node->parent),
						tree->order);
			if (ret != 0)
				return ret;
			pfsDbNodePageDirty(page);
			node = pfsDbNodeOf(page);
			if (node == NULL)
				return SCE_PFS_ERROR_DB_NODE;
			index = offset / tree->pages->lru->pageSize;
			records = pfsDbNodeRecords(node, tree->order);
			for (slot = 0; records[slot].index != index; )
				if (++slot > node->count)
					return SCE_PFS_ERROR_NOT_FOUND;
			pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[slot], &hash);
			pfsDbNodeHash(node, key, &hash, tree);
			offset = page->offset;
		}
	}
	pfsHashCopy(&tree->hash, &hash);
	return 0;
}

/* check the hashes from the node page holds up to the tree's root hash */
int pfsDbTreeCheckHashes(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page,
			 int *valid)
{
	ScePfsHash hash;
	ScePfsDbNode *node;
	ScePfsDbRecord *records;
	SceUInt32 index, slot;
	SceOff offset;
	int ret;

	offset = page->offset;
	if (offset < 0)
		return SCE_PFS_ERROR_DB_INVALID_ARG;
	node = pfsDbNodeOf(page);
	pfsDbNodeHash(node, key, &hash, tree);
	while (node->parent != -1) {
		ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree, node->parent), tree->order);
		if (ret != 0)
			return ret;
		node = pfsDbNodeOf(page);
		if (node == NULL)
			return SCE_PFS_ERROR_DB_NODE;
		index = offset / tree->pages->lru->pageSize;
		records = pfsDbNodeRecords(node, tree->order);
		for (slot = 0; records[slot].index != index; )
			if (++slot > node->count)
				return SCE_PFS_ERROR_NOT_FOUND;
		if (!pfsHashEqual(&pfsDbNodeHashes(node, tree->order)[slot], &hash)) {
			*valid = 0;
			return 0;
		}
		pfsDbNodeHash(node, key, &hash, tree);
		offset = page->offset;
	}
	if (!pfsHashEqual(&tree->hash, &hash)) {
		*valid = 0;
		return 0;
	}
	*valid = 1;
	return 0;
}

int pfsDbNodeCreate(const SceUInt32 *order, ScePfsDbNodePage *page, SceUInt32 internal)
{
	ScePfsDbNode *node;
	SceUInt32 i;
	int ret;

	ret = pfsDbNodePageAlloc(page);
	if (ret != 0)
		return ret;
	node = pfsDbNodeOf(page);
	pfsDbNodePageDirty(page);
	node->internal = internal;
	node->count = 0;
	node->parent = -1;
	for (i = 0; i < *order - 1; i++)
		pfsDbKeyClear(&node->keys[i]);
	for (i = 0; i < *order; i++) {
		pfsDbRecordClear(&pfsDbNodeRecords(node, *order)[i]);
		pfsHashZero(&pfsDbNodeHashes(node, *order)[i]);
	}
	return 0;
}

/* bpt.c's insert_into_parent(): rkey separates right, new, from left */
int pfsDbTreeInsertIntoParent(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *left,
			      ScePfsDbRecordKey *rkey, ScePfsDbNodePage *right)
{
	ScePfsDbNodePage parent, split;
	ScePfsDbNode *node, *p, *n, *o;
	ScePfsDbRecord *records;
	ScePfsDbRecordKey kprime;
	ScePfsHash hash, rhash;
	SceUInt32 order, index, slot, half, i, j, k, next;
	int ret, err;

	node = pfsDbNodeOf(left);
	if (node->parent == -1) {
		/* a new root */
		pfsDbNodePageInit(&split, tree->pages);
		ret = pfsDbNodeCreate(&tree->order, &split, 1);
		if (ret != 0)
			return ret;
		tree->root = pfsDbTreePageIndex(tree, &split);
		n = pfsDbNodeOf(&split);
		pfsDbKeyCopy(&n->keys[0], rkey);
		pfsDbNodeRecords(n, tree->order)[0].index = pfsDbTreePageIndex(tree, left);
		pfsDbNodeRecords(n, tree->order)[1].index = pfsDbTreePageIndex(tree, right);
		n->parent = -1;
		n->count++;
		pfsDbNodeOf(left)->parent = tree->root;
		pfsDbNodeOf(right)->parent = tree->root;
		pfsDbNodeHash(pfsDbNodeOf(left), key, &hash, tree);
		pfsDbNodeHash(pfsDbNodeOf(right), key, &rhash, tree);
		pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[0], &hash);
		pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[1], &rhash);
		err = pfsDbTreeUpdateHashes(tree, key, &split);
		ret = pfsDbNodePageRelease(&split);
		if (ret == 0)
			ret = err;
		return ret;
	}

	pfsDbNodePageInit(&parent, tree->pages);
	ret = pfsDbNodePageLoad(&parent, pfsDbTreePageOffset(tree, node->parent), tree->order);
	if (ret != 0)
		return ret;
	p = pfsDbNodeOf(&parent);
	pfsDbNodePageDirty(&parent);
	index = pfsDbTreePageIndex(tree, left);
	records = pfsDbNodeRecords(p, tree->order);
	for (slot = 0; records[slot].index != index; ) {
		if (++slot > p->count) {
			ret = pfsDbNodePageRelease(left);
			err = SCE_PFS_ERROR_NOT_FOUND;
			if (ret != 0)
				return ret;
			goto release;
		}
	}
	pfsDbNodeHash(node, key, &hash, tree);
	pfsHashCopy(&pfsDbNodeHashes(p, tree->order)[slot], &hash);
	ret = pfsDbNodePageRelease(left);
	if (ret != 0)
		return ret;

	order = tree->order;
	if (p->count < order - 1) {
		/* bpt.c's insert_into_node() */
		n = pfsDbNodeOf(&parent);
		for (i = n->count; i > slot; i--) {
			pfsDbNodeRecords(n, tree->order)[i + 1] = pfsDbNodeRecords(n, tree->order)[i];
			pfsDbKeyCopy(&n->keys[i], &n->keys[i - 1]);
			pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[i + 1],
				    &pfsDbNodeHashes(n, tree->order)[i]);
		}
		pfsDbNodeRecords(n, tree->order)[slot + 1].index = pfsDbTreePageIndex(tree, right);
		pfsDbKeyCopy(&n->keys[slot], rkey);
		n->count++;
		pfsDbNodeHash(pfsDbNodeOf(right), key, &rhash, tree);
		pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[slot + 1], &rhash);
		err = pfsDbTreeUpdateHashes(tree, key, &parent);
		goto release;
	}

	/* bpt.c's insert_into_node_after_splitting() */
	pfsDbNodePageInit(&split, tree->pages);
	err = pfsDbNodeCreate(&tree->order, &split, 1);
	if (err != 0)
		goto release;
	order = tree->order;
	n = pfsDbNodeOf(&split);
	o = pfsDbNodeOf(&parent);
	half = pfsDbCut(order);
	if (slot < half) {
		for (i = half - 1, j = 0; i < order - 1; i++, j++) {
			pfsDbKeyCopy(&n->keys[j], &o->keys[i]);
			n->count++;
			pfsDbKeyClear(&o->keys[i]);
			order = tree->order;
			o->count--;
		}
		k = slot == half - 1;
		for (i = k ? half : half - 1, j = k; i < order; i++, j++) {
			pfsDbNodeRecords(n, order)[j] = pfsDbNodeRecords(o, order)[i];
			pfsDbRecordClear(&pfsDbNodeRecords(o, tree->order)[i]);
			pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[j],
				    &pfsDbNodeHashes(o, tree->order)[i]);
			pfsHashZero(&pfsDbNodeHashes(o, tree->order)[i]);
			order = tree->order;
		}
		if (slot == half - 1) {
			pfsDbKeyCopy(&o->keys[slot], rkey);
			pfsDbRecordClear(&pfsDbNodeRecords(n, tree->order)[0]);
			pfsDbNodeRecords(n, tree->order)[0].index = pfsDbTreePageIndex(tree, right);
			pfsDbNodeHash(pfsDbNodeOf(right), key, &pfsDbNodeHashes(n, tree->order)[0],
				      tree);
		} else {
			for (i = half - 1; i > slot; i--)
				pfsDbKeyCopy(&o->keys[i], &o->keys[i - 1]);
			for (i = half - 1; i > slot + 1; i--) {
				pfsDbNodeRecords(o, tree->order)[i] =
					pfsDbNodeRecords(o, tree->order)[i - 1];
				pfsHashCopy(&pfsDbNodeHashes(o, tree->order)[i],
					    &pfsDbNodeHashes(o, tree->order)[i - 1]);
			}
			pfsDbKeyCopy(&o->keys[slot], rkey);
			pfsDbRecordClear(&pfsDbNodeRecords(o, tree->order)[slot + 1]);
			pfsDbNodeRecords(o, tree->order)[slot + 1].index =
				pfsDbTreePageIndex(tree, right);
			pfsDbNodeHash(pfsDbNodeOf(right), key,
				      &pfsDbNodeHashes(o, tree->order)[slot + 1], tree);
		}
		o->count++;
	} else {
		k = slot - half;
		for (i = half, j = 0; i < order - 1; i++, j++) {
			if (j == k)
				j = k + 1;
			pfsDbKeyCopy(&n->keys[j], &o->keys[i]);
			n->count++;
			pfsDbKeyClear(&o->keys[i]);
			order = tree->order;
			o->count--;
		}
		next = k + 1;
		for (i = half, j = 0; i < order; i++, j++) {
			if (j == next)
				j = next + 1;
			pfsDbNodeRecords(n, order)[j] = pfsDbNodeRecords(o, order)[i];
			pfsDbRecordClear(&pfsDbNodeRecords(o, tree->order)[i]);
			pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[j],
				    &pfsDbNodeHashes(o, tree->order)[i]);
			pfsHashZero(&pfsDbNodeHashes(o, tree->order)[i]);
			order = tree->order;
		}
		pfsDbKeyCopy(&n->keys[k], rkey);
		pfsDbRecordClear(&pfsDbNodeRecords(n, tree->order)[next]);
		pfsDbNodeRecords(n, tree->order)[next].index = pfsDbTreePageIndex(tree, right);
		pfsDbNodeHash(pfsDbNodeOf(right), key, &pfsDbNodeHashes(n, tree->order)[next], tree);
		n->count++;
	}
	pfsDbKeyCopy(&kprime, &o->keys[half - 1]);
	pfsDbKeyClear(&o->keys[half - 1]);
	o->count--;
	index = pfsDbTreePageIndex(tree, &split);
	n->parent = o->parent;
	for (i = 0; i <= n->count; i++) {
		err = pfsDbNodePageLoad(right, pfsDbTreePageOffset(tree,
					pfsDbNodeRecords(n, tree->order)[i].index), tree->order);
		if (err != 0)
			goto release;
		pfsDbNodePageDirty(right);
		pfsDbNodeOf(right)->parent = index;
	}
	err = pfsDbNodePageRelease(right);
	if (err == 0 && (err = pfsDbTreeInsertIntoParent(tree, key, &parent, &kprime, &split)) == 0)
		err = pfsDbNodePageRelease(&split);
release:
	ret = pfsDbNodePageRelease(&parent);
	if (ret == 0)
		ret = err;
	return ret;
}

/* where rkey is or would be in the node page holds */
int pfsDbNodeFind(ScePfsDbTree *tree, ScePfsDbNodePage *page, ScePfsDbRecordKey *rkey,
		  SceUInt32 *slot, int *found)
{
	ScePfsDbNode *node = pfsDbNodeOf(page);
	ScePfsDbEntryName sk, k;
	SceUInt32 i;
	int ret;

	ret = pfsDbTreeDecodeKey(tree, &sk, rkey);
	if (ret != 0)
		return ret;
	for (i = 0; i < node->count; i++) {
		ret = pfsDbTreeDecodeKey(tree, &k, &node->keys[i]);
		if (ret != 0)
			return ret;
		if (!pfsDbKeyLess(&k, &sk))
			break;
	}
	*slot = i;
	if (k.parentIndex == sk.parentIndex && strncmp(k.name, sk.name, sizeof(k.name)) == 0)
		*found = 1;
	else
		*found = 0;
	return 0;
}

/* bpt.c's remove_entry_from_node(): inner removes the record right of the key */
int pfsDbTreeRemoveEntry(ScePfsDbTree *tree, ScePfsDbNodePage *page, ScePfsDbRecordKey *rkey,
			 int inner)
{
	ScePfsDbRecordKey first;
	ScePfsDbNode *node;
	ScePfsDbRecord *records;
	SceUInt32 slot, count, n, i, index, s;
	SceOff offset, child;
	int found, ret, err;

	ret = pfsDbNodeFind(tree, page, rkey, &slot, &found);
	if (ret != 0)
		return ret;
	if (!found)
		return SCE_PFS_ERROR_NOT_FOUND;
	node = pfsDbNodeOf(page);
	count = node->count;
	for (i = slot + 1; i < count; i++) {
		pfsDbKeyCopy(&node->keys[i - 1], &node->keys[i]);
		count = node->count;
	}
	i = slot + 1;
	if (inner == 1) {
		i = slot + 2;
		slot++;
	}
	n = node->internal ? count + 1 : count;
	for (; i < n; i++) {
		pfsDbNodeRecords(node, tree->order)[i - 1] = pfsDbNodeRecords(node, tree->order)[i];
		pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[i - 1],
			    &pfsDbNodeHashes(node, tree->order)[i]);
		count = node->count;
	}
	pfsDbKeyClear(&node->keys[count - 1]);
	pfsDbRecordClear(&pfsDbNodeRecords(node, tree->order)[n - 1]);
	node->count--;
	if (slot != 0 || node->internal)
		return 0;

	/* the first key of a leaf left: it separates the leaf in an ancestor */
	offset = page->offset;
	if (offset < 0)
		return SCE_PFS_ERROR_DB_NODE;
	node = pfsDbNodeOf(page);
	pfsDbKeyCopy(&first, &node->keys[0]);
	child = offset;
	for (;;) {
		if (node->parent == -1) {
			err = 0;
			break;
		}
		ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree, node->parent), tree->order);
		if (ret != 0)
			return ret;
		pfsDbNodePageDirty(page);
		node = pfsDbNodeOf(page);
		if (node == NULL) {
			err = SCE_PFS_ERROR_DB_NODE;
			break;
		}
		index = child / tree->pages->lru->pageSize;
		records = pfsDbNodeRecords(node, tree->order);
		for (s = 0; records[s].index != index; )
			if (++s > node->count)
				break;
		if (s > node->count) {
			err = SCE_PFS_ERROR_NOT_FOUND;
			break;
		}
		if (s != 0) {
			pfsDbKeyCopy(&node->keys[s - 1], &first);
			err = 0;
			break;
		}
		child = page->offset;
	}
	ret = pfsDbNodePageLoad(page, offset, tree->order);
	if (ret != 0)
		return ret;
	pfsDbNodePageDirty(page);
	return err;
}

/* bpt.c's delete_entry() after the removal: adjust the root, coalesce or redistribute */
int pfsDbTreeDeleteEntry(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page)
{
	ScePfsDbNodePage parent, neighbor, child, *left, *right;
	ScePfsDbNode *node, *p, *l, *r;
	ScePfsDbRecord *records;
	ScePfsDbRecordKey kprime;
	ScePfsHash hash;
	SceUInt32 index, slot, kIndex, min, total, half, lc, rc, i;
	int move, j, ret, err;

	if (tree->root == pfsDbTreePageIndex(tree, page)) {
		/* bpt.c's adjust_root() */
		node = pfsDbNodeOf(page);
		if (node->count == 0) {
			if (!node->internal) {
				if (page->offset != -1)
					pfsDbNodePageFree(page);
				tree->root = -1;
			} else {
				tree->root = pfsDbNodeRecords(node, tree->order)[0].index;
				if (page->offset != -1)
					pfsDbNodePageFree(page);
				ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree, tree->root),
							tree->order);
				if (ret != 0)
					return ret;
				pfsDbNodePageDirty(page);
				pfsDbNodeOf(page)->parent = -1;
			}
		}
		return pfsDbTreeUpdateHashes(tree, key, page);
	}

	node = pfsDbNodeOf(page);
	if (node->internal)
		min = pfsDbCut(tree->order) - 1;
	else
		min = pfsDbCut(tree->order - 1);
	if (node->count >= min)
		return pfsDbTreeUpdateHashes(tree, key, page);

	pfsDbNodePageInit(&parent, tree->pages);
	ret = pfsDbNodePageLoad(&parent, pfsDbTreePageOffset(tree, node->parent), tree->order);
	if (ret != 0)
		return ret;
	p = pfsDbNodeOf(&parent);
	pfsDbNodePageDirty(&parent);
	index = pfsDbTreePageIndex(tree, page);
	records = pfsDbNodeRecords(p, tree->order);
	for (slot = 0; records[slot].index != index; ) {
		if (++slot > p->count) {
			ret = pfsDbNodePageRelease(&parent);
			if (ret == 0)
				ret = SCE_PFS_ERROR_NOT_FOUND;
			return ret;
		}
	}
	pfsDbNodePageInit(&neighbor, tree->pages);
	if (slot == 0) {
		ret = pfsDbNodePageLoad(&neighbor,
					pfsDbTreePageOffset(tree, pfsDbNodeRecords(p, tree->order)[1].index),
					tree->order);
		if (ret != 0)
			return ret;
		kIndex = 0;
		pfsDbNodePageDirty(&neighbor);
		left = page;
		right = &neighbor;
	} else {
		slot--;
		ret = pfsDbNodePageLoad(&neighbor,
					pfsDbTreePageOffset(tree, pfsDbNodeRecords(p, tree->order)[slot].index),
					tree->order);
		if (ret != 0)
			return ret;
		left = &neighbor;
		kIndex = slot;
		pfsDbNodePageDirty(&neighbor);
		right = page;
	}
	pfsDbKeyCopy(&kprime, &p->keys[kIndex]);
	ret = pfsDbNodePageRelease(&parent);
	if (ret != 0)
		return ret;

	l = pfsDbNodeOf(left);
	r = pfsDbNodeOf(right);
	lc = l->count;
	total = lc + r->count;
	if (!l->internal) {
		if (total >= tree->order)
			goto redistribute;
		/* bpt.c's coalesce_nodes() of leaves */
		for (i = 0; i < r->count; i++) {
			pfsDbKeyCopy(&l->keys[lc + i], &r->keys[i]);
			pfsDbNodeRecords(l, tree->order)[lc + i] = pfsDbNodeRecords(r, tree->order)[i];
			pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[lc + i],
				    &pfsDbNodeHashes(r, tree->order)[i]);
			l->count++;
		}
		pfsDbNodeRecords(l, tree->order)[tree->order - 1] =
			pfsDbNodeRecords(r, tree->order)[tree->order - 1];
	} else if (l->internal == 1 && total < tree->order - 1) {
		/* bpt.c's coalesce_nodes() of inner nodes */
		pfsDbKeyCopy(&l->keys[lc], &kprime);
		l->count++;
		for (i = 0; i < r->count; i++) {
			pfsDbKeyCopy(&l->keys[lc + 1 + i], &r->keys[i]);
			pfsDbNodeRecords(l, tree->order)[lc + 1 + i] =
				pfsDbNodeRecords(r, tree->order)[i];
			pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[lc + 1 + i],
				    &pfsDbNodeHashes(r, tree->order)[i]);
			l->count++;
		}
		pfsDbNodeRecords(l, tree->order)[l->count] =
			pfsDbNodeRecords(r, tree->order)[r->count];
		pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[l->count],
			    &pfsDbNodeHashes(r, tree->order)[r->count]);
		for (i = 0; i < l->count + 1; i++) {
			pfsDbNodePageInit(&child, tree->pages);
			err = pfsDbNodePageLoad(&child, pfsDbTreePageOffset(tree,
						pfsDbNodeRecords(l, tree->order)[i].index), tree->order);
			if (err != 0)
				goto release;
			pfsDbNodePageDirty(&child);
			pfsDbNodeOf(&child)->parent = pfsDbTreePageIndex(tree, left);
			err = pfsDbNodePageRelease(&child);
			if (err != 0)
				goto release;
		}
	} else {
		goto redistribute;
	}

	/* the right node is gone: take its key out of the parent */
	pfsDbNodePageInit(&parent, tree->pages);
	err = pfsDbNodePageLoad(&parent, pfsDbTreePageOffset(tree, l->parent), tree->order);
	if (err == 0) {
		pfsDbNodePageDirty(&parent);
		pfsDbNodeHash(pfsDbNodeOf(left), key, &hash, tree);
		pfsHashCopy(&pfsDbNodeHashes(pfsDbNodeOf(&parent), tree->order)[slot], &hash);
		err = pfsDbNodePageRelease(left);
		if (err == 0) {
			if (right->offset != -1)
				pfsDbNodePageFree(right);
			err = pfsDbTreeRemoveEntry(tree, &parent, &kprime, 1);
			if (err == 0)
				err = pfsDbTreeDeleteEntry(tree, key, &parent);
			ret = pfsDbNodePageRelease(&parent);
			if (ret != 0)
				err = ret;
		}
	}
	goto release;

redistribute:
	/* bpt.c's redistribute_nodes(), evening out the two nodes */
	pfsDbNodePageInit(&parent, tree->pages);
	err = pfsDbNodePageLoad(&parent, pfsDbTreePageOffset(tree, l->parent), tree->order);
	if (err != 0)
		goto release;
	lc = l->count;
	rc = r->count;
	p = pfsDbNodeOf(&parent);
	total = lc + rc;
	pfsDbNodePageDirty(&parent);
	if (r->internal == 1)
		total++;
	half = (total - 1) / 2;
	if ((int)half < (int)lc) {
		/* from the left node to the right one */
		move = lc - half;
		if (!r->internal) {
			for (j = rc - 1; j >= 0; j--) {
				pfsDbKeyCopy(&r->keys[j + move], &r->keys[j]);
				pfsDbNodeRecords(r, tree->order)[j + move] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j + move],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			for (j = 0; j != move; j++) {
				pfsDbKeyCopy(&r->keys[j], &l->keys[half + j]);
				pfsDbNodeRecords(r, tree->order)[j] =
					pfsDbNodeRecords(l, tree->order)[half + j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j],
					    &pfsDbNodeHashes(l, tree->order)[half + j]);
			}
			memcpy(&p->keys[kIndex], &r->keys[0], sizeof(p->keys[0]));
			for (i = half; i != lc; i++) {
				pfsDbKeyClear(&l->keys[i]);
				pfsDbRecordClear(&pfsDbNodeRecords(l, tree->order)[i]);
			}
		} else {
			pfsDbNodeRecords(r, tree->order)[move + rc] = pfsDbNodeRecords(r, tree->order)[rc];
			pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[move + rc],
				    &pfsDbNodeHashes(r, tree->order)[rc]);
			for (j = r->count - 1; j >= 0; j--) {
				pfsDbKeyCopy(&r->keys[j + move], &r->keys[j]);
				pfsDbNodeRecords(r, tree->order)[j + move] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j + move],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			pfsDbKeyCopy(&r->keys[move - 1], &kprime);
			pfsDbNodeRecords(r, tree->order)[move - 1] = pfsDbNodeRecords(l, tree->order)[lc];
			pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[move - 1],
				    &pfsDbNodeHashes(l, tree->order)[lc]);
			for (j = 0; j < (int)(lc - 1 - half); j++) {
				pfsDbKeyCopy(&r->keys[j], &l->keys[half + 1 + j]);
				pfsDbNodeRecords(r, tree->order)[j] =
					pfsDbNodeRecords(l, tree->order)[half + 1 + j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j],
					    &pfsDbNodeHashes(l, tree->order)[half + 1 + j]);
			}
			memcpy(&p->keys[kIndex], &l->keys[half], sizeof(p->keys[0]));
			for (j = 0; j < move; j++) {
				pfsDbNodePageInit(&child, tree->pages);
				err = pfsDbNodePageLoad(&child, pfsDbTreePageOffset(tree,
							pfsDbNodeRecords(r, tree->order)[j].index),
							tree->order);
				if (err != 0)
					goto release;
				pfsDbNodePageDirty(&child);
				pfsDbNodeOf(&child)->parent = pfsDbTreePageIndex(tree, right);
				err = pfsDbNodePageRelease(&child);
				if (err != 0)
					goto release;
			}
			for (i = half; i != lc; i++) {
				pfsDbKeyClear(&l->keys[i]);
				pfsDbRecordClear(&pfsDbNodeRecords(l, tree->order)[i + 1]);
			}
		}
		l->count -= move;
		r->count += move;
	} else {
		/* from the right node to the left one */
		move = half - lc;
		if (!r->internal) {
			for (j = 0; j < move; j++) {
				pfsDbKeyCopy(&l->keys[lc + j], &r->keys[j]);
				pfsDbNodeRecords(l, tree->order)[lc + j] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[lc + j],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			for (j = move; j < (int)r->count; j++) {
				pfsDbKeyCopy(&r->keys[j - move], &r->keys[j]);
				pfsDbNodeRecords(r, tree->order)[j - move] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j - move],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			pfsDbKeyCopy(&p->keys[kIndex], &r->keys[0]);
			for (j = r->count - move; j < (int)r->count; j++) {
				pfsDbKeyClear(&r->keys[j]);
				pfsDbRecordClear(&pfsDbNodeRecords(r, tree->order)[j]);
			}
		} else {
			pfsDbKeyCopy(&l->keys[lc], &kprime);
			for (j = 0; j < move - 1; j++) {
				pfsDbKeyCopy(&l->keys[lc + 1 + j], &r->keys[j]);
				pfsDbNodeRecords(l, tree->order)[lc + 1 + j] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[lc + 1 + j],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			pfsDbNodeRecords(l, tree->order)[half] = pfsDbNodeRecords(r, tree->order)[move - 1];
			pfsHashCopy(&pfsDbNodeHashes(l, tree->order)[half],
				    &pfsDbNodeHashes(r, tree->order)[move - 1]);
			memcpy(&p->keys[kIndex], &r->keys[move - 1], sizeof(p->keys[0]));
			for (j = move; j < (int)r->count; j++) {
				pfsDbKeyCopy(&r->keys[j - move], &r->keys[j]);
				pfsDbNodeRecords(r, tree->order)[j - move] =
					pfsDbNodeRecords(r, tree->order)[j];
				pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[j - move],
					    &pfsDbNodeHashes(r, tree->order)[j]);
			}
			pfsDbNodeRecords(r, tree->order)[r->count - move] =
				pfsDbNodeRecords(r, tree->order)[r->count];
			pfsHashCopy(&pfsDbNodeHashes(r, tree->order)[r->count - move],
				    &pfsDbNodeHashes(r, tree->order)[r->count]);
			for (j = lc + 1; j <= (int)half; j++) {
				pfsDbNodePageInit(&child, tree->pages);
				err = pfsDbNodePageLoad(&child, pfsDbTreePageOffset(tree,
							pfsDbNodeRecords(l, tree->order)[j].index),
							tree->order);
				if (err != 0)
					goto release;
				pfsDbNodePageDirty(&child);
				pfsDbNodeOf(&child)->parent = pfsDbTreePageIndex(tree, left);
				err = pfsDbNodePageRelease(&child);
				if (err != 0)
					goto release;
			}
			for (j = r->count - move; j < (int)r->count; j++) {
				pfsDbKeyClear(&r->keys[j]);
				pfsDbRecordClear(&pfsDbNodeRecords(r, tree->order)[j + 1]);
			}
		}
		l->count += move;
		r->count -= move;
	}
	pfsDbNodeHash(pfsDbNodeOf(left), key, &hash, tree);
	pfsHashCopy(&pfsDbNodeHashes(p, tree->order)[slot], &hash);
	pfsDbNodeHash(pfsDbNodeOf(right), key, &hash, tree);
	pfsHashCopy(&pfsDbNodeHashes(p, tree->order)[slot + 1], &hash);
	err = pfsDbTreeUpdateHashes(tree, key, &parent);
	ret = pfsDbNodePageRelease(&parent);
	if (ret != 0)
		err = ret;
release:
	ret = pfsDbNodePageRelease(&neighbor);
	if (ret == 0)
		ret = err;
	return ret;
}

/* bpt.c's find_leaf(): the leftmost leaf when rkey is NULL */
int pfsDbTreeFindLeaf(ScePfsDbTree *tree, ScePfsDbRecordKey *rkey, ScePfsDbNodePage *page)
{
	ScePfsDbEntryName sk, k;
	ScePfsDbNode *node;
	SceUInt32 i;
	int ret;

	if (tree->root == -1)
		return SCE_PFS_ERROR_DB_END;
	ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree, tree->root), tree->order);
	if (ret != 0)
		return ret;
	node = pfsDbNodeOf(page);
	if (node == NULL) {
		ret = pfsDbNodePageRelease(page);
		if (ret == 0)
			ret = SCE_PFS_ERROR_DB_NODE;
		return ret;
	}
	while (node->internal == 1) {
		i = 0;
		if (rkey != NULL) {
			ret = pfsDbTreeDecodeKey(tree, &sk, rkey);
			if (ret != 0)
				return ret;
			for (; i < node->count; i++) {
				ret = pfsDbTreeDecodeKey(tree, &k, &node->keys[i]);
				if (ret != 0)
					return ret;
				if (pfsDbKeyLess(&sk, &k))
					break;
			}
		}
		ret = pfsDbNodePageLoad(page, pfsDbTreePageOffset(tree,
					pfsDbNodeRecords(node, tree->order)[i].index), tree->order);
		if (ret != 0)
			return ret;
		node = pfsDbNodeOf(page);
	}
	return 0;
}

/* the levels of the tree */
int pfsDbTreeHeight(ScePfsDbTree *tree, SceUInt32 *height)
{
	ScePfsDbNodePage page;
	ScePfsDbNode *node;
	int ret, err;

	if (tree->root == -1) {
		*height = 0;
		return 0;
	}
	pfsDbNodePageInit(&page, tree->pages);
	ret = pfsDbNodePageLoad(&page, pfsDbTreePageOffset(tree, tree->root), tree->order);
	if (ret != 0)
		return ret;
	node = pfsDbNodeOf(&page);
	if (node == NULL) {
		err = SCE_PFS_ERROR_DB_NODE;
	} else {
		*height = 1;
		while (node->internal == 1) {
			(*height)++;
			ret = pfsDbNodePageLoad(&page, pfsDbTreePageOffset(tree,
						pfsDbNodeRecords(node, tree->order)[0].index),
						tree->order);
			if (ret != 0)
				return ret;
			node = pfsDbNodeOf(&page);
		}
		err = 0;
	}
	ret = pfsDbNodePageRelease(&page);
	if (ret == 0)
		ret = err;
	return ret;
}

int pfsDbTreeFind(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey,
		  ScePfsDbRecord *record, ScePfsHash *hash)
{
	ScePfsDbNodePage page;
	ScePfsDbNode *node;
	SceUInt32 slot;
	int found, valid, ret, err;

	pfsDbNodePageInit(&page, tree->pages);
	ret = pfsDbTreeFindLeaf(tree, rkey, &page);
	if (ret != 0) {
		if (ret == SCE_PFS_ERROR_DB_END)
			ret = SCE_PFS_ERROR_NOT_FOUND;
		return ret;
	}
	err = pfsDbNodeFind(tree, &page, rkey, &slot, &found);
	if (err == 0) {
		if (!found) {
			ret = pfsDbNodePageRelease(&page);
			if (ret == 0)
				ret = SCE_PFS_ERROR_NOT_FOUND;
			return ret;
		}
		node = pfsDbNodeOf(&page);
		*record = pfsDbNodeRecords(node, tree->order)[slot];
		pfsHashCopy(hash, &pfsDbNodeHashes(pfsDbNodeOf(&page), tree->order)[slot]);
		err = pfsDbTreeCheckHashes(tree, key, &page, &valid);
		if (err == 0 && !valid)
			err = SCE_PFS_ERROR_DB_TREE_HASH;
	}
	ret = pfsDbNodePageRelease(&page);
	if (ret == 0)
		ret = err;
	return ret;
}

/* the entry after after, the first when after is NULL */
int pfsDbTreeNext(ScePfsDbTree *tree, ScePfsDbRecordKey *after, ScePfsDbRecordKey *rkey,
		  ScePfsDbRecord *record)
{
	ScePfsDbNodePage page;
	ScePfsDbNode *node;
	SceUInt32 slot, next;
	int found, ret, err;

	pfsDbNodePageInit(&page, tree->pages);
	ret = pfsDbTreeFindLeaf(tree, after, &page);
	if (ret != 0)
		return ret;
	node = pfsDbNodeOf(&page);
	if (after == NULL) {
		if (node->count == 0) {
			err = SCE_PFS_ERROR_DB_END;
			goto release;
		}
		slot = 0;
	} else {
		err = pfsDbNodeFind(tree, &page, after, &slot, &found);
		if (err != 0)
			goto release;
		if (!found) {
			err = SCE_PFS_ERROR_NOT_FOUND;
			goto release;
		}
		if (slot < node->count - 1) {
			slot++;
		} else {
			next = pfsDbNodeRecords(node, tree->order)[tree->order - 1].index;
			if (next == -1) {
				err = SCE_PFS_ERROR_DB_END;
				goto release;
			}
			ret = pfsDbNodePageLoad(&page, pfsDbTreePageOffset(tree, next), tree->order);
			if (ret != 0)
				return ret;
			node = pfsDbNodeOf(&page);
			slot = 0;
		}
	}
	pfsDbKeyCopy(rkey, &node->keys[slot]);
	*record = pfsDbNodeRecords(node, tree->order)[slot];
	err = 0;
release:
	ret = pfsDbNodePageRelease(&page);
	if (ret == 0)
		ret = err;
	return ret;
}

int pfsDbTreeClear(ScePfsDbTree *tree)
{
	int ret;

	if (tree->root != -1) {
		ret = pfsDbTreeDestroy(tree, tree->root);
		if (ret != 0)
			return ret;
		tree->root = -1;
	}
	pfsHashZero(&tree->hash);
	return 0;
}

int pfsDbTreeInit(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsPageAllocator *pages,
		  SceUInt32 root, SceUInt32 type)
{
	ScePfsDbNodePage page;
	SceSize pageSize;
	SceUInt32 order;
	int ret;

	tree->pages = pages;
	tree->root = root;
	pageSize = pages->lru->pageSize;
	for (order = 1; pfsDbNodeSize(order) < pageSize; order++)
		;
	if (pfsDbNodeSize(order) > pageSize)
		order--;
	tree->order = order;
	tree->mnt = pages->lf->vp->core.mnt;
	tree->type = type;
	if (root == -1)
		return pfsDbTreeClear(tree);
	pfsDbNodePageInit(&page, pages);
	ret = pfsDbNodePageLoad(&page, pfsDbTreePageOffset(tree, tree->root), tree->order);
	if (ret != 0)
		return ret;
	pfsDbNodeHash(pfsDbNodeOf(&page), key, &tree->hash, tree);
	return pfsDbNodePageRelease(&page);
}

/* bpt.c's insert_into_leaf() */
static inline int pfsDbTreeInsertIntoLeaf(ScePfsDbTree *tree, const ScePfsHash *key,
					  ScePfsDbNodePage *leaf, ScePfsDbRecordKey *rkey,
					  ScePfsDbRecord *record, const ScePfsHash *hash)	/* name unknown */
{
	ScePfsDbNode *node = pfsDbNodeOf(leaf);
	ScePfsDbEntryName sk, k;
	SceUInt32 slot, i;
	int ret;

	ret = pfsDbTreeDecodeKey(tree, &sk, rkey);
	if (ret != 0)
		return ret;
	for (slot = 0; slot < node->count; slot++) {
		ret = pfsDbTreeDecodeKey(tree, &k, &node->keys[slot]);
		if (ret != 0)
			return ret;
		if (!pfsDbKeyLess(&k, &sk))
			break;
	}
	for (i = node->count; i > slot; i--) {
		pfsDbKeyCopy(&node->keys[i], &node->keys[i - 1]);
		pfsDbNodeRecords(node, tree->order)[i] = pfsDbNodeRecords(node, tree->order)[i - 1];
		pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[i],
			    &pfsDbNodeHashes(node, tree->order)[i - 1]);
	}
	pfsDbKeyCopy(&node->keys[slot], rkey);
	pfsDbNodeRecords(node, tree->order)[slot] = *record;
	node->count++;
	pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[slot], hash);
	return pfsDbTreeUpdateHashes(tree, key, leaf);
}

/* bpt.c's insert_into_leaf_after_splitting() */
static inline int pfsDbTreeSplitLeaf(ScePfsDbTree *tree, const ScePfsHash *key,
				     ScePfsDbNodePage *leaf, ScePfsDbRecordKey *rkey,
				     ScePfsDbRecord *record, const ScePfsHash *hash)	/* name unknown */
{
	ScePfsDbNodePage split;
	ScePfsDbNode *n, *o;
	ScePfsDbEntryName sk, k;
	ScePfsDbRecordKey sep;
	SceSize pageSize;
	SceUInt32 slot, half, i, j, at;
	int ret, err;

	pfsDbNodePageInit(&split, tree->pages);
	ret = pfsDbNodeCreate(&tree->order, &split, 0);
	if (ret != 0)
		return ret;
	n = pfsDbNodeOf(&split);
	o = pfsDbNodeOf(leaf);
	pageSize = tree->pages->lru->pageSize;
	ret = pfsDbTreeDecodeKey(tree, &sk, rkey);
	if (ret != 0)
		return ret;
	for (slot = 0; slot < tree->order - 1; slot++) {
		ret = pfsDbTreeDecodeKey(tree, &k, &o->keys[slot]);
		if (ret != 0)
			return ret;
		if (!pfsDbKeyLess(&k, &sk))
			break;
	}
	half = pfsDbCut(tree->order - 1);
	if (slot < half) {
		for (i = half - 1, j = 0; i < tree->order - 1; i++, j++) {
			pfsDbKeyCopy(&n->keys[j], &o->keys[i]);
			n->count++;
			pfsDbKeyClear(&o->keys[i]);
			o->count--;
			pfsDbNodeRecords(n, tree->order)[j] = pfsDbNodeRecords(o, tree->order)[i];
			pfsDbRecordClear(&pfsDbNodeRecords(o, tree->order)[i]);
			pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[j],
				    &pfsDbNodeHashes(o, tree->order)[i]);
			pfsHashZero(&pfsDbNodeHashes(o, tree->order)[i]);
		}
		for (i = half - 1; i > slot; i--) {
			pfsDbKeyCopy(&o->keys[i], &o->keys[i - 1]);
			pfsDbNodeRecords(o, tree->order)[i] = pfsDbNodeRecords(o, tree->order)[i - 1];
			pfsHashCopy(&pfsDbNodeHashes(o, tree->order)[i],
				    &pfsDbNodeHashes(o, tree->order)[i - 1]);
		}
		pfsDbKeyCopy(&o->keys[slot], rkey);
		pfsDbNodeRecords(o, tree->order)[slot] = *record;
		pfsHashCopy(&pfsDbNodeHashes(o, tree->order)[slot], hash);
		o->count++;
	} else {
		at = slot - half;
		for (i = half, j = 0; i < tree->order - 1; i++, j++) {
			if (j == at)
				j = at + 1;
			pfsDbKeyCopy(&n->keys[j], &o->keys[i]);
			n->count++;
			pfsDbKeyClear(&o->keys[i]);
			o->count--;
			pfsDbNodeRecords(n, tree->order)[j] = pfsDbNodeRecords(o, tree->order)[i];
			pfsDbRecordClear(&pfsDbNodeRecords(o, tree->order)[i]);
			pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[j],
				    &pfsDbNodeHashes(o, tree->order)[i]);
			pfsHashZero(&pfsDbNodeHashes(o, tree->order)[i]);
		}
		pfsDbKeyCopy(&n->keys[at], rkey);
		pfsDbNodeRecords(n, tree->order)[at] = *record;
		pfsHashCopy(&pfsDbNodeHashes(n, tree->order)[at], hash);
		n->count++;
	}
	/* the next leaf link */
	pfsDbNodeRecords(n, tree->order)[tree->order - 1] =
		pfsDbNodeRecords(o, tree->order)[tree->order - 1];
	pfsDbNodeRecords(o, tree->order)[tree->order - 1].index = split.offset / pageSize;
	pfsDbKeyCopy(&sep, &n->keys[0]);
	n->parent = o->parent;
	err = pfsDbTreeInsertIntoParent(tree, key, leaf, &sep, &split);
	ret = pfsDbNodePageRelease(&split);
	if (ret != 0)
		err = ret;
	return err;
}

/* the record of an entry already in the leaf */
static inline int pfsDbTreeUpdateLeaf(ScePfsDbTree *tree, const ScePfsHash *key,
				      ScePfsDbNodePage *leaf, ScePfsDbRecordKey *rkey,
				      ScePfsDbRecord *record, const ScePfsHash *hash)	/* name unknown */
{
	ScePfsDbNode *node = pfsDbNodeOf(leaf);
	ScePfsDbEntryName sk, k;
	SceUInt32 slot;
	int ret;

	ret = pfsDbTreeDecodeKey(tree, &sk, rkey);
	if (ret != 0)
		return ret;
	for (slot = 0; slot < node->count; slot++) {
		ret = pfsDbTreeDecodeKey(tree, &k, &node->keys[slot]);
		if (ret != 0)
			return ret;
		if (!pfsDbKeyLess(&k, &sk))
			break;
	}
	if (k.parentIndex != sk.parentIndex || strncmp(k.name, sk.name, sizeof(k.name)) != 0)
		return SCE_PFS_ERROR_NOT_FOUND;
	pfsDbKeyCopy(&node->keys[slot], rkey);
	pfsDbNodeRecords(node, tree->order)[slot] = *record;
	pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[slot], hash);
	return pfsDbTreeUpdateHashes(tree, key, leaf);
}

/* add (mode PFS_DB_TREE_INSERT), change (PFS_DB_TREE_UPDATE) or set an entry */
int pfsDbTreeSet(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey,
		 ScePfsDbRecord *record, const ScePfsHash *hash, int mode)
{
	ScePfsDbNodePage leaf;
	ScePfsDbNode *node;
	SceUInt32 slot;
	SceOff offset;
	int found, valid, ret, err;

	if (tree->root == -1) {
		/* bpt.c's start_new_tree() */
		pfsDbNodePageInit(&leaf, tree->pages);
		ret = pfsDbNodeCreate(&tree->order, &leaf, 0);
		if (ret != 0)
			return ret;
		tree->root = pfsDbTreePageIndex(tree, &leaf);
		node = pfsDbNodeOf(&leaf);
		pfsDbKeyCopy(&node->keys[0], rkey);
		pfsDbNodeRecords(node, tree->order)[0] = *record;
		pfsDbRecordClear(&pfsDbNodeRecords(node, tree->order)[tree->order - 1]);
		node->count++;
		pfsHashCopy(&pfsDbNodeHashes(node, tree->order)[0], hash);
		err = pfsDbTreeUpdateHashes(tree, key, &leaf);
		ret = pfsDbNodePageRelease(&leaf);
		if (ret == 0)
			ret = err;
		return ret;
	}

	pfsDbNodePageInit(&leaf, tree->pages);
	ret = pfsDbTreeFindLeaf(tree, rkey, &leaf);
	if (ret != 0)
		return ret;
	offset = leaf.offset;
	err = pfsDbTreeCheckHashes(tree, key, &leaf, &valid);
	if (err == 0) {
		if (!valid) {
			err = SCE_PFS_ERROR_DB_TREE_HASH;
		} else {
			ret = pfsDbNodePageLoad(&leaf, offset, tree->order);
			if (ret != 0)
				return ret;
			pfsDbNodePageDirty(&leaf);
			ret = pfsDbNodeFind(tree, &leaf, rkey, &slot, &found);
			if (ret != 0)
				return ret;
			if (!found) {
				if (mode & PFS_DB_TREE_UPDATE)
					err = SCE_PFS_ERROR_NOT_FOUND;
				else if (pfsDbNodeOf(&leaf)->count < tree->order - 1)
					err = pfsDbTreeInsertIntoLeaf(tree, key, &leaf, rkey, record,
								      hash);
				else
					err = pfsDbTreeSplitLeaf(tree, key, &leaf, rkey, record, hash);
			} else if (mode & PFS_DB_TREE_INSERT) {
				err = SCE_PFS_ERROR_DB_EXISTS;
			} else {
				err = pfsDbTreeUpdateLeaf(tree, key, &leaf, rkey, record, hash);
			}
		}
	}
	ret = pfsDbNodePageRelease(&leaf);
	if (ret == 0)
		ret = err;
	return ret;
}

int pfsDbTreeRemove(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey)
{
	ScePfsDbNodePage page;
	SceOff offset;
	int valid, ret, err;

	pfsDbNodePageInit(&page, tree->pages);
	ret = pfsDbTreeFindLeaf(tree, rkey, &page);
	if (ret != 0) {
		if (ret == SCE_PFS_ERROR_DB_END)
			ret = SCE_PFS_ERROR_NOT_FOUND;
		return ret;
	}
	offset = page.offset;
	err = pfsDbTreeCheckHashes(tree, key, &page, &valid);
	if (err == 0) {
		if (!valid) {
			err = SCE_PFS_ERROR_DB_TREE_HASH;
		} else {
			ret = pfsDbNodePageLoad(&page, offset, tree->order);
			if (ret != 0)
				return ret;
			pfsDbNodePageDirty(&page);
			err = pfsDbTreeRemoveEntry(tree, &page, rkey, 0);
			if (err == 0)
				err = pfsDbTreeDeleteEntry(tree, key, &page);
		}
	}
	ret = pfsDbNodePageRelease(&page);
	if (ret == 0)
		ret = err;
	return ret;
}
