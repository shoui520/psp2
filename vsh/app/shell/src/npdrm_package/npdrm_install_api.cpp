#include "npdrm_package_internal.h"

extern "C" {
NpDrmInstallPerfStats g_npDrmInstallPerf;
SceUInt8 g_npDrmHwAccelActive = 1U;
}

namespace {

static NpDrmInstallContext *g_installSlots[NPDRM_PKG_MAX_SLOTS];
static NpDrmInstallContext g_defaultSlotContext;

typedef struct NpDrmWorkerThreadArg {
	NpDrmJobWorker *worker;
	SceUInt32 workerIndex;
	SceUID forkEventFlag;
	SceUID joinEventFlag;
} NpDrmWorkerThreadArg;

static int npDrmNoopForkCb(NpDrmPipelineState *pipeline, SceUInt32 slotMask, void *userData)
{
	(void)pipeline;
	(void)slotMask;
	(void)userData;
	return 0;
}

static int npDrmSyncJoinCb(NpDrmPipelineState *pipeline, SceUInt32 slotMask, void *userData)
{
	(void)userData;
	for (SceUInt32 idx = 0; idx < 32U; ++idx) {
		if ((slotMask & (1U << idx)) != 0) {
			int err = npDrmExecuteChunkStage(pipeline, idx);
			if (err != 0) {
				return err;
			}
		}
	}
	return 0;
}

static SceUInt64 readPerfStamp(void)
{
	if ((g_npDrmInstallPerf.enabled & 0xFFULL) == 0) {
		return 0ULL;
	}
	return (SceUInt64)sceKernelGetSystemTimeWide();
}

static void recordPerfStep(SceUInt64 *accumulator)
{
	SceUInt64 now = readPerfStamp();

	*accumulator += now - g_npDrmInstallPerf.lastStampUs;
	g_npDrmInstallPerf.lastStampUs = now;
}

static SceUInt64 getThreadRunClocks(SceUID threadId)
{
	if ((g_npDrmInstallPerf.enabled & 0xFFULL) == 0) {
		return 0ULL;
	}
	SceKernelThreadInfo info;

	info.size = sizeof(info);
	if (sceKernelGetThreadInfo(threadId, &info) < 0) {
		return 0ULL;
	}
	return info.runClocks.quad;
}

static SceUInt32 swapBe32(SceUInt32 value)
{
	return ((value & 0x000000FFU) << 24)
		| ((value & 0x0000FF00U) << 8)
		| ((value & 0x00FF0000U) >> 8)
		| ((value & 0xFF000000U) >> 24);
}

static int lookupSlotContext(int handle, NpDrmInstallContext **outCtx)
{
	int slotIndex = handle - NPDRM_PKG_HANDLE_BASE;

	if (slotIndex < 0 || slotIndex >= NPDRM_PKG_MAX_SLOTS) {
		return SCE_NPDRM_PKG_ERROR_INVALID_SLOT;
	}
	NpDrmInstallContext *ctx = g_installSlots[slotIndex];

	if (ctx == NULL || ctx->slotIndex != slotIndex) {
		return SCE_NPDRM_PKG_ERROR_INVALID_HANDLE;
	}
	*outCtx = ctx;
	return slotIndex;
}

static void resetInstallContextState(NpDrmInstallContext *ctx, SceUInt32 flags, SceMode mode)
{
	sceClibMemset(&ctx->statFile, 0, sizeof(ctx->statFile));
	ctx->statFile.magic = NPDRM_PKG_STAT_MAGIC;
	ctx->statFile.version = 1U;
	ctx->statFile.statSize = NPDRM_PKG_STAT_SIZE_BASE;
	ctx->statFile.hashState.currentItemIndex = 0U;
	sceSha1BlockInit(&ctx->statFile.hashState.pkgSha1Ctx);
	ctx->statFile.hashState.pkgHashedBytes = 0ULL;
	ctx->statFile.hashState.pkgTotalBytes = 0ULL;
	sceSha256BlockInit(&ctx->statFile.hashState.bodySha256Ctx);
	ctx->statFile.hashState.bodyHashedBytes = 0ULL;
	ctx->statFile.hashState.bodyTotalBytes = 0ULL;
	sceSha256BlockInit(&ctx->statFile.hashState.fileSha256Ctx);
	ctx->statFile.hashState.fileHashedBytes = 0ULL;
	ctx->statFile.hashState.fileTotalBytes = 0ULL;

	sceClibMemset(&ctx->pkgMeta, 0, sizeof(ctx->pkgMeta));
	sceClibMemset(&ctx->currentItem, 0, sizeof(ctx->currentItem));
	ctx->prefixFilterLen = 0U;
	ctx->pkgMeta.openFlags = flags;
	ctx->pkgMeta.openMode = mode;
	ctx->currentItem.itemIndex = -1;
	ctx->activeFd = -1;
	ctx->bodyFd = -1;
	ctx->headFd = -1;

	ctx->pipeline.activeFdPtr = &ctx->activeFd;
	ctx->pipeline.pathBufPtr = ctx->workPath;
	ctx->pipeline.rootDirPtr = ctx->rootDir;
	ctx->pipeline.statFilePtr = &ctx->statFile;
	ctx->pipeline.chunks = NULL;
	ctx->pipeline.chunkCount = 0U;
	ctx->pipeline.currentChunkIndex = 0U;
	ctx->pipeline.forkMask = 0U;
	ctx->pipeline.joinMask = 0U;
	ctx->pipeline.forkCb = npDrmNoopForkCb;
	ctx->pipeline.joinCb = npDrmSyncJoinCb;
	ctx->pipeline.cbUserData = NULL;
}

static void resetPerfCounters(SceUInt8 keepEnabled)
{
	sceClibMemset(&g_npDrmInstallPerf, 0, sizeof(g_npDrmInstallPerf));
	g_npDrmInstallPerf.enabled = (SceUInt64)keepEnabled;
}

static void spinWaitOneSecond(void)
{
	SceUInt64 targetTime;

	do {
		targetTime = (SceUInt64)sceKernelGetSystemTimeWide() + 1000000ULL;
	} while (targetTime < 1000000ULL);
	while ((SceUInt64)sceKernelGetSystemTimeWide() < targetTime) {
	}
}

static int installWorkerThreadEntry(SceSize argsLen, void *argsPtr)
{
	if (argsLen != sizeof(NpDrmWorkerThreadArg)) {
		return 0;
	}
	NpDrmWorkerThreadArg *arg = (NpDrmWorkerThreadArg *)argsPtr;
	NpDrmJobWorker *worker = arg->worker;
	SceUInt32 waitPattern = 1U << arg->workerIndex;

	for (;;) {
		int err = sceKernelWaitEventFlag(arg->forkEventFlag, waitPattern,
			SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
		if (err != 0) {
			return err;
		}
		worker->lastResult = npDrmExecuteChunkStage(worker->pipeline, worker->workerIndex);
		err = sceKernelSetEventFlag(arg->joinEventFlag, 1U << worker->workerIndex);
		if (err != 0) {
			return err;
		}
	}
}

static int destroyThreadPool(NpDrmThreadPool *pool)
{
	if (pool == NULL) {
		return 0;
	}

	int firstErr = 0;
	if (pool->totalSize != pool->workerCount * sizeof(NpDrmJobWorker) + 64U) {
		firstErr = SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}

	int err = sceKernelDeleteEventFlag(pool->joinEventFlag);
	if (err < 0 && firstErr == 0) {
		firstErr = err;
	}
	err = sceKernelDeleteEventFlag(pool->forkEventFlag);
	if (err < 0 && firstErr == 0) {
		firstErr = err;
	}

	for (SceUInt32 idx = 0; idx < pool->workerCount; ++idx) {
		SceUID threadId = pool->workers[idx].threadId;

		err = sceKernelWaitThreadEnd(threadId, NULL, NULL);
		if (err >= 0) {
			int delErr = sceKernelDeleteThread(threadId);
			if (delErr < 0 && firstErr == 0) {
				firstErr = delErr;
			}
		} else if (firstErr == 0) {
			firstErr = err;
		}
	}

	g_npDrmInstallPerf.sha1Us = pool->stageClocks[0];
	g_npDrmInstallPerf.decryptUs = pool->stageClocks[1];
	g_npDrmInstallPerf.sha256Us = pool->stageClocks[2];
	g_npDrmInstallPerf.fileWriteUs = pool->stageClocks[3];

	if (pool->workMemBlockId > 0) {
		err = sceKernelFreeMemBlock(pool->workMemBlockId);
		if (err < 0 && firstErr == 0) {
			firstErr = err;
		}
	}
	if (pool->heapMemBlockId > 0) {
		err = sceKernelFreeMemBlock(pool->heapMemBlockId);
		if (err < 0 && firstErr == 0) {
			firstErr = err;
		}
	}
	return firstErr;
}

static int createThreadPool(NpDrmThreadPool *pool, SceSize poolBytes, SceUInt32 numJobs, SceUInt32 flags)
{
	SceSize requiredBytes = numJobs * sizeof(NpDrmJobWorker) + 64U;

	if (poolBytes < requiredBytes) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}
	pool->totalSize = requiredBytes;
	pool->workerCount = numJobs;
	pool->modeFlags = flags;
	pool->forkEventFlag = -1;
	pool->joinEventFlag = -1;
	for (SceUInt32 idx = 0; idx < numJobs; ++idx) {
		pool->workers[idx].threadId = -1;
	}

	SceUID evf = sceKernelCreateEventFlag("SceNpDrmInstallFork", 0x1000U, 0U, NULL);
	pool->forkEventFlag = evf;
	if (evf < 0) {
		return evf;
	}
	evf = sceKernelCreateEventFlag("SceNpDrmInstallJoin", 0U, 0U, NULL);
	pool->joinEventFlag = evf;
	if (evf < 0) {
		return evf;
	}

	SceSize stackSize = 4096U;
	if ((flags & 0x20U) != 0) {
		stackSize = 2048U;
	}
	if ((flags & 0x10U) != 0) {
		stackSize = 1024U;
	}

	int basePriority = sceKernelGetThreadCurrentPriority();
	int affinityMask = ((flags & 1U) != 0) ? 0x000F0000 : 0;

	for (SceUInt32 idx = 0; idx < numJobs; ++idx) {
		SceUID tid = sceKernelCreateThread("SceNpDrmInstallJob",
			installWorkerThreadEntry, basePriority + 2, stackSize, 0U, affinityMask, NULL);
		if (tid < 0) {
			return tid;
		}
		pool->workers[idx].threadId = tid;
	}
	return 0;
}

static void cleanupFailedThreadPool(NpDrmThreadPool *pool, SceSize poolBytes, SceUInt32 numJobs)
{
	if (poolBytes < numJobs * sizeof(NpDrmJobWorker) + 64U) {
		return;
	}
	for (SceUInt32 idx = 0; idx < pool->workerCount; ++idx) {
		SceUID tid = pool->workers[idx].threadId;

		if (tid >= 0 && sceKernelDeleteThread(tid) == 0) {
			pool->workers[idx].threadId = -1;
		}
	}
	if (pool->forkEventFlag >= 0 && sceKernelDeleteEventFlag(pool->forkEventFlag) == 0) {
		pool->forkEventFlag = -1;
	}
	if (pool->joinEventFlag >= 0 && sceKernelDeleteEventFlag(pool->joinEventFlag) == 0) {
		pool->joinEventFlag = -1;
	}
}

static int threadedForkCb(NpDrmPipelineState *pipeline, SceUInt32 slotMask, void *userData)
{
	NpDrmThreadPool *pool = (NpDrmThreadPool *)userData;
	int curPriority = 0;

	if ((pool->modeFlags & 1U) != 0) {
		curPriority = sceKernelGetThreadCurrentPriority();
		SceUInt8 hwAccel = (sceNpDrmPackageIsGameExist() == 0) ? 1U : 0U;

		if ((pool->modeFlags & 4U) != 0) {
			hwAccel = 0U;
		}
		g_npDrmHwAccelActive = hwAccel;
	}

	SceUInt32 dispatchBits = 0;
	for (SceUInt32 idx = 0; idx < pool->workerCount; ++idx) {
		if ((slotMask & (1U << idx)) != 0) {
			NpDrmJobWorker *worker = &pool->workers[idx];

			worker->startClocks = getThreadRunClocks(worker->threadId);
			worker->workerIndex = idx;
			worker->pipeline = pipeline;
			if ((pool->modeFlags & 1U) != 0) {
				int targetPrio = (worker->stagePhase == 3)
					? (curPriority - 1) : (curPriority + 1);

				if (g_npDrmHwAccelActive != 0) {
					targetPrio -= 2;
				}
				sceKernelChangeThreadPriority(worker->threadId, targetPrio);
			}
			dispatchBits |= (1U << idx);
		}
	}
	if (dispatchBits != 0) {
		return sceKernelSetEventFlag(pool->forkEventFlag, dispatchBits);
	}
	return 0;
}

static int threadedJoinCb(NpDrmPipelineState *pipeline, SceUInt32 slotMask, void *userData)
{
	(void)pipeline;
	NpDrmThreadPool *pool = (NpDrmThreadPool *)userData;
	int err = sceKernelWaitEventFlag(pool->joinEventFlag, slotMask,
		SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	if (err < 0) {
		return err;
	}

	for (SceUInt32 idx = 0; idx < pool->workerCount; ++idx) {
		if ((slotMask & (1U << idx)) != 0) {
			NpDrmJobWorker *worker = &pool->workers[idx];

			if (worker->lastResult < 0) {
				return worker->lastResult;
			}
			SceUInt64 endClocks = getThreadRunClocks(worker->threadId);
			SceUInt64 delta = endClocks - worker->startClocks;
			SceInt32 phase = worker->stagePhase;

			pool->stageClocks[phase] += delta;
			++phase;
			if (phase >= 4) {
				phase = 0;
			}
			worker->stagePhase = phase;
		}
	}
	return 0;
}

static int configurePipelineSlots(NpDrmInstallContext *ctx, void *slotMem, SceSize slotMemSize,
	SceUInt32 numJobs)
{
	ctx->pipeline.forkMask = 0U;
	ctx->pipeline.joinMask = 0U;
	ctx->pipeline.chunkCount = 0U;
	ctx->pipeline.currentChunkIndex = 0U;
	ctx->pipeline.chunks = NULL;
	ctx->pipeline.forkCb = npDrmNoopForkCb;
	ctx->pipeline.joinCb = npDrmSyncJoinCb;
	ctx->pipeline.cbUserData = NULL;

	if (numJobs < 2U || numJobs > 32U || slotMemSize < numJobs * sizeof(NpDrmChunkSlot)) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}
	ctx->pipeline.chunkCount = numJobs;
	ctx->pipeline.currentChunkIndex = 0U;
	if (numJobs >= 5U) {
		SceUInt32 tailBit = 1U << (numJobs - 1U);
		SceUInt32 prevBit = 1U << (numJobs - 2U);
		SceUInt32 midBit = 1U << (numJobs - 3U);
		SceUInt32 leadBit = 1U << (numJobs - 4U);

		ctx->pipeline.forkMask = leadBit | midBit | prevBit | tailBit;
		ctx->pipeline.joinMask = 2U | midBit | prevBit | tailBit;
	}
	ctx->pipeline.chunks = (NpDrmChunkSlot *)slotMem;
	return 0;
}

/* 0x8136F32C */
static int configurePipelineBuffers(NpDrmInstallContext *ctx, void *chunkHeap, SceSize heapSize,
	SceSize chunkSize, SceUInt32 flags)
{
	if (ctx->pipeline.chunks == NULL) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}
	SceUInt32 count = ctx->pipeline.chunkCount;

