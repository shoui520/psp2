/*
 * The icv trees kept in "unicv.db": kind 0 keeps an icv per block of the
 * file, kind 3 only counts the blocks.
 */

#include "pfsmgr_internal.h"


static inline SceUInt64 pfsRoundUp64(SceUInt64 x, SceUInt64 align)	/* name unknown */
{
	return x + (align - x % align) % align;
}


static inline SceUInt32 pfsRoundUp32(SceUInt32 x, SceUInt32 align)	/* name unknown */
{
	return x + (align - x % align) % align;
}


static inline SceOff pfsUnicvIndexOffset(ScePfsUnicv *unicv, SceUInt32 index)	/* name unknown */
{
	SceUInt32 pageSize = unicv->header.pageSize;
	SceUInt32 offset = pageSize * index;

	offset -= pageSize;
	return offset;
}


static void pfsUnicvTreeGetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->entry.record.count;
}


static SceSize pfsUnicvTreeBlockSize(ScePfsCrypt *ctx)
{
	return ctx->entry.record.blockSize;
}


static SceOff pfsUnicvTreeIcvSize(ScePfsCrypt *ctx, SceOff size)
{
	ScePfsUnicvRecord *record = &ctx->entry.record;
	SceUInt32 blocks, pages;

	blocks = pfsRoundUp64(size, record->blockSize) / record->blockSize;
	pages = pfsRoundUp32(blocks, record->hashesPerPage) / record->hashesPerPage;
	return (SceUInt64)record->pageSize * pages + record->pageSize;
}


static int pfsUnicvTreeHashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				 const void *data, SceSize len)
{
	ScePfsHash key;

	icv_set_hmac_sw(&key, &ctx->key, &index, sizeof(index));
	return icv_set_hmac(icv, &key, data, len);
}


static int pfsUnicvTreeGetHash(ScePfsCrypt *ctx, void *hash)
{
	/* the record, with its seed from version 2 on */
	SceSize len = ctx->entry.record.version <= 1 ? 32 : 72;

	icv_set_hmac(hash, &ctx->key, &ctx->entry.record, len);
	return 0;
}


static int pfsUnicvTreeResize(ScePfsCrypt *ctx, SceUInt32 count)
{
	SceUInt32 n = ctx->entry.record.count;
	ScePfsHash zero;
	int ret;

	if (count < n) {
		do {
			ret = pfsUnicvEntryRemoveLast(&ctx->entry);
			if (ret != 0)
				break;
			n--;
		} while (count < n);
		return ret;
	}
	icv_clear(&zero);
	for (; n < count; n++) {
		ret = pfsUnicvEntryAppend(&ctx->entry, &zero);
		if (ret != 0)
			return ret;
	}
	return 0;
}


static int pfsUnicvTreeSetIcv(ScePfsCrypt *ctx, SceUInt32 index, const ScePfsHash *icv)
{
	int ret;

	if (index < ctx->entry.record.count)
		return pfsUnicvEntrySet(&ctx->entry, index, icv);
	if (index > ctx->entry.record.count) {
		ret = pfsUnicvTreeResize(ctx, index);
		if (ret != 0)
			return ret;
	}
	return pfsUnicvEntryAppend(&ctx->entry, icv);
}


static int pfsUnicvTreeGetIcv(ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv)
{
	return pfsUnicvEntryGet(&ctx->entry, index, icv);
}


static int pfsUnicvTreeFini(ScePfsCrypt *ctx)
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


static inline void pfsUnicvTreeSetup(ScePfsCrypt *ctx, ScePfsUnicv *unicv)	/* name unknown */
{
	ctx->getIcv = pfsUnicvTreeGetIcv;
	ctx->setIcv = pfsUnicvTreeSetIcv;
	ctx->blockSize = pfsUnicvTreeBlockSize;
	ctx->getHash = pfsUnicvTreeGetHash;
	ctx->hashBlock = pfsUnicvTreeHashBlock;
	ctx->fini = pfsUnicvTreeFini;
	ctx->getCount = pfsUnicvTreeGetCount;
	ctx->resize = pfsUnicvTreeResize;
	ctx->icvSize = pfsUnicvTreeIcvSize;
	ctx->lru = unicv->pages.lru;
	ctx->unicv = unicv;
	ctx->lf = unicv->pages.lf;
}


int pfsUnicvTreeCreate(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, SceSize blockSize,
		       const void *key)
{
	int ret;

	pfsUnicvTreeSetup(ctx, unicv);
	ctx->entry.key = *(const ScePfsHash *)key;
	ret = pfsUnicvOpenEntry(&ctx->entry, unicv, pfsUnicvIndexOffset(unicv, index));
	if (ret != 0)
		return ret;
	return pfsUnicvEntrySetBlockSize(&ctx->entry, blockSize);
}


int pfsUnicvTreeOpen(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, const void *key,
		     SceUInt64 size)
{
	SceUInt32 count, expected;
	SceSize blockSize;
	int ret;

	pfsUnicvTreeSetup(ctx, unicv);
	ctx->entry.key = *(const ScePfsHash *)key;
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


void pfsUnicvTreeTerm(ScePfsCrypt *ctx)
{
	pfsUnicvCloseEntry(&ctx->entry);
}
