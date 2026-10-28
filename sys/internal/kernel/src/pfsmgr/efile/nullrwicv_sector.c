/*
 * efile/nullrwicv_sector.c (file name estimated: no debug record names this object) -- the
 * "null" icvs of a writable file: the icv file holds a 0x18-byte header (SCEINULL)
 * and every icv reads as zero. Functions in address order. */

#include "pfsmgr_internal.h"

#define PFS_HASH_TREE_HEADER_SIZE	0x40	/* name unknown */


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
	icv_set_hmac(hash, &ctx->flat.key, &ctx->flat.record, sizeof(ctx->flat.record));
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
	ret = file_access_truncate(lf, sizeof(ScePfsIcvFlatRecord));
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
	file_access_length(lf, &fileSize);
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