	if (heapSize < chunkSize * count) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}
	SceUInt8 *cursor = (SceUInt8 *)chunkHeap;

	for (SceUInt32 idx = 0; idx < count; ++idx) {
		NpDrmChunkSlot *slot = &ctx->pipeline.chunks[idx];
		SceUInt32 prevIdx = (idx == 0) ? (count - 1U) : (idx - 1U);

		slot->buffer = cursor;
		slot->length = 0U;
		slot->pkgOffset = 0ULL;
		slot->targetFill = chunkSize;
		slot->capacity = chunkSize;
		slot->stage = NPDRM_STAGE_IDLE;
		slot->prevSlot = &ctx->pipeline.chunks[prevIdx];
		cursor += chunkSize;
	}

	if ((flags & 2U) != 0) {
		ctx->pkgMeta.metaFlags |= NPDRM_META_FLAG_HW_CRYPTO;
	} else {
		ctx->pkgMeta.metaFlags &= ~NPDRM_META_FLAG_HW_CRYPTO;
	}
	return 0;
}

/* 0x8136E472 */
static int openInstallContextInternal(NpDrmInstallContext *ctx, const char *dirPath,
	SceUInt32 flags, SceMode mode)
{
	if (dirPath == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}
	const char *nulPos = (const char *)sceClibMemchr(dirPath, '\0', NPDRM_PKG_ROOT_MAX);
	SceSize dirLen = (nulPos != NULL) ? (SceSize)(nulPos - dirPath) : NPDRM_PKG_ROOT_MAX;

	if (dirLen == 0 || dirPath[dirLen - 1U] != '/') {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}
	sceClibMemcpy_safe(ctx->rootDir, dirPath, dirLen);
	ctx->rootDirLen = dirLen;
	resetInstallContextState(ctx, flags, mode);
	resetPerfCounters(((flags & NPDRM_OPEN_FLAG_PERF_LOG) != 0) ? 1U : 0U);

	if ((flags & NPDRM_OPEN_FLAG_RESET_CURSOR) == 0) {
		ctx->currentCursor = ctx->statFile.hashState.pkgHashedBytes;
	} else {
		ctx->currentCursor = 0ULL;
	}
	return ctx->slotIndex;
}

