/*
 * SceLibMp4Recorder asynchronous background file writer thread.
 */
#include "mp4rec_internal.h"

extern "C" int32_t Mp4RecIoWrite(SceUID fd, const void *buf, SceSize size, int32_t unused, uint64_t *outWritten);

int32_t ThrWriteFileBufTerm(ThrWriteFileBuf *ctx)
{
	int32_t ret = 0;
	if (ctx->magic != MP4REC_THR_WRITE_MAGIC)
		return -4;

	if ((ctx->initFlags & 2U) != 0)
		sceKernelLockLwMutex(&ctx->apiMutex, 1, NULL);

	if (ctx->threadId != 0) {
		sceKernelSetEventFlag(ctx->threadEvf, 0x10000000U);
		sceKernelWaitThreadEnd(ctx->threadId, NULL, NULL);
		ret = sceKernelDeleteThread(ctx->threadId);
		ctx->threadId = 0;
	}
	if (ctx->ackEvf != 0) {
		sceKernelDeleteEventFlag(ctx->ackEvf);
		ctx->ackEvf = 0;
	}
	if (ctx->threadEvf != 0) {
		sceKernelDeleteEventFlag(ctx->threadEvf);
		ctx->threadEvf = 0;
	}
	if ((ctx->initFlags & 4U) != 0) {
		BufferPoolTerm(&ctx->pool);
		ctx->initFlags &= ~4U;
	}
	if ((ctx->initFlags & 1U) != 0) {
		sceKernelDeleteLwMutex(&ctx->dataMutex);
		ctx->initFlags &= ~1U;
	}
	if ((ctx->initFlags & 2U) != 0) {
		sceKernelDeleteLwMutex(&ctx->apiMutex);
		ctx->initFlags &= ~1U;
	}
	ctx->magic = 0;
	return ret;
}

