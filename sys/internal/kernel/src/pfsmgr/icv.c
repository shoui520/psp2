/*
 * Icv trees kept in a file's own "%08x.icv" file: kind 1 is a hash tree over
 * the file's blocks, kind 2 a record without icvs ("SCEINULL").
 */

#include "pfsmgr_internal.h"

#define PFS_HASH_TREE_HEADER_SIZE	0x40	/* name unknown */

static inline SceUInt64 pfsRoundUp64(SceUInt64 x, SceUInt64 align)	/* name unknown */
{
	return x + (align - x % align) % align;
}

static inline SceUInt32 pfsRoundUp32(SceUInt32 x, SceUInt32 align)	/* name unknown */
{
	return x + (align - x % align) % align;
}

/* kind 1: a hash tree */

static void pfsIcvTreeGetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->tree.header.count;
}

static SceSize pfsIcvTreeBlockSize(ScePfsCrypt *ctx)
{
	return ctx->tree.header.blockSize;
}

static SceOff pfsIcvTreeIcvSize(ScePfsCrypt *ctx, SceOff size)
{
	SceUInt32 blocks, fanout, span, nodes, levels, i;

	blocks = pfsRoundUp64(size, ctx->tree.header.blockSize) / ctx->tree.header.blockSize;
	fanout = ctx->tree.nodes.fanout;
	nodes = 0;
	if (blocks >= 2) {
		span = 1;
		levels = 0;
		do {
			span *= fanout;
			levels++;
		} while (span < blocks);
		span = fanout;
		for (i = 0; i < levels; i++) {
			nodes += pfsRoundUp32(blocks, span) / span;
			span *= fanout;
		}
	}
	return (SceUInt64)nodes * ctx->tree.header.nodeSize + PFS_HASH_TREE_HEADER_SIZE;
}

static void pfsIcvTreeHashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				const void *data, SceSize len)
{
	/* PLACEHOLDER: cryptographic routine, deferred */
	memset(icv, 0, sizeof(*icv));
}

static int pfsIcvTreeGetHash(ScePfsCrypt *ctx, void *hash)
{
	return pfsHashTreeRootHash(&ctx->tree, hash);
}

static int pfsIcvTreeResize(ScePfsCrypt *ctx, SceUInt32 count)
{
	SceUInt32 n = ctx->tree.header.count;
	ScePfsHash zero;
	int ret;

	if (count < n) {
		do {
			ret = pfsHashTreeRemoveLast(&ctx->tree);
			if (ret != 0)
				break;
			n--;
		} while (count < n);
		return ret;
	}
	pfsHashZero(&zero);
	for (; n < count; n++) {
		ret = pfsHashTreeAppend(&ctx->tree, &zero);
		if (ret != 0)
			return ret;
	}
	return 0;
}

static int pfsIcvTreeGetIcv(ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv)
{
	return pfsHashTreeGet(&ctx->tree, index, icv);
}

static int pfsIcvTreeSetIcv(ScePfsCrypt *ctx, SceUInt32 index, const ScePfsHash *icv)
{
	int ret;

	if (index < ctx->tree.header.count)
		return pfsHashTreeSet(&ctx->tree, index, icv);
	if (index > ctx->tree.header.count) {
		ret = pfsIcvTreeResize(ctx, index);
		if (ret != 0)
			return ret;
	}
	return pfsHashTreeAppend(&ctx->tree, icv);
}

static int pfsIcvTreeFini(ScePfsCrypt *ctx)
{
	int ret;

	ret = pfsHashTreeFlush(&ctx->tree);
	if (ret != 0)
		return ret;
	return pfsLruFlush(ctx->lru, ctx->lf, 0);
}

static inline void pfsIcvTreeSetup(ScePfsCrypt *ctx, ScePfsLru *lru, ScePfsLowerFile *lf)	/* name unknown */
{
	ctx->icvSize = pfsIcvTreeIcvSize;
	ctx->lf = lf;
	ctx->lru = lru;
	ctx->fini = pfsIcvTreeFini;
	ctx->getIcv = pfsIcvTreeGetIcv;
	ctx->setIcv = pfsIcvTreeSetIcv;
	ctx->getCount = pfsIcvTreeGetCount;
	ctx->resize = pfsIcvTreeResize;
	ctx->blockSize = pfsIcvTreeBlockSize;
	ctx->getHash = pfsIcvTreeGetHash;
	ctx->hashBlock = pfsIcvTreeHashBlock;
}

int pfsIcvTreeCreate(ScePfsCrypt *ctx, ScePfsLru *lru, ScePfsLowerFile *lf, SceSize blockSize,
		     const void *key, SceUInt32 version)
{
	int ret;

	pfsIcvTreeSetup(ctx, lru, lf);
	pfsHashTreeInit(&ctx->tree, lru);
	ret = pfsHashTreeCreate(&ctx->tree, ctx->lf, key, blockSize, version);
	if (ret != 0)
		return ret;
	return pfsIcvTreeFini(ctx);
}

int pfsIcvTreeOpen(ScePfsCrypt *ctx, ScePfsLru *lru, ScePfsLowerFile *lf, const void *key,
		   SceUInt64 size, SceUInt32 version)
{
	SceUInt32 count, expected;
	SceSize blockSize;
	int ret;

	pfsIcvTreeSetup(ctx, lru, lf);
	pfsHashTreeInit(&ctx->tree, lru);
	ret = pfsHashTreeOpen(&ctx->tree, ctx->lf, key, version);
	if (ret != 0)
		return ret;
	blockSize = ctx->blockSize(ctx);
	expected = size / blockSize;
	if (size % blockSize != 0)
		expected++;
	ctx->getCount(ctx, &count);
	if (count != expected)
		return SCE_PFS_ERROR_ICV_COUNT;
	return 0;
}

