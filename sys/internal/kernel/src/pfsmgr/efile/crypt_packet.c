/*
 * The keys a file's blocks are read and written with, and the requests that
 * say which blocks a read, a write or a resize covers.
 */

#include <sysmem/sysclib.h>
#include <iofilemgr/iofilemgr.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_REQUEST_BUSY	0x80140609	/* name unknown */

/* the calling thread's I/O weight (name unknown) */
static inline SceInt32 pfsIoWeight(void)
{
	IoRequest *req = SceIofilemgrForDriver_12F8D58E();

	if (req == NULL)
		return 0x100;
	if (req->mntOpt < 0)
		return req->mntOpt;
	return (SceUInt16)req->mntOpt;
}

/* the calling thread's I/O priority (name unknown) */
static inline SceInt32 pfsIoPriority(void)
{
	IoRequest *req = SceIofilemgrForDriver_12F8D58E();

	if (req == NULL)
		return 0xf;
	return req->priority;
}

/* the seed a unicv record keeps for its file's keys (name unknown) */
static inline const ScePfsHash *pfsCryptSeed(ScePfsCrypt *ctx)
{
	if (ctx->kind == 0 || ctx->kind == 3)
		return &ctx->entry.record.seed;
	return NULL;
}

void pfsFileKeyInit(ScePfsFileKey *key, SceUInt32 index, SceOff size, SceUInt16 attr,
		    SceUInt32 type, ScePfsCrypt *ctx, ScePfsPmi *pmi)
{
	key->ioWeight = pfsIoWeight();
	key->ioPriority = pfsIoPriority();
	key->keyParam = pmi->a8;
	key->index = index;
	key->type = type;
	key->mode = pmi->mode;
	key->flags = pmi->flags;
	key->attr = attr;
	key->a6 = pmi->a6;
	key->size = size;
	key->klicensee = pmi->klicensee;
	key->blockSize = ctx->blockSize(ctx);

	if (pfsModeHasFileKeys(key->flags)) {
		if ((ctx->kind == 0 || ctx->kind == 3) && ctx->entry.record.version > 1)
			pfs_get_keys_hmac(&key->dataKey, &key->tweakKey, key->klicensee,
						pmi->flags, pmi->a6, pfsCryptSeed(ctx),
						sizeof(ScePfsHash));
		else
			pfs_get_keys(&key->dataKey, &key->tweakKey, key->klicensee, pmi->a8,
					  pmi->flags, index, pmi->a6);
	} else {
		pfs_get_keys_sha1(&key->dataKey, &key->tweakKey, key->klicensee, pmi->a8, index);
	}
	pfs_get_secret(&key->icvKey, key->klicensee, pmi->a8, pmi->flags, index, pmi->a6);
}

void pfsFileKeyClear(ScePfsFileKey *key)
{
	memset(key, 0, sizeof(*key));
}

void pfsBlockRequestInitRead(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff offset,
			     SceSize nbyte, void *dst)
{
	SceOff size = key->size;
	SceSize blockSize = key->blockSize;
	SceOff end = offset + nbyte;
	SceOff rem;
	SceUInt32 first, count, last;

	req->op = PFS_BLOCK_READ;
	req->src = NULL;
	req->headPartial = 0;
	req->fullBlocks = 0;
	req->allocated = 0;
	req->headLen = 0;
	req->tailLen = 0;
	req->lastLen = 0;
	req->padLen = 0;
	req->icvs = NULL;
	req->plain = NULL;
	req->disk = NULL;
	req->key = key;
	req->offset = offset;
	if (size < end) {
		nbyte = size - offset;
		end = offset + nbyte;
	}
	req->length = nbyte;
	req->dst = dst;

	first = offset / blockSize;
	count = (end - 1) / blockSize - first + 1;
	req->first = first;
	req->count = count;
	rem = offset % blockSize;
	req->offsetInBlock = rem;

	last = first + count - 1;
	if (key->type == 1) {
		if (size / blockSize < last)
			req->tailLen = 0;
		else if (size / blockSize == last)
			req->tailLen = size % blockSize;
		else
			req->tailLen = blockSize;
	} else {
		req->tailLen = size / blockSize < last ? 0 : blockSize;
	}

	if ((((blockSize - (SceSize)rem) % blockSize + (SceUIntPtr)dst) & 0xf) == 0) {
		if (rem != 0)
			req->headPartial = 1;
		if (end % blockSize == 0) {
			req->fullBlocks = count - req->headPartial;
			return;
		}
		if (count > 1)
			req->fullBlocks = count - 1 - req->headPartial;
	}
}