int32_t ThrWriteFileBufInit(ThrWriteFileBuf *ctx, MemAllocator *allocator, SceUID fd,
	int32_t priority, int32_t affinity)
{
	if (ctx->magic == MP4REC_THR_WRITE_MAGIC)
		return -5;

	memset(ctx, 0, sizeof(*ctx));
	ctx->fd = fd;
	ctx->allocator = allocator;
	ctx->magic = MP4REC_THR_WRITE_MAGIC;
	ctx->maxBlocks = MP4REC_WRITE_BLOCK_COUNT;
	ctx->lastError = 0;
	ctx->threadPriority = priority;
	ctx->threadAffinity = affinity;

	ThrWriteFileBuf *threadArg = ctx;
	int32_t ret = sceKernelCreateLwMutex(&ctx->apiMutex, "SceMp4RecThrWriteFileBufApi", 2U, 0, NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->initFlags |= 2U;

	ret = sceKernelCreateLwMutex(&ctx->dataMutex, "SceMp4RecThrWriteFileBuf", 2U, 0, NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->initFlags |= 1U;

	ret = BufferPoolInit(&ctx->pool, allocator, MP4REC_WRITE_BLOCK_SIZE, MP4REC_WRITE_BLOCK_COUNT,
		"SceMp4RecThrWriteFileBufBuf", NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->initFlags |= 4U;

	ret = sceKernelCreateEventFlag("SceMp4RecThrWriteFileBufThread", 0x1000, 0, NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->threadEvf = ret;

	ret = sceKernelCreateEventFlag("SceMp4RecThrWriteFileBufAck", 0x1000, 0, NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->ackEvf = ret;

	ret = sceKernelCreateThread("SceMp4RecWriteFileBuf", ThrWriteFileBufThreadMain,
		priority, 0x1000U, 0, affinity, NULL);
	if (ret < 0) {
		ThrWriteFileBufTerm(ctx);
		return ret;
	}
	ctx->threadId = ret;

	ret = sceKernelStartThread(ctx->threadId, sizeof(threadArg), &threadArg);
	if (ret < 0)
		ThrWriteFileBufTerm(ctx);
	return ret;
}

int32_t ThrWriteFileBufAcquire(ThrWriteFileBuf *ctx, void **outBuf, uint32_t *outSize)
{
	if (ctx->magic != MP4REC_THR_WRITE_MAGIC)
		return -4;

	int32_t ret = sceKernelLockLwMutex(&ctx->apiMutex, 1, NULL);
	if (ret < 0)
		return ret;

	int32_t status = 0;
	while ((uint32_t)BufferPoolGetQueuedCount(&ctx->pool) >= ctx->maxBlocks) {
		uint32_t outBits = 0;
		status = sceKernelWaitEventFlag(ctx->ackEvf, 0xFFFFFFFFU, 3U, &outBits, NULL);
		if (status < 0)
			break;
		if ((outBits & 0x08000000U) != 0) {
			sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
			status = ctx->lastError;
			sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
			break;
		}
	}

	if (status >= 0) {
		BufferPoolBlock *blk = NULL;
		BufferPoolGetWriteBlock(&ctx->pool, &blk);
		if (blk != NULL) {
			*outBuf = blk->data;
			*outSize = blk->capacity;
		}
		status = 0;
	}

	sceKernelUnlockLwMutex(&ctx->apiMutex, 1);
	return status;
}

int32_t ThrWriteFileBufSubmit(ThrWriteFileBuf *ctx, void *buf, uint32_t size, uint32_t *outWritten)
{
	if (ctx->magic != MP4REC_THR_WRITE_MAGIC)
		return -4;

	int32_t ret = sceKernelLockLwMutex(&ctx->apiMutex, 1, NULL);
	if (ret < 0)
		return ret;

	BufferPoolBlock *blk = NULL;
	BufferPoolGetWriteBlock(&ctx->pool, &blk);
	if (blk != NULL && blk->data == buf && blk->capacity >= size) {
		blk->usedSize = size;
		*outWritten = size;
		BufferPoolCommitWriteBlock(&ctx->pool);
		sceKernelSetEventFlag(ctx->threadEvf, 1U);
	} else {
		ret = -3;
	}

	sceKernelUnlockLwMutex(&ctx->apiMutex, 1);
	return ret;
}

int32_t ThrWriteFileBufFlush(ThrWriteFileBuf *ctx)
{
	if (ctx->magic != MP4REC_THR_WRITE_MAGIC)
		return -4;

	int32_t ret = sceKernelLockLwMutex(&ctx->apiMutex, 1, NULL);
	if (ret < 0)
		return ret;

	sceKernelSetEventFlag(ctx->threadEvf, 0x20000000U);
	uint32_t outBits = 0;
	while (BufferPoolGetQueuedCount(&ctx->pool) > 0) {
		ret = sceKernelWaitEventFlag(ctx->ackEvf, 0xFFFFFFFFU, 3U, &outBits, NULL);
		if (ret < 0)
			break;
		if ((outBits & 0x08000000U) != 0) {
			sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
			ret = ctx->lastError;
			sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
			break;
		}
	}

	sceKernelUnlockLwMutex(&ctx->apiMutex, 1);
	return ret;
}

int32_t ThrWriteFileBufSetLowSpaceCallback(ThrWriteFileBuf *ctx, void *unused,
	uint64_t threshold, LowSpaceCallbackFn cb)
{
	(void)unused;
	if (ctx->magic != MP4REC_THR_WRITE_MAGIC)
		return -4;

	int32_t ret = sceKernelLockLwMutex(&ctx->apiMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (cb != NULL && threshold != 0) {
		sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
		ctx->lowSpaceThreshold = threshold;
		ctx->lowSpaceCb = cb;
		sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
	}

	sceKernelUnlockLwMutex(&ctx->apiMutex, 1);
	return ret;
}

int32_t ThrWriteFileBufThreadMain(SceSize args, void *argp)
{
	(void)args;
	ThrWriteFileBuf *ctx = *(ThrWriteFileBuf **)argp;
	uint32_t stateFlags = 1U;

	for (;;) {
		uint32_t outBits = 0;
		int32_t waitRes = sceKernelWaitEventFlag(ctx->threadEvf, 0xFFFFFFFFU, 3U, &outBits, NULL);
		if ((outBits & 0x80000000U) != 0)
			break;

		if (waitRes < 0) {
			sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
			ctx->lastError = waitRes;
			sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
			sceKernelSetEventFlag(ctx->ackEvf, 0x08000000U);
			continue;
		}

		if ((outBits & 0x10000000U) != 0)
			stateFlags |= 4U;
		if ((outBits & 0x20000000U) != 0)
			stateFlags = (stateFlags & ~3U) | 2U;

		if (BufferPoolGetQueuedCount(&ctx->pool) > 0) {
			BufferPoolBlock *blk = NULL;
			BufferPoolGetReadBlock(&ctx->pool, &blk);
			uint64_t written = 0;
			int32_t ioRes = Mp4RecIoWrite(ctx->fd, blk->data, blk->usedSize, 0, &written);
			if (ioRes < 0) {
				sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
				ctx->lastError = ioRes;
				sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
				sceKernelSetEventFlag(ctx->ackEvf, 0x08000000U);
				if ((stateFlags & 4U) != 0)
					break;
				continue;
			}

			uint32_t ackBits = 0;
			if (written != (uint64_t)blk->usedSize) {
				sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
				ctx->lastError = -2;
				sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
				ackBits = 0x08000000U;
			}
			BufferPoolReleaseReadBlock(&ctx->pool);
			if ((stateFlags & 1U) != 0)
				ackBits |= 0x10000U;
			sceKernelSetEventFlag(ctx->ackEvf, ackBits);
		}

		if (BufferPoolGetQueuedCount(&ctx->pool) <= 0) {
			if ((stateFlags & 2U) != 0) {
				sceKernelSetEventFlag(ctx->ackEvf, 0x10000U);
				stateFlags = (stateFlags & ~3U) | 1U;
			}
			if ((stateFlags & 4U) != 0)
				break;
		} else {
			sceKernelSetEventFlag(ctx->threadEvf, 1U);
		}

		uint64_t maxSize = 0;
		uint64_t freeSize = 0;
		if (sceAppMgrGetDevInfo("ux0:", &maxSize, &freeSize) >= 0) {
			sceKernelLockLwMutex(&ctx->dataMutex, 1, NULL);
			if (freeSize <= ctx->lowSpaceThreshold && ctx->lowSpaceCb != NULL)
				ctx->lowSpaceCb(freeSize);
			sceKernelUnlockLwMutex(&ctx->dataMutex, 1);
		}
	}

	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
