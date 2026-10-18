/*
 * A file's blocks: a read, a write or a resize covers a run of blocks, read
 * from the lower file into crypt buffers, put through the crypt engine and
 * copied out or written back, with one icv per block.
 */

#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_SHORT_READ	0x80140f02	/* name unknown */
#define SCE_PFS_ERROR_BLOCK_ARG		0x80140f09	/* name unknown */
#define SCE_PFS_ERROR_NO_BUFFER		0x80142318	/* name unknown */

#define PFS_CACHE_LINE	0x40

/* the blocks carry icvs (name unknown) */
static inline int pfsFileKeyHasIcvs(const ScePfsFileKey *key)
{
	return !(key->attr & 0x2000) && !(key->attr & 0x8000) &&
	       !(key->flags & PFS_PMI_MODE_3);
}

static inline SceSize pfsCacheAlign(SceSize len)	/* name unknown */
{
	return (len + PFS_CACHE_LINE - 1) & ~(PFS_CACHE_LINE - 1);
}

/* write the request's blocks and their icvs */
int pfsBlockWrite(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx)
{
	ScePfsFileKey *key = req->key;
	SceOff offset;
	SceSize len, n;
	SceUInt32 i;
	int ret;

	if (req->count == 0)
		len = req->lastLen;
	else
		len = key->blockSize * (req->count - 1) + req->lastLen;
	offset = (SceUInt64)key->blockSize * req->first;
	if (!pfsModeHasFileKeys(key->mode))
		SceCpuForDriver_103872A5(req->plain, pfsCacheAlign(len));
	ret = pfsLowerPwrite(lf, req->disk, len, offset, &n);
	if (ret == 0 && pfsFileKeyHasIcvs(key)) {
		for (i = 0; i < req->count; i++) {
			ret = ctx->setIcv(ctx, req->first + i, &req->icvs[i]);
			if (ret != 0)
				break;
		}
	}
	return ret;
}

/* read the partly covered first and last blocks before a write or a resize */
int pfsBlockReadEdges(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx)
{
	ScePfsFileKey *key = req->key;
	SceSize len, n;
	int ret;

	len = req->headLen;
	if (len != 0) {
		SceCpuForDriver_364E68A4(req->plain, pfsCacheAlign(len));
		ret = pfsLowerPread(lf, req->disk, len, req->first * key->blockSize, &n);
		if (ret != 0)
			return ret;
		if (n < len)
			return SCE_PFS_ERROR_SHORT_READ;
		if (pfsFileKeyHasIcvs(key)) {
			ret = ctx->getIcv(ctx, req->first, req->icvs);
			if (ret != 0)
				return ret;
		}
	}

	len = req->tailLen;
	if (len != 0 && (req->count != 1 || req->headLen == 0)) {
		SceCpuForDriver_364E68A4((char *)req->plain +
						     key->blockSize * (req->count - 1),
						     pfsCacheAlign(len));
		ret = pfsLowerPread(lf, (char *)req->disk + key->blockSize * (req->count - 1), len,
				    key->blockSize * (req->count + req->first - 1), &n);
		if (ret != 0)
			return ret;
		if (n < len)
			return SCE_PFS_ERROR_SHORT_READ;
		if (pfsFileKeyHasIcvs(key)) {
			ret = ctx->getIcv(ctx, req->first + req->count - 1,
					  &req->icvs[req->count - 1]);
			if (ret != 0)
				return ret;
		}
	}

	if (req->op == PFS_BLOCK_RESIZE && req->offset < key->size) {
		ret = pfsLowerSetSize(lf, req->first * key->blockSize);
		if (ret != 0)
			return ret;
		if (pfsFileKeyHasIcvs(key))
			return ctx->resize(ctx, req->first);
	}
	return 0;
}

