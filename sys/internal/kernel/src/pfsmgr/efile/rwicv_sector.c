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


static int pfsIcvTreeHashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
			       const void *data, SceSize len)
{
	return icv_set_hmac(icv, &ctx->tree.key, data, len);
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
	icv_clear(&zero);
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