/* 0x8136F01C */
static void printThreadedPerfLog(NpDrmInstallContext *ctx)
{
	if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_PERF_LOG) == 0 || ctx->threadPool == NULL) {
		return;
	}
	int affinityMask = sceKernelGetThreadCpuAffinityMask(0);
	SceUInt32 cpuCount = 0U;

	while (affinityMask != 0) {
		if ((affinityMask & 1) != 0) {
			++cpuCount;
		}
		affinityMask >>= 1;
	}
	if (cpuCount == 0U) {
		cpuCount = 1U;
	}

	SceUInt64 sumWorkerUs = g_npDrmInstallPerf.sha1Us + g_npDrmInstallPerf.decryptUs
		+ g_npDrmInstallPerf.fileWriteUs + g_npDrmInstallPerf.sha256Us;
	SceUInt64 perCoreUs = sumWorkerUs / cpuCount;

	sceClibPrintf(
		"package size: %10lld KB [%s]\n"
		"install time: %10lld ms [%lld,%lld,%lld,%lld,%lld,%lld] (%d)\n",
		(long long)(ctx->pkgMeta.pkgInfo.totalSize >> 10),
		ctx->pkgMeta.pkgInfo.contentId,
		(long long)(g_npDrmInstallPerf.totalWriteUs / 1000ULL),
		(long long)(g_npDrmInstallPerf.sha1Us / 1000ULL),
		(long long)(g_npDrmInstallPerf.decryptUs / 1000ULL),
		(long long)(g_npDrmInstallPerf.fileWriteUs / 1000ULL),
		(long long)(g_npDrmInstallPerf.sha256Us / 1000ULL),
		(long long)(perCoreUs / 1000ULL),
		(long long)(sumWorkerUs / 1000ULL),
		(int)cpuCount);

	resetPerfCounters((SceUInt8)(g_npDrmInstallPerf.enabled & 0xFFULL));
	spinWaitOneSecond();
}