int pfsFileWriteChunk(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
			     ScePfsPmi *pmi, ScePfsCrypt *ctx, const void *buf, SceSize nbyte,
			     SceOff offset, SceSize *written)
{
	ScePfsCryptJob job;
	ScePfsBlockRequest req;
	ScePfsFileKey key;
	const ScePfsModeInfo *info;
	int ret;

	info = pfsModeInfo(pmi->mode);
	pfsFileKeyInit(&key, index, size, attr, info->type, ctx, pmi);
	pfsBlockRequestInitWrite(&req, &key, offset, nbyte, buf);
	ret = pfsBlockRequestAlloc(&req);
	if (ret == SCE_ERROR_ERRNO_ENOENT)
		return SCE_PFS_ERROR_NO_BUFFER;
	if (ret != 0)
		return ret;
	ret = pfsBlockReadEdges(&req, lf, ctx);
	if (ret == 0) {
		job.thread = sceKernelGetThreadId();
		job.req = &req;
		job.result = 0;
		job.waiting = 0;
		ret = pfsCryptEngineSubmit(&job);
		if (ret == 0)
			ret = pfsCryptEngineWait(&job);
		if (ret == 0)
			ret = job.result;
		if (ret == 0)
			ret = pfsBlockWrite(&req, lf, ctx);
		if (ret == 0)
			*written = nbyte;
	}
	pfsBlockRequestFree(&req);
	pfsFileKeyClear(&key);
	return ret;
}

