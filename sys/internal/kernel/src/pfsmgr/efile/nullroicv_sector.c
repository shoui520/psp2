/*
 * The icv trees kept in "unicv.db": kind 0 keeps an icv per block of the
 * file, kind 3 only counts the blocks.
 */

#include "pfsmgr_internal.h"

static inline SceOff pfsUnicvIndexOffset(ScePfsUnicv *unicv, SceUInt32 index)	/* name unknown */
{
	SceUInt32 pageSize = unicv->header.pageSize;

	return pageSize * index - pageSize;
}

/* kind 3: a block count only */

static void pfsUnicvTree3GetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->entry.record.count;
}


static int pfsUnicvTree3Resize(ScePfsCrypt *ctx, SceUInt32 count)
{
	ctx->entry.record.count = count;
	ctx->entry.dirty = 1;
	return 0;
}


static SceSize pfsUnicvTree3BlockSize(ScePfsCrypt *ctx)
{
	return ctx->entry.record.blockSize;
}


static SceSize pfsUnicvTree3IcvSize(ScePfsCrypt *ctx)
{
	return ctx->entry.record.blockSize;
}


static int pfsUnicvTree3GetHash(ScePfsCrypt *ctx, void *hash)
{
	SceSize len = ctx->entry.record.version <= 1 ? 32 : 72;

	icv_set_hmac(hash, &ctx->key, &ctx->entry.record, len);
	return 0;
}


static int pfsUnicvTree3HashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				  const void *data, SceSize len)
{
	ScePfsHash key;

	icv_set_hmac_sw(&key, &ctx->key, &index, sizeof(index));
	icv_set_hmac(icv, &key, data, len);
	icv_clear(icv);
	return 0;
}


static int pfsUnicvTree3GetIcv(ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv)
{
	icv_clear(icv);
	return 0;
}


static int pfsUnicvTree3SetIcv(ScePfsCrypt *ctx, SceUInt32 index, const ScePfsHash *icv)
{
	SceUInt32 count = ctx->entry.record.count;

	if (count <= index) {
		ctx->entry.dirty = 1;
		ctx->entry.record.count = (index >= count ? index : count) + 1;
	}
	return 0;
}


static int pfsUnicvTree3Fini(ScePfsCrypt *ctx)
{
	int ret;

	ret = pfsUnicvEntryFlush(&ctx->entry);
	if (ret != 0)
		return ret;
	ret = pfsUnicvSync(ctx->unicv);
	if (ret != 0)
		return ret;
	return pfsLruFlush(ctx->lru, ctx->lf, 0);
}


static inline void pfsUnicvTree3Setup(ScePfsCrypt *ctx, ScePfsUnicv *unicv)	/* name unknown */
{
	ctx->resize = pfsUnicvTree3Resize;
	ctx->blockSize = pfsUnicvTree3BlockSize;
	ctx->getHash = pfsUnicvTree3GetHash;
	ctx->hashBlock = pfsUnicvTree3HashBlock;
	ctx->fini = pfsUnicvTree3Fini;
	ctx->getIcv = pfsUnicvTree3GetIcv;
	ctx->setIcv = pfsUnicvTree3SetIcv;
	ctx->getCount = pfsUnicvTree3GetCount;
	ctx->icvSize = (SceOff (*)(ScePfsCrypt *, SceOff))pfsUnicvTree3IcvSize;
	ctx->lru = unicv->pages.lru;
	ctx->unicv = unicv;
	ctx->lf = unicv->pages.lf;
}


int pfsUnicvTree3Create(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, SceSize blockSize)
{
	int ret;

	pfsUnicvTree3Setup(ctx, unicv);
	ret = pfsUnicvOpenEntry(&ctx->entry, unicv, pfsUnicvIndexOffset(unicv, index));
	if (ret != 0)
		return ret;
	return pfsUnicvEntrySetBlockSize(&ctx->entry, blockSize);
}


int pfsUnicvTree3Open(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, const void *key,
		      SceUInt64 size)
{
	SceUInt32 count, expected;
	SceSize blockSize;
	int ret;

	pfsUnicvTree3Setup(ctx, unicv);
	ret = pfsUnicvOpenEntry(&ctx->entry, unicv, pfsUnicvIndexOffset(unicv, index));
	if (ret != 0)
		return ret;
	blockSize = ctx->entry.record.blockSize;
	expected = size / blockSize;
	if (size % blockSize != 0)
		expected++;
	ctx->getCount(ctx, &count);
	if (count != expected)
		return SCE_PFS_ERROR_ICV_COUNT;
	return 0;
}


void pfsUnicvTree3Term(ScePfsCrypt *ctx)
{
	pfsUnicvCloseEntry(&ctx->entry);
}