static int closeInstallContextInternal(NpDrmInstallContext *ctx)
{
	int closeActiveErr = 0;
	int closeBodyErr = 0;
	int closeHeadErr = 0;
	int statWriteErr = 0;

	if (ctx->activeFd >= 0) {
		closeActiveErr = sceIoClose(ctx->activeFd);
		ctx->activeFd = -1;
	}
	if (ctx->bodyFd >= 0) {
		closeBodyErr = sceIoClose(ctx->bodyFd);
		ctx->bodyFd = -1;
	}
	if (ctx->headFd >= 0) {
		closeHeadErr = sceIoClose(ctx->headFd);
		ctx->headFd = -1;
	}
	if (closeActiveErr != 0) {
		return closeActiveErr;
	}

	SceUInt64 headSize = (SceUInt64)ctx->pkgMeta.headSize;
	SceUInt64 pkgEnd = ctx->pkgMeta.tailOffset + ctx->pkgMeta.tailSize;
	SceUInt64 hashed = ctx->statFile.hashState.pkgHashedBytes;

	if (headSize != 0ULL && hashed >= headSize && hashed < pkgEnd) {
		int openFlags = ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 67074 : 1538;

		if (ctx->rootDirLen + 25U > NPDRM_PKG_PATH_MAX) {
			statWriteErr = SCE_NPDRM_PKG_ERROR_PATH_TOO_LONG;
		} else {
			sceClibMemcpy_safe(ctx->workPath, ctx->rootDir, ctx->rootDirLen);
			sceClibMemcpy_safe(&ctx->workPath[ctx->rootDirLen], "sce_sys/package/stat.bin", 25U);
			ctx->workPathLen = ctx->rootDirLen + 25U;

			SceUID statFd = sceIoOpen(ctx->workPath, openFlags, 6);
			ctx->activeFd = statFd;
			if (statFd < 0) {
				ctx->activeFd = -1;
				statWriteErr = statFd;
			} else {
				SceSize statSize = ctx->statFile.statSize;
				SceUInt8 *statBody = (SceUInt8 *)&ctx->statFile.magic;

				npDrmStatComputeCmac(ctx->statFile.cmac, statBody, statSize - 16U,
					ctx->pkgMeta.keySlotId);
				npDrmStatEncryptCbc(statBody, NPDRM_PKG_STAT_SIZE_EXT, ctx->pkgMeta.keySlotId);
				int written = sceIoPwrite(ctx->activeFd, &ctx->statFile, statSize, 0);
				npDrmStatDecryptCbc(statBody, NPDRM_PKG_STAT_SIZE_EXT, ctx->pkgMeta.keySlotId);

				if (written < 0) {
					statWriteErr = written;
				} else if ((SceSize)written != statSize) {
					statWriteErr = SCE_NPDRM_PKG_ERROR_SHORT_WRITE;
				}
				if (ctx->activeFd >= 0) {
					int closeErr = sceIoClose(ctx->activeFd);
					ctx->activeFd = -1;
					if (statWriteErr == 0) {
						statWriteErr = closeErr;
					}
				}
			}
		}
	}

	if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_PERF_LOG) != 0 && ctx->threadPool == NULL) {
		g_npDrmInstallPerf.contentPhaseUs += g_npDrmInstallPerf.itemOpenUs
			+ g_npDrmInstallPerf.sha1Us + g_npDrmInstallPerf.decryptUs
			+ g_npDrmInstallPerf.fileCreateUs + g_npDrmInstallPerf.fileWriteUs
			+ g_npDrmInstallPerf.fileCloseUs + g_npDrmInstallPerf.sha256Us
			+ g_npDrmInstallPerf.bodyStatUs;

		SceUInt64 metaIoUs = g_npDrmInstallPerf.headPhaseUs + g_npDrmInstallPerf.tailPhaseUs
			+ g_npDrmInstallPerf.itemOpenUs;
		SceUInt64 allPhaseUs = g_npDrmInstallPerf.headPhaseUs + g_npDrmInstallPerf.contentPhaseUs
			+ g_npDrmInstallPerf.tailPhaseUs;

		sceClibPrintf(
			"package size: %10lld KB [%s]\n"
			"install time: %10lld ms [%lld,%lld,%lld,%lld,%lld,%d,%lld,%lld,%lld,%lld]\n",
			(long long)(ctx->pkgMeta.pkgInfo.totalSize >> 10),
			ctx->pkgMeta.pkgInfo.contentId,
			(long long)(g_npDrmInstallPerf.totalWriteUs / 1000ULL),
			(long long)(g_npDrmInstallPerf.decryptUs / 1000ULL),
			(long long)(g_npDrmInstallPerf.fileCreateUs / 1000ULL),
			(long long)(g_npDrmInstallPerf.fileWriteUs / 1000ULL),
			(long long)(g_npDrmInstallPerf.fileCloseUs / 1000ULL),
			(long long)(metaIoUs / 1000ULL),
			(int)ctx->pkgMeta.pkgInfo.itemCount,
			(long long)(g_npDrmInstallPerf.sha1Us / 1000ULL),
			(long long)(g_npDrmInstallPerf.sha256Us / 1000ULL),
			(long long)(g_npDrmInstallPerf.bodyStatUs / 1000ULL),
			(long long)(allPhaseUs / 1000ULL));

		resetPerfCounters((SceUInt8)(g_npDrmInstallPerf.enabled & 0xFFULL));
		spinWaitOneSecond();
	}

	resetInstallContextState(ctx, 0U, 6);

	if (statWriteErr != 0) {
		return statWriteErr;
	}
	if (closeBodyErr != 0) {
		return closeBodyErr;
	}
	if (closeHeadErr != 0) {
		return closeHeadErr;
	}
	return 0;
}