/* kind 2: no icvs */

static void pfsIcvFlatGetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->flat.record.count;
}

static int pfsIcvFlatResize(ScePfsCrypt *ctx, SceUInt32 count)
{
	if (count < ctx->flat.record.count || count > ctx->flat.record.count) {
		ctx->flat.record.count = count;
		ctx->flat.dirty = 1;
	}
	return 0;
}

static SceSize pfsIcvFlatBlockSize(ScePfsCrypt *ctx)
{
	return ctx->flat.record.blockSize;
}

static SceOff pfsIcvFlatIcvSize(ScePfsCrypt *ctx, SceOff size)
{
	return sizeof(ScePfsIcvFlatRecord);
}

static int pfsIcvFlatGetIcv(ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv)
{
	memset(icv, 0, sizeof(*icv));
	return 0;
}

static void pfsIcvFlatHashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				const void *data, SceSize len)
{
	memset(icv, 0, sizeof(*icv));
}

static int pfsIcvFlatGetHash(ScePfsCrypt *ctx, void *hash)
{
	/* PLACEHOLDER: cryptographic routine, deferred */
	memset(hash, 0, PFS_HASH_SIZE);
	return 0;
}

static int pfsIcvFlatSetIcv(ScePfsCrypt *ctx, SceUInt32 index, const ScePfsHash *icv)
{
	SceUInt32 count = ctx->flat.record.count;

	if (count <= index) {
		ctx->flat.dirty = 1;
		ctx->flat.record.count = (index >= count ? index : count) + 1;
	}
	return 0;
}

static int pfsIcvFlatFini(ScePfsCrypt *ctx)
{
	SceSize n;
	int ret;

	if (ctx->flat.dirty) {
		ret = pfsLowerPwrite(ctx->flat.lf, &ctx->flat.record, sizeof(ctx->flat.record), 0,
				     &n);
		if (ret != 0)
			return ret;
		ctx->flat.dirty = 0;
	}
	return 0;
}

static inline void pfsIcvFlatSetup(ScePfsCrypt *ctx, ScePfsLowerFile *lf)	/* name unknown */
{
	ctx->lf = lf;
	ctx->fini = pfsIcvFlatFini;
	ctx->getIcv = pfsIcvFlatGetIcv;
	ctx->getCount = pfsIcvFlatGetCount;
	ctx->resize = pfsIcvFlatResize;
	ctx->blockSize = pfsIcvFlatBlockSize;
	ctx->getHash = pfsIcvFlatGetHash;
	ctx->hashBlock = pfsIcvFlatHashBlock;
	ctx->flat.lf = lf;
	ctx->icvSize = pfsIcvFlatIcvSize;
	ctx->setIcv = pfsIcvFlatSetIcv;
	ctx->lru = NULL;
	ctx->flat.dirty = 0;
	ctx->flat.record.version = 0;
	ctx->flat.record.magic = 0;
}

int pfsIcvFlatCreate(ScePfsCrypt *ctx, ScePfsLowerFile *lf, SceSize blockSize, const void *key,
		     SceUInt32 version)
{
	SceSize n;
	int ret;

	pfsIcvFlatSetup(ctx, lf);
	ret = pfsLowerSetSize(lf, sizeof(ScePfsIcvFlatRecord));
	if (ret != 0)
		return ret;
	ctx->flat.record.version = version;
	ctx->flat.dirty = 1;
	ctx->flat.record.magic = PFS_ICV_FLAT_MAGIC;
	ctx->flat.record.blockSize = blockSize;
	ctx->flat.record.count = 0;
	ctx->flat.record.reserved = 0;
	ctx->flat.key = *(const ScePfsHash *)key;
	if (ctx->flat.dirty) {
		ret = pfsLowerPwrite(ctx->flat.lf, &ctx->flat.record, sizeof(ctx->flat.record), 0,
				     &n);
		if (ret != 0)
			return ret;
		ctx->flat.dirty = 0;
	}
	return 0;
}

int pfsIcvFlatOpen(ScePfsCrypt *ctx, ScePfsLowerFile *lf, const void *key, SceUInt64 size,
		   SceUInt32 version)
{
	SceUInt64 fileSize;
	SceSize n;
	int ret;

	pfsIcvFlatSetup(ctx, lf);
	pfsLowerGetSize(lf, &fileSize);
	if (fileSize < sizeof(ScePfsIcvFlatRecord))
		return SCE_PFS_ERROR_ICV_BROKEN;
	ret = pfsLowerPread(ctx->flat.lf, &ctx->flat.record, sizeof(ctx->flat.record), 0, &n);
	if (ret != 0)
		return ret;
	if (n < sizeof(ctx->flat.record) || ctx->flat.record.magic != PFS_ICV_FLAT_MAGIC)
		return SCE_PFS_ERROR_ICV_BROKEN;
	if (version != ctx->flat.record.version)
		return SCE_PFS_ERROR_ICV_VERSION;
	ctx->flat.key = *(const ScePfsHash *)key;
	return 0;
}