void pfsBlockRequestInitWrite(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff offset,
			      SceSize nbyte, const void *src)
{
	SceSize blockSize = key->blockSize;
	SceOff size = key->size;
	SceOff end = offset + nbyte;
	SceOff rem, endRem;
	SceUInt32 first, count;
	SceSize len, pad;

	req->key = key;
	req->op = PFS_BLOCK_WRITE;
	req->length = nbyte;
	req->offset = offset;
	req->headPartial = 0;
	req->fullBlocks = 0;
	req->headLen = 0;
	req->tailLen = 0;
	req->lastLen = 0;
	req->padLen = 0;
	req->icvs = NULL;
	req->plain = NULL;
	req->dst = NULL;
	req->allocated = 0;
	req->src = src;
	req->disk = NULL;

	first = offset / blockSize;
	count = (end - 1) / blockSize - first + 1;
	req->first = first;
	req->count = count;
	rem = offset % blockSize;
	req->offsetInBlock = rem;

	if (key->type == 1) {
		if (rem == 0)
			len = 0;
		else if (size / blockSize == first)
			len = size % blockSize;
		else
			len = blockSize;
		req->headLen = len;
		if (end < size) {
			if (size / blockSize == first + count - 1)
				len = size % blockSize;
			else
				len = blockSize;
		} else {
			len = 0;
		}
		req->tailLen = len;
		endRem = end % blockSize;
		if (endRem >= len)
			len = endRem;
		if (len == 0)
			len = blockSize;
		req->lastLen = len;
	} else {
		req->headLen = rem != 0 ? blockSize : 0;
		if (end < size)
			req->tailLen = blockSize;
		if (req->tailLen == 0 && end % blockSize > 0)
			pad = blockSize - end % blockSize;
		else
			pad = 0;
		req->lastLen = blockSize;
		req->padLen = pad;
	}

	if ((((blockSize - (SceSize)rem) % blockSize + (SceUIntPtr)src) & 0xf) == 0) {
		req->headPartial = rem != 0;
		if (end % blockSize == 0) {
			req->fullBlocks = req->count - req->headPartial;
			return;
		}
		if (req->count < 2)
			req->fullBlocks = 0;
		else
			req->fullBlocks = req->count - 1 - req->headPartial;
	}
}

void pfsBlockRequestInitResize(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff newSize,
			       SceOff *reached)
{
	SceOff size = key->size;
	SceSize blockSize = key->blockSize;
	SceOff limit, rem, endRem;
	SceUInt32 first, max;

	req->op = PFS_BLOCK_RESIZE;
	req->key = key;
	req->offset = 0;
	req->dst = NULL;
	req->src = NULL;
	req->headPartial = 0;
	req->fullBlocks = 0;
	req->length = 0;
	req->count = 0;
	req->allocated = 0;
	req->first = 0;
	req->offsetInBlock = 0;
	req->headLen = 0;
	req->tailLen = 0;
	req->lastLen = 0;
	req->padLen = 0;
	req->icvs = NULL;
	req->plain = NULL;
	req->disk = NULL;

	if (newSize <= size) {
		/* shrinking: only the new last block is rewritten */
		req->offset = newSize;
		first = newSize / blockSize;
		req->first = first;
		rem = newSize % blockSize;
		req->offsetInBlock = rem;
		req->count = rem != 0;
		if (key->type == 1) {
			if (req->count == 0)
				req->headLen = 0;
			else if (size / blockSize == first)
				req->headLen = size % blockSize;
			else
				req->headLen = blockSize;
			req->lastLen = rem;
		} else {
			if (req->count != 0) {
				req->headLen = blockSize;
				if (rem > 0) {
					req->padLen = blockSize - rem;
					req->lastLen = blockSize;
					goto done;
				}
			}
			req->lastLen = 0;
		}
	} else {
		/* growing: as many blocks as the buffers hold */
		max = pfsCryptBufferMaxBlocks(key->ioWeight, key->ioPriority);
		limit = size + blockSize * (max - 1) + 1;
		if (limit < newSize)
			newSize = limit - limit % blockSize;
		req->offset = newSize;
		req->length = newSize - size;
		first = size / blockSize;
		req->first = first;
		req->count = (newSize - 1) / blockSize - first + 1;
		rem = size % blockSize;
		req->offsetInBlock = rem;
		req->tailLen = 0;
		if (key->type == 1) {
			if (rem == 0)
				req->headLen = 0;
			else if (size / blockSize == first)
				req->headLen = rem;
			else
				req->headLen = blockSize;
			req->padLen = 0;
			endRem = newSize % blockSize;
			req->lastLen = endRem > 0 ? endRem : blockSize;
		} else {
			req->headLen = rem != 0 ? blockSize : 0;
			endRem = newSize % blockSize;
			req->lastLen = blockSize;
			req->padLen = endRem > 0 ? blockSize - endRem : 0;
		}
	}
done:
	*reached = newSize;
}

int pfsBlockRequestAlloc(ScePfsBlockRequest *req)
{
	int ret;

	if (req->op == -1 || req->plain != NULL || req->disk != NULL || req->icvs != NULL)
		return SCE_PFS_ERROR_REQUEST_BUSY;
	if (req->count != 0) {
		ret = scePfsBufAllocatorAllocateBuffer(req->key->ioWeight, req->key->ioPriority, req->count,
					  &req->plain, &req->disk, &req->icvs, 0);
		if (ret != 0) {
			req->plain = NULL;
			req->disk = NULL;
			req->icvs = NULL;
			req->allocated = 0;
			return ret;
		}
	}
	req->allocated = req->count;
	return 0;
}

int pfsBlockRequestFree(ScePfsBlockRequest *req)
{
	int ret;

	if (req->allocated == 0 || req->plain == NULL || req->disk == NULL)
		return 0;
	ret = pfsCryptBufferFree(req->key->ioWeight, req->key->ioPriority, req->allocated,
				 req->plain, req->disk, req->icvs);
	if (ret == 0) {
		req->key = NULL;
		req->plain = NULL;
		req->disk = NULL;
		req->icvs = NULL;
		req->allocated = 0;
		req->op = -1;
	}
	return ret;
}