static SceOff lseekInstallContextInternal(NpDrmInstallContext *ctx, SceOff offset, int whence)
{
	SceOff maxOffset = (SceOff)ctx->maxWrittenOffset;
	SceOff targetOffset;

	if (whence == SCE_SEEK_SET) {
		targetOffset = offset;
	} else if (whence == SCE_SEEK_CUR) {
		targetOffset = (SceOff)ctx->currentCursor + offset;
	} else if (whence == SCE_SEEK_END) {
		targetOffset = maxOffset + offset;
	} else {
		return (SceOff)SCE_NPDRM_PKG_ERROR_INVALID_ARG;
	}

	if (targetOffset < 0 || targetOffset > maxOffset) {
		return (SceOff)SCE_NPDRM_PKG_ERROR_INVALID_OFFSET;
	}
	ctx->currentCursor = (SceUInt64)targetOffset;
	return targetOffset;
}

static int writeInstallContextInternal(NpDrmInstallContext *ctx, const void *buf, SceSize size)
{
	if (size > 0x7FFFFFFFU) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}

	SceUInt64 startStamp = readPerfStamp();
	SceOff curPos = (SceOff)ctx->currentCursor;

	if (curPos < 0 || curPos > (SceOff)ctx->maxWrittenOffset) {
		return SCE_NPDRM_PKG_ERROR_INVALID_OFFSET;
	}

	NpDrmWriteCursor cursor;
	cursor.data = (const SceUInt8 *)buf;
	cursor.length = size;
	cursor.offset = (SceUInt64)curPos;

	int err = npDrmDispatchWriteCursor(ctx, &cursor);
	int ret;

	if (err != 0) {
		ret = err;
	} else {
		if (ctx->maxWrittenOffset < cursor.offset) {
			ctx->maxWrittenOffset = cursor.offset;
		}
		ret = (int)(cursor.offset - (SceUInt64)curPos);
	}

	SceUInt64 endStamp = readPerfStamp();
	g_npDrmInstallPerf.totalWriteUs += endStamp - startStamp;
	if (ret >= 0) {
		ctx->currentCursor += (SceUInt64)ret;
	}
	return ret;
}

static int resumeInstallContextInternal(NpDrmInstallContext *ctx, void *headBuf, SceSize bufSize)
{
	NpDrmIoHandle ioHandle;

	ioHandle.fdPtr = &ctx->activeFd;
	ioHandle.pathBuf = ctx->workPath;
	ioHandle.rootDir = ctx->rootDir;

	if (bufSize < g_npDrmHeadBinSpec.minBufferSize) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}

	int openFlags = ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 65537 : 1;
	int readRet;

	if (ctx->rootDirLen + 25U > NPDRM_PKG_PATH_MAX) {
		readRet = SCE_NPDRM_PKG_ERROR_PATH_TOO_LONG;
	} else {
		sceClibMemcpy_safe(ctx->workPath, ctx->rootDir, ctx->rootDirLen);
		sceClibMemcpy_safe(&ctx->workPath[ctx->rootDirLen], "sce_sys/package/head.bin", 25U);
		ctx->workPathLen = ctx->rootDirLen + 25U;

		SceUID fd = sceIoOpen(ctx->workPath, openFlags, 6);
		ctx->activeFd = fd;
		if (fd < 0) {
			ctx->activeFd = -1;
			readRet = fd;
		} else {
			SceOff fileLen = sceIoLseek(fd, 0, SCE_SEEK_END);

			if (fileLen < 0) {
				readRet = (int)fileLen;
			} else {
				SceSize toRead = (fileLen < (SceOff)bufSize) ? (SceSize)fileLen : bufSize;
				int got = sceIoPread(ctx->activeFd, headBuf, toRead, 0);
				int ioErr = 0;

				if (got < 0) {
					ioErr = got;
				} else if ((SceSize)got != toRead) {
					ioErr = SCE_NPDRM_PKG_ERROR_SHORT_READ;
				}
				if (ioErr != 0) {
					if (ctx->activeFd >= 0) {
						sceIoClose(ctx->activeFd);
						ctx->activeFd = -1;
					}
					readRet = ioErr;
				} else {
					readRet = (int)toRead;
					if (ctx->activeFd >= 0) {
						int closeErr = sceIoClose(ctx->activeFd);
						ctx->activeFd = -1;
						if (closeErr != 0) {
							readRet = closeErr;
						}
					}
				}
			}
		}
	}

	if (readRet < 0) {
		return readRet;
	}

	int err = npDrmProcessPkgHeader(ctx, headBuf, bufSize, 1U);
	if (err < 0) {
		return err;
	}
	if (err == 0) {
		err = npDrmLoadAndVerifyStat(&ctx->statFile, &ioHandle, &ctx->pkgMeta);
		if (err < 0) {
			return err;
		}
		if (err == 0) {
			SceUInt8 actualHeadDigest[32];
			int diff = 0;

			sceSha256Digest(headBuf, ctx->pkgMeta.headSize, actualHeadDigest);
			for (SceSize idx = 0; idx < sizeof(actualHeadDigest); ++idx) {
				diff += (SceUInt8)(ctx->statFile.headSha256Digest[idx] - actualHeadDigest[idx]);
			}
			if (diff != 0) {
				return SCE_NPDRM_PKG_ERROR_CORRUPT_STAT;
			}
			ctx->maxWrittenOffset = ctx->statFile.hashState.pkgHashedBytes;
		}
	}

	ctx->currentCursor = ctx->statFile.hashState.pkgHashedBytes;
	return err;
}

