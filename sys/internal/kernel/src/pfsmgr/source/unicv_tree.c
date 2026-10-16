/*
 * The icv trees kept in "unicv.db": kind 0 keeps an icv per block of the
 * file, kind 3 only counts the blocks.
 */

#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_ICV_COUNT		0x80141102	/* name unknown */

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

static int pfsUnicvTreeGetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->entry.record.count;
	return 0;
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

static void pfsUnicvTreeHashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				  const void *data, SceSize len)
{
	/* PLACEHOLDER: cryptographic routine, deferred */
	memset(icv, 0, sizeof(*icv));
}

static int pfsUnicvTreeGetHash(ScePfsCrypt *ctx, void *hash)
{
	/* PLACEHOLDER: cryptographic routine, deferred */
	memset(hash, 0, PFS_HASH_SIZE);
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
	pfsHashZero(&zero);
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

/* kind 3: a block count only */

static int pfsUnicvTree3GetCount(ScePfsCrypt *ctx, SceUInt32 *count)
{
	*count = ctx->entry.record.count;
	return 0;
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
	/* PLACEHOLDER: cryptographic routine, deferred */
	memset(hash, 0, PFS_HASH_SIZE);
	return 0;
}

static void pfsUnicvTree3HashBlock(ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
				   const void *data, SceSize len)
{
	/* PLACEHOLDER: cryptographic routine, deferred */
	pfsHashZero(icv);
}

static int pfsUnicvTree3GetIcv(ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv)
{
	pfsHashZero(icv);
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