/* read the request's blocks and their icvs */
int pfsBlockRead(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx)
{
	ScePfsFileKey *key = req->key;
	SceSize len, n = 0;
	SceUInt32 i;
	int ret;

	len = key->blockSize * (req->count - 1) + req->tailLen;
	SceCpuForDriver_364E68A4(req->plain, pfsCacheAlign(len));
	ret = pfsLowerPread(lf, req->disk, len, req->offset - req->offsetInBlock, &n);
	if (ret != 0)
		return ret;
	if (n < len)
		return SCE_PFS_ERROR_SHORT_READ;
	if (pfsFileKeyHasIcvs(key)) {
		for (i = 0; i < req->count; i++) {
			ret = ctx->getIcv(ctx, req->first + i, &req->icvs[i]);
			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

/* copy what was read to the caller; full blocks were decrypted in place */
int pfsBlockCopyOut(ScePfsBlockRequest *req, void *buf, SceSize nbyte, SceOff offset,
			   SceSize *read)
{
	ScePfsFileKey *key = req->key;
	SceSize blockSize = key->blockSize;
	SceSize start = blockSize * req->first;
	SceSize pos, head, tail, from, to, last, end;
	SceUInt32 count;
	const char *src;
	const char *mid;
	SceOff limit;

	if (offset < start)
		return SCE_PFS_ERROR_BLOCK_ARG;
	if (pfsModeHasFileKeys(key->mode))
		src = req->disk;
	else
		src = req->plain;
	count = req->count;
	limit = blockSize * (req->first + count);
	if (key->size < limit)
		limit = key->size;
	if (limit < offset) {
		*read = 0;
		return 0;
	}
	if (limit < offset + nbyte)
		nbyte = limit - offset;

	pos = offset - start;
	if (req->fullBlocks == 0) {
		memcpy(buf, src + pos, nbyte);
		goto done;
	}

	head = 0;
	if (pos < blockSize && req->headPartial) {
		end = pos + nbyte;
		head = (end > blockSize ? blockSize : end) - pos;
		memcpy(buf, src + pos, head);
		count = req->count;
	}

	tail = 0;
	if (count >= 2) {
		end = pos + nbyte;
		last = key->blockSize * (count - 1);
		if (end > last && count > req->headPartial + req->fullBlocks) {
			to = key->blockSize + last;
			from = last > pos ? last : pos;
			tail = (end < to ? end : to) - from;
			memcpy((char *)buf + (from - pos), src + from, tail);
		}
	}

	if (head + tail < nbyte) {
		mid = (const char *)req->dst + (pos - req->offsetInBlock + head);
		if (mid != (char *)buf + head)
			memcpy((char *)buf + head, mid, nbyte - tail - head);
	}
done:
	*read = nbyte;
	return 0;
}

int pfsFileRead(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		ScePfsPmi *pmi, ScePfsCrypt *ctx, void *buf, SceSize nbyte, SceOff offset,
		SceSize *read)
{
	ScePfsCryptJob job;
	ScePfsBlockRequest req;
	ScePfsFileKey key;
	const ScePfsModeInfo *info;
	int ret;

	info = pfsModeInfo(pmi->mode);
	pfsFileKeyInit(&key, index, size, attr, info->type, ctx, pmi);
	pfsBlockRequestInitRead(&req, &key, offset, nbyte, buf);
	ret = pfsBlockRequestAlloc(&req);
	if (ret == SCE_ERROR_ERRNO_ENOENT)
		return SCE_PFS_ERROR_NO_BUFFER;
	if (ret != 0)
		return ret;
	ret = pfsBlockRead(&req, lf, ctx);
	if (ret == 0) {
		job.thread = sceKernelGetThreadId();
		job.req = &req;
		job.result = 0;
		job.waiting = 0;
		ret = pfsCryptEngineSubmit(&job);
		if (ret == 0)
			ret = pfsCryptEngineWait(&job);
		if (ret == 0)
			ret = job.result;
		if (ret == 0)
			ret = pfsBlockCopyOut(&req, buf, nbyte, offset, read);
	}
	pfsBlockRequestFree(&req);
	pfsFileKeyClear(&key);
	return ret;
}

/* writes are cut at a 0x8000 boundary when they are large */
int pfsFileWrite(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		 ScePfsPmi *pmi, ScePfsCrypt *ctx, const void *buf, SceSize nbyte, SceOff offset,
		 SceSize *written)
{
	SceSize head = 0, rest = 0;
	SceOff end = offset + nbyte;
	int ret;

	if (end + (0x8000 - end % 0x8000) % 0x8000 - offset > 0x40000) {
		ret = pfsFileWriteChunk(lf, index, size, attr, pmi, ctx, buf,
					(0x8000 - offset % 0x8000) % 0x8000, offset, &head);
		if (ret < 0)
			return ret;
		offset += head;
		nbyte -= head;
		buf = (const char *)buf + head;
		if (size < offset + head)
			size = offset + head;
	}
	ret = pfsFileWriteChunk(lf, index, size, attr, pmi, ctx, buf, nbyte, offset, &rest);
	if (ret >= 0)
		*written = head + rest;
	return ret;
}

/* a file grows by as many blocks as the crypt buffers hold at a time */
int pfsFileResize(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		  ScePfsPmi *pmi, ScePfsCrypt *ctx, SceOff newSize)
{
	ScePfsCryptJob job;
	ScePfsBlockRequest req;
	ScePfsFileKey key;
	const ScePfsModeInfo *info;
	SceOff reached;
	int ret;

	info = pfsModeInfo(pmi->mode);
	do {
		pfsFileKeyInit(&key, index, size, attr, info->type, ctx, pmi);
		pfsBlockRequestInitResize(&req, &key, newSize, &reached);
		ret = pfsBlockRequestAlloc(&req);
		if (ret == SCE_ERROR_ERRNO_ENOENT)
			return SCE_PFS_ERROR_NO_BUFFER;
		if (ret != 0)
			return ret;
		ret = pfsBlockReadEdges(&req, lf, ctx);
		if (ret == 0) {
			job.thread = sceKernelGetThreadId();
			job.req = &req;
			job.result = 0;
			job.waiting = 0;
			ret = pfsCryptEngineSubmit(&job);
			if (ret == 0)
				ret = pfsCryptEngineWait(&job);
			if (ret == 0)
				ret = job.result;
			if (ret == 0)
				ret = pfsBlockWrite(&req, lf, ctx);
		}
		if (ret != 0) {
			pfsBlockRequestFree(&req);
			pfsFileKeyClear(&key);
			return ret;
		}
		size = reached;
		pfsBlockRequestFree(&req);
		pfsFileKeyClear(&key);
	} while (size < newSize);
	return ret;
}