static int readInstallInfoInternal(SceNpDrmInstallInfo *outInfo, const char *dirPath,
	void *headBuf, SceSize bufSize)
{
	int err = npDrmReadDirFile(dirPath, headBuf, bufSize, &g_npDrmHeadBinSpec);
	if (err < 0) {
		return err;
	}

	sceClibMemset(outInfo, 0, sizeof(*outInfo));
	err = npDrmParsePkgInfo(&outInfo->pkgInfo, headBuf, bufSize);
	if (err < 0) {
		return err;
	}

	SceUInt32 contentType = outInfo->pkgInfo.contentType;
	int needsLicense = (contentType == 21U || contentType == 22U || contentType == 31U
		|| contentType == 6U || contentType == 7U || contentType == 14U
		|| contentType == 15U || contentType == 16U || contentType == 17U);

	if (!needsLicense || (outInfo->pkgInfo.packageType & 0x10U) != 0) {
		return npDrmParseInstallInfo(outInfo, headBuf, bufSize);
	}

	SceUInt32 drmType = outInfo->pkgInfo.drmType;
	int workRead = npDrmReadDirFile(dirPath, outInfo->licenseData, sizeof(outInfo->licenseData),
		&g_npDrmWorkBinSpec);
	outInfo->licenseSize = workRead;

	int hasWorkBin = (workRead > 0);
	int isCertWorkBin = 0;
	if (hasWorkBin) {
		static const SceUInt8 kCertMagic[8] = { 0x7F, 'C', 'E', 'R', 0x00, 0x00, 0x00, 0x06 };
		int diff = 0;

		for (SceSize idx = 0; idx < sizeof(kCertMagic); ++idx) {
			diff += (SceUInt8)(outInfo->licenseData[idx] - kCertMagic[idx]);
		}
		isCertWorkBin = (diff == 0);
	}

	if (drmType == 13U) {
		workRead = npDrmReadDirFile(dirPath, outInfo->licenseData, sizeof(outInfo->licenseData),
			&g_npDrmTempBinSpec);
		outInfo->licenseSize = workRead;
	}
	if (workRead < 0) {
		outInfo->licenseSize = 0;
	}
	if (!hasWorkBin) {
		npDrmWriteDirFile(dirPath, outInfo->licenseData, sizeof(outInfo->licenseData),
			&g_npDrmWorkBinSpec);
	}
	if (outInfo->pkgInfo.contentType == 22U) {
		outInfo->instSize = npDrmReadDirFile(dirPath, outInfo->instData, sizeof(outInfo->instData),
			&g_npDrmInstBinSpec);
	}
	if (outInfo->instSize < 0) {
		outInfo->instSize = 0;
	}

	err = npDrmParseInstallInfo(outInfo, headBuf, bufSize);
	if (err != 0) {
		return err;
	}

	if (outInfo->pkgInfo.contentType == 21U && hasWorkBin && !isCertWorkBin) {
		outInfo->licenseSize = npDrmReadDirFile(dirPath, outInfo->licenseData,
			sizeof(outInfo->licenseData), &g_npDrmWorkBinSpec);
	}

	SceUInt32 curType = outInfo->pkgInfo.contentType;
	if (curType == 6U || curType == 7U || curType == 14U || curType == 15U || curType == 16U) {
		NpDrmPkgMeta pkgMeta;

		sceClibMemset(&pkgMeta, 0, sizeof(pkgMeta));
		err = npDrmParsePkgMeta(&pkgMeta, headBuf, bufSize, 0U);
		if (err != 0) {
			return err;
		}
		if (dirPath == NULL) {
			return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
		}
		const char *nulPos = (const char *)sceClibMemchr(dirPath, '\0', NPDRM_PKG_ROOT_MAX);
		SceSize dirLen = (nulPos != NULL) ? (SceSize)(nulPos - dirPath) : NPDRM_PKG_ROOT_MAX;

		if (dirLen == 0 || dirPath[dirLen - 1U] != '/') {
			return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
		}

		struct {
			char rootDir[NPDRM_PKG_ROOT_MAX];
			SceSize rootDirLen;
			char workPath[NPDRM_PKG_PATH_MAX];
			SceSize workPathLen;
		} localPaths;
		SceUID localFd = -1;
		NpDrmIoHandle ioHandle;
		NpDrmStatFile statFile;

		sceClibMemcpy_safe(localPaths.rootDir, dirPath, dirLen);
		localPaths.rootDirLen = dirLen;
		ioHandle.fdPtr = &localFd;
		ioHandle.pathBuf = localPaths.workPath;
		ioHandle.rootDir = localPaths.rootDir;

		err = npDrmLoadAndVerifyStat(&statFile, &ioHandle, &pkgMeta);
		if (err != 0) {
			return err;
		}
		if (statFile.statSize >= NPDRM_PKG_STAT_SIZE_EXT) {
			sceClibMemcpy_safe(outInfo->contentDigest, statFile.ebootPartialDigest,
				sizeof(outInfo->contentDigest));
			outInfo->contentDigestSize = 32;
		}
	}
	return 0;
}

static int readContentInfoInternal(SceNpDrmContentInfo *outInfo, const char *dirPath,
	void *headBuf, SceSize bufSize)
{
	int err = npDrmReadDirFile(dirPath, headBuf, bufSize, &g_npDrmHeadBinSpec);
	if (err < 0) {
		return err;
	}
	sceClibMemset(outInfo, 0, sizeof(*outInfo));
	return npDrmParseContentInfo(outInfo, headBuf, bufSize);
}

} /* namespace */

extern "C" {

SceSize npDrmGetChunkSlotSize(void)
{
	return sizeof(NpDrmChunkSlot);
}

int npDrmExecuteChunkStage(NpDrmPipelineState *pipeline, SceUInt32 chunkIndex)
{
	NpDrmChunkSlot *slot = &pipeline->chunks[chunkIndex];
	SceUInt32 stage = slot->stage;

	if (stage == NPDRM_STAGE_IDLE || stage >= NPDRM_STAGE_DONE) {
		return 0;
	}

	if (stage == NPDRM_STAGE_SHA1) {
		NpDrmChunkSlot *prevSlot = slot->prevSlot;

		if (slot->sha1HashedBytes <= prevSlot->sha1HashedBytes) {
			slot->sha1Ctx = prevSlot->sha1Ctx;
			slot->sha1HashedBytes = prevSlot->sha1HashedBytes;
			slot->sha1TotalBytes = prevSlot->sha1TotalBytes;
		}

		int useHw = (((slot->pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0
			|| (slot->pkgMeta->metaFlags & NPDRM_META_FLAG_HW_CRYPTO) != 0)
			&& g_npDrmHwAccelActive != 0);
		int err = 0;

		if (!useHw) {
			sceSha1BlockUpdate(&slot->sha1Ctx, slot->buffer, slot->length);
			slot->sha1HashedBytes += slot->length;
		} else {
			SceUInt32 remaining = slot->length;
			const SceUInt8 *cursor = slot->buffer;

			if ((slot->sha1HashedBytes & 63ULL) != 0) {
				SceUInt32 lead = (SceUInt32)(64ULL - (slot->sha1HashedBytes & 63ULL));

				if (lead > remaining) {
					lead = remaining;
				}
				sceSha1BlockUpdate(&slot->sha1Ctx, cursor, lead);
				slot->sha1HashedBytes += lead;
				cursor += lead;
				remaining -= lead;
			}
			if (remaining >= 64U) {
				SceUInt32 alignedLen = remaining & ~63U;
				NpDrmTransformState hwState;

				sceClibMemset(&hwState, 0, sizeof(hwState));
				if (slot->sha1Ctx.usRemains == 0) {
					for (SceSize idx = 0; idx < 5U; ++idx) {
						hwState.hashWords[idx] = swapBe32(slot->sha1Ctx.h[idx]);
					}
					hwState.bitCount = slot->sha1Ctx.ullTotalLen * 8ULL;
				}
				err = sceNpDrmPackageTransform((void *)cursor, alignedLen,
					&hwState, NPDRM_TRANSFORM_SHA1);
				if (err >= 0) {
					sceClibMemset(&slot->sha1Ctx, 0, sizeof(slot->sha1Ctx));
					for (SceSize idx = 0; idx < 5U; ++idx) {
						slot->sha1Ctx.h[idx] = swapBe32(hwState.hashWords[idx]);
					}
					slot->sha1Ctx.ullTotalLen = hwState.bitCount >> 3;
					slot->sha1HashedBytes += alignedLen;
					cursor += alignedLen;
					remaining -= alignedLen;
					err = 0;
				}
			}
			if (err == 0 && remaining != 0) {
				sceSha1BlockUpdate(&slot->sha1Ctx, cursor, remaining);
				slot->sha1HashedBytes += remaining;
			}
		}

		recordPerfStep(&g_npDrmInstallPerf.sha1Us);
		if (err != 0) {
			return err;
		}
		if (slot->pkgOffset < slot->itemDataStart) {
			slot->length = 0U;
			slot->pkgOffset = 0ULL;
		}
		slot->stage = NPDRM_STAGE_DECRYPT;
		return 0;
	}

	if (stage == NPDRM_STAGE_DECRYPT) {
		if (slot->length != 0) {
			SceUInt8 useHw = (((slot->pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0
				|| (slot->pkgMeta->metaFlags & NPDRM_META_FLAG_HW_CRYPTO) != 0)
				&& g_npDrmHwAccelActive != 0) ? 1U : 0U;
			int err = npDrmDecryptPkgRegion(slot->buffer, slot->length, slot->pkgOffset,
				slot->pkgMeta, useHw);

			recordPerfStep(&g_npDrmInstallPerf.decryptUs);
			if (err != 0) {
				return err;
			}
		}
		slot->stage = NPDRM_STAGE_SHA256;
		return 0;
	}

	if (stage == NPDRM_STAGE_SHA256) {
		NpDrmChunkSlot *prevSlot = slot->prevSlot;

		if (slot->sha256HashedBytes <= prevSlot->sha256HashedBytes) {
			slot->sha256Ctx = prevSlot->sha256Ctx;
			slot->sha256HashedBytes = prevSlot->sha256HashedBytes;
			slot->sha256TotalBytes = prevSlot->sha256TotalBytes;
		}
		if (slot->length != 0) {
			SceUInt64 relOffset = slot->pkgOffset - slot->itemDataStart;
			SceUInt64 remainInItem = slot->itemDataSize - relOffset;

			slot->pkgOffset = relOffset;
			if (remainInItem < (SceUInt64)slot->length) {
				slot->length = (SceUInt32)remainInItem;
			}
			int err = npDrmHashChunkSha256(slot, &slot->sha256Ctx, &slot->sha256HashedBytes);
			if (err != 0) {
				return err;
			}
		}
		slot->stage = NPDRM_STAGE_WRITE;
		return 0;
	}

	/* NPDRM_STAGE_WRITE */
	if (slot->length != 0) {
		SceUID fd = *slot->pipeline->activeFdPtr;
		int written = sceIoPwrite(fd, slot->buffer, slot->length, (SceOff)slot->pkgOffset);
		int ioErr = 0;

		if (written < 0) {
			ioErr = written;
		} else if ((SceUInt32)written != slot->length) {
			ioErr = SCE_NPDRM_PKG_ERROR_SHORT_WRITE;
		}
		recordPerfStep(&g_npDrmInstallPerf.fileWriteUs);
		if (ioErr != 0) {
			return ioErr;
		}
		slot->length = 0U;
		slot->pkgOffset = 0ULL;
	}
	slot->stage = NPDRM_STAGE_DONE;
	return 0;
}

int sceNpDrmInstallOpen(const char *dirPath, SceUInt32 flags, SceMode mode, void *workCtx)
{
	if (workCtx == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_WORK_CTX;
	}
	NpDrmInstallContext *ctx = (NpDrmInstallContext *)workCtx;
	SceInt32 slotIndex = ctx->slotIndex;

	if (slotIndex < 0 || slotIndex >= NPDRM_PKG_MAX_SLOTS) {
		return SCE_NPDRM_PKG_ERROR_INVALID_SLOT;
	}
	if (slotIndex == 0 && ctx != &g_defaultSlotContext) {
		return SCE_NPDRM_PKG_ERROR_INVALID_SLOT;
	}
	if (g_installSlots[slotIndex] != NULL) {
		return SCE_NPDRM_PKG_ERROR_SLOT_IN_USE;
	}

	sceClibMemset(&ctx->threadPool, 0, sizeof(*ctx) - sizeof(ctx->slotIndex));
	g_installSlots[slotIndex] = ctx;

	int ret = openInstallContextInternal(ctx, dirPath, flags, mode);
	if (ret >= 0) {
		ret += NPDRM_PKG_HANDLE_BASE;
	}
	return ret;
}

int sceNpDrmInstallClose(int handle)
{
	NpDrmInstallContext *ctx = NULL;
	int slotIndex = lookupSlotContext(handle, &ctx);

	if (slotIndex < 0) {
		return slotIndex;
	}

	int poolErr = destroyThreadPool(ctx->threadPool);
	printThreadedPerfLog(ctx);
	int closeErr = closeInstallContextInternal(ctx);

	g_installSlots[slotIndex] = NULL;
	if (poolErr < 0) {
		return poolErr;
	}
	return closeErr;
}

int sceNpDrmInstallResume(int handle, void *headBuf, SceSize bufSize)
{
	NpDrmInstallContext *ctx = NULL;
	int slotIndex = lookupSlotContext(handle, &ctx);

	if (slotIndex < 0) {
		return slotIndex;
	}
	return resumeInstallContextInternal(ctx, headBuf, bufSize);
}

int sceNpDrmInstallWrite(int handle, const void *buf, SceSize size)
{
	NpDrmInstallContext *ctx = NULL;
	int slotIndex = lookupSlotContext(handle, &ctx);

	if (slotIndex < 0) {
		return slotIndex;
	}
	return writeInstallContextInternal(ctx, buf, size);
}

SceOff sceNpDrmInstallLseek(int handle, SceOff offset, int whence)
{
	NpDrmInstallContext *ctx = NULL;
	int slotIndex = lookupSlotContext(handle, &ctx);

	if (slotIndex < 0) {
		return (SceOff)slotIndex;
	}
	return lseekInstallContextInternal(ctx, offset, whence);
}

int sceNpDrmInstallGetPkgInfo(SceNpDrmPkgInfo *outInfo, const void *buf, SceSize size)
{
	int err = npDrmParsePkgInfo(outInfo, buf, size);

	if (err < 0) {
		sceClibMemset(outInfo, 0, sizeof(*outInfo));
	}
	return err;
}

int sceNpDrmInstallGetPkgInfoFromDir(SceNpDrmPkgInfo *outInfo, const char *dirPath,
	void *headBuf, SceSize bufSize)
{
	int err = npDrmReadDirFile(dirPath, headBuf, bufSize, &g_npDrmHeadBinSpec);

	if (err < 0) {
		return err;
	}
	err = npDrmParsePkgInfo(outInfo, headBuf, bufSize);
	if (err < 0) {
		sceClibMemset(outInfo, 0, sizeof(*outInfo));
	}
	return err;
}

int sceNpDrmInstallGetInstallInfo(SceNpDrmInstallInfo *outInfo, SceSize outSize,
	const char *dirPath, void *headBuf, SceSize bufSize)
{
	if (outSize != sizeof(SceNpDrmInstallInfo) || dirPath == NULL || headBuf == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ARG;
	}
	int err = readInstallInfoInternal(outInfo, dirPath, headBuf, bufSize);

	if (err < 0) {
		sceClibMemset(outInfo, 0, sizeof(*outInfo));
	}
	return err;
}

int sceNpDrmInstallGetContentInfo(SceNpDrmContentInfo *outInfo, SceSize outSize,
	const char *dirPath, void *headBuf, SceSize bufSize)
{
	if (outSize != sizeof(SceNpDrmContentInfo) || dirPath == NULL || headBuf == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ARG;
	}
	int err = readContentInfoInternal(outInfo, dirPath, headBuf, bufSize);

	if (err < 0) {
		sceClibMemset(outInfo, 0, sizeof(*outInfo));
	}
	return err;
}

int sceNpDrmInstallSetThreadMode(int handle, void *workMem, SceSize workSize,
	void *chunkHeap, SceSize heapSize, SceSize chunkSize, SceUInt32 numJobs, SceUInt32 flags)
{
	SceSize poolBytes = numJobs * sizeof(NpDrmJobWorker) + 64U;
	SceSize slotsBytes = npDrmGetChunkSlotSize() * numJobs;

	if (workSize < poolBytes + slotsBytes) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}

	NpDrmInstallContext *ctx = NULL;
	int slotIndex = lookupSlotContext(handle, &ctx);

	if (slotIndex < 0) {
		return slotIndex;
	}

	NpDrmThreadPool *pool = (NpDrmThreadPool *)workMem;
	SceUInt8 *workBytes = (SceUInt8 *)workMem;
	void *slotMem = &workBytes[poolBytes];

	int err = createThreadPool(pool, poolBytes, numJobs, flags);
	if (err < 0) {
		cleanupFailedThreadPool(pool, poolBytes, numJobs);
		return err;
	}
	err = configurePipelineSlots(ctx, slotMem, slotsBytes, numJobs);
	if (err < 0) {
		cleanupFailedThreadPool(pool, poolBytes, numJobs);
		return err;
	}
	err = configurePipelineBuffers(ctx, chunkHeap, heapSize, chunkSize, flags);
	if (err < 0) {
		cleanupFailedThreadPool(pool, poolBytes, numJobs);
		return err;
	}

	ctx->pipeline.forkCb = threadedForkCb;
	ctx->pipeline.joinCb = threadedJoinCb;
	ctx->pipeline.cbUserData = pool;

	for (SceUInt32 idx = 0; idx < pool->workerCount; ++idx) {
		NpDrmWorkerThreadArg arg;

		arg.worker = &pool->workers[idx];
		arg.workerIndex = idx;
		arg.forkEventFlag = pool->forkEventFlag;
		arg.joinEventFlag = pool->joinEventFlag;

		err = sceKernelStartThread(pool->workers[idx].threadId, sizeof(arg), &arg);
		if (err < 0) {
			cleanupFailedThreadPool(pool, poolBytes, numJobs);
			return err;
		}
	}

	ctx->threadPool = pool;
	return 0;
}

} /* extern "C" */
