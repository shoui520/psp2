/*
 * GPU Live Metrics session manager and SceHudLiveTrace worker thread.
 */
#include "razor_hud_internal.h"
#include "hud_assets.h"

typedef struct HudLiveState {
	SceKernelLwMutexWork mutex; /* 0..31 */
	int32_t threadId;           /* 32 */
	struct HudLiveState *self;  /* 36 */
	uint32_t state;             /* 40: 0=stopped, 1=running, 2=error */
	uint32_t paBufInfo[2];      /* 44..51 */
	uint32_t stopFlag;          /* 52 */
	uint32_t decoder[640];      /* 56..2615 */
	uint32_t entryCount;        /* 2616 */
	uint32_t overflowCount;     /* 2620 */
	uint32_t bufferSize;        /* 2624 */
	uint32_t bufferOffset;      /* 2628 */
	uint64_t startTimeNs;       /* 2632 */
	uint64_t endTimeNs;         /* 2640 */
	void *resultBuffer;         /* 2648 */
	uint32_t metricsGroup;      /* 2652 */
	uint32_t gpuFreq;           /* 2656 */
	uint32_t liveTailPad;           /* 2660 */
} HudLiveState;

typedef struct HudLiveSession {
	void *paMgr;
	HudLiveState *state;
} HudLiveSession;

int hudMgrLiveGetMetricsGroup(void *mgr)
{
	void *live = *(void **)((uint8_t *)mgr + 1476);
	if (live != NULL)
		return hudLiveGetMetricsGroup(live);
	return 0;
}

int hudMgrLiveSetMetricsGroup(void *mgr, int internal, uint32_t group)
{
	uint8_t *m = (uint8_t *)mgr;
	int mode = *(int *)(m + 188);
	void *live;

	if (mode == 4) {
		if (internal == 0)
			return HUD_ERR_WRONG_MODE;
	} else if (mode != 3) {
		return HUD_ERR_WRONG_MODE;
	}
	live = *(void **)(m + 1476);
	if (live != NULL)
		return hudLiveSetMetricsGroup(live, group);
	return HUD_ERR_NOT_INITIALIZED;
}

int hudMgrLiveStart(void *mgr, int internal)
{
	uint8_t *m = (uint8_t *)mgr;
	if ((*(uint32_t *)(m + 144) & 0x10U) != 0) {
		if (internal == 0)
			return HUD_ERR_WRONG_MODE;
	} else if (*(int *)(m + 188) != 3) {
		return HUD_ERR_WRONG_MODE;
	}
	if (*(void **)(m + 1384) != NULL && *(void **)(m + 1476) != NULL)
		return hudLiveStart(*(void **)(m + 1476));
	return HUD_ERR_NOT_INITIALIZED;
}

int hudMgrLiveIsRunning(void *mgr)
{
	uint8_t *m = (uint8_t *)mgr;
	if (*(void **)(m + 1384) == NULL)
		return 0;
	if (*(void **)(m + 1476) != NULL)
		return hudLiveIsRunning(*(void **)(m + 1476));
	return 0;
}

int hudMgrLiveStop(void *mgr, int internal)
{
	uint8_t *m = (uint8_t *)mgr;
	if ((*(uint32_t *)(m + 144) & 0x10U) != 0) {
		if (internal == 0)
			return HUD_ERR_WRONG_MODE;
	} else if (*(int *)(m + 188) != 3) {
		return HUD_ERR_WRONG_MODE;
	}
	if (*(void **)(m + 1476) != NULL && *(void **)(m + 1384) != NULL)
		return hudLiveStop(*(void **)(m + 1476));
	return HUD_ERR_NOT_INITIALIZED;
}

int hudMgrLiveSetBuffer(void *mgr, int internal, void *buf, uint32_t size, void *results)
{
	uint8_t *m = (uint8_t *)mgr;
	if ((*(uint32_t *)(m + 144) & 0x10U) != 0) {
		if (internal == 0)
			return HUD_ERR_WRONG_MODE;
	} else if (*(int *)(m + 188) == 3) {
		if (internal == 0 && (results == NULL || buf == NULL))
			return HUD_ERR_NULL_POINTER;
	} else {
		return HUD_ERR_WRONG_MODE;
	}
	if (*(void **)(m + 1476) != NULL && *(void **)(m + 1384) != NULL)
		return hudLiveSetBuffer(*(void **)(m + 1476), buf, size, results);
	return HUD_ERR_NOT_INITIALIZED;
}

int hudMgrLiveDump(void *mgr, int internal, const char *path)
{
	uint8_t *m = (uint8_t *)mgr;
	if ((*(uint32_t *)(m + 144) & 0x10U) != 0) {
		if (internal == 0)
			return HUD_ERR_WRONG_MODE;
	} else if (*(int *)(m + 188) != 3) {
		return HUD_ERR_WRONG_MODE;
	}
	if (*(void **)(m + 1476) != NULL && *(void **)(m + 1384) != NULL)
		return hudLiveDump(*(void **)(m + 1476), path);
	return HUD_ERR_NOT_INITIALIZED;
}

void hudMgrGxmFinish(void *mgr)
{
	SceGxmContext *ctx = *(SceGxmContext **)((uint8_t *)mgr + 196);
	if (ctx != NULL)
		sceGxmFinish(ctx);
}

int hudMgrTraceSetFilename(void *mgr, const char *filename, uint32_t countLen)
{
	uint8_t *m = (uint8_t *)mgr;
	if (filename == NULL || countLen > 16U)
		return HUD_ERR_INVALID_PARAM;
	sceClibStrncpy((char *)(m + 1124), filename, 255U);
	*(uint32_t *)(m + 1380) = countLen;
	m[1379] = '\0';
	return 0;
}

char *hudMgrTraceGetFilename(void *mgr, char *dst, int size, uint32_t *countLen)
{
	uint8_t *m = (uint8_t *)mgr;
	char *ret = sceClibStrncpy(dst, (const char *)(m + 1124), (uint32_t)(size - 1));
	dst[size - 1] = '\0';
	*countLen = *(uint32_t *)(m + 1380);
	return ret;
}

int hudMgrSetDisplayEnabled(void *mgr, uint8_t enabled)
{
	*((uint8_t *)mgr + 2000) = enabled;
	return 0;
}

int hudMgrSetDisplayFrameCount(void *mgr, int count)
{
	if (count <= 0)
		return HUD_ERR_INVALID_PARAM;
	*(int *)((uint8_t *)mgr + 2072) = count;
	return 0;
}

int hudMgrGetDisplayFrameCount(void *mgr)
{
	return *(int *)((uint8_t *)mgr + 2072);
}

int hudMgrSetSceneColorMode(void *mgr, int mode)
{
	*(int *)((uint8_t *)mgr + 156) = mode;
	return 0;
}

int hudMgrGetSceneColorMode(void *mgr)
{
	return *(int *)((uint8_t *)mgr + 156);
}

int hudMgrTraceSetMetricsGroup(void *mgr, int group)
{
	uint8_t *m = (uint8_t *)mgr;
	int ret;

	if (*(int *)(m + 188) != 2)
		return HUD_ERR_WRONG_MODE;
	if (group < 0)
		return HUD_ERR_INVALID_PARAM;
	ret = hudConfigLookupMetricsGroup((uint32_t)group, m + 1388);
	if (ret == 0) {
		void *pa = *(void **)(m + 1384);
		if (pa != NULL)
			hudPaStop(pa);
		m[1480] = 1;
	}
	return ret;
}

int hudLiveThreadEntry(uint32_t arglen, void *argp)
{
	HudLiveState *st = *(HudLiveState **)argp;
	(void)arglen;

	for (;;) {
		uint32_t wrPtr, tbFreq;
		uint64_t tbVal;
		sceKernelLockLwMutex(&st->mutex, 1, NULL);
		if (st->stopFlag != 0)
			break;
		wrPtr = sceKernelPaGetWritePointer();
		tbVal = sceKernelPaGetTimebaseValue();
		tbFreq = st->decoder[744 / 4];
		st->endTimeNs = (tbVal / (uint64_t)tbFreq) * 1000ULL;
		hudPaDecoderDrain(st->decoder, st->paBufInfo, 0, wrPtr,
			st->resultBuffer, st->bufferSize, &st->bufferOffset,
			&st->entryCount, &st->overflowCount);
		sceKernelUnlockLwMutex(&st->mutex, 1);
		sceKernelDelayThread(1000U);
	}
	sceKernelUnlockLwMutex(&st->mutex, 1);
	return 0;
}

int hudLiveStartPa(void *statePtr, void *paMgr)
{
	HudLiveState *st = (HudLiveState *)statePtr;
	uint32_t grp = st->metricsGroup;
	uint64_t flags = (uint64_t)g_gpuLiveMetricsTable[grp][0] | ((uint64_t)g_gpuLiveMetricsTable[grp][1] << 32);
	int enableCounter = (int)(flags & 1ULL);
	int enableGpu = (flags & 2ULL) != 0 ? 1 : 0;
	uint32_t counters[16];
	uint64_t mask = 0;
	int ret;

	if (grp > 3U) {
		sceClibMemset(counters, 0, sizeof(counters));
	} else {
		const uint8_t *mbytes = (const uint8_t *)&g_gpuLiveMetricsTable[grp][18];
		int i;
		sceClibMemcpy_safe(&counters[0], &g_gpuLiveMetricsTable[grp][2], 0x20U);
		sceClibMemcpy_safe(&counters[8], &g_gpuLiveMetricsTable[grp][10], 0x20U);
		for (i = 0; i < 8; i++)
			mask = (mask << 8) | mbytes[i];
	}

	hudPaSetBuffer(paMgr, 0, 0x4000000U);
	hudPaSetTraceFlags(paMgr, 1, enableCounter, enableGpu);
	hudPaSetConfig(paMgr, counters, (uint32_t)mask, (uint32_t)(mask >> 32), 13320U);
	ret = hudPaSetup(paMgr);
	if (ret == 0) {
		uint32_t tbFreq = sceKernelPaGetTimebaseFrequency();
		hudPaDecoderInit(st->decoder, paMgr, st->metricsGroup, st->gpuFreq, tbFreq, 13320U);
		ret = hudPaStart(paMgr);
		if (ret == 0)
			hudPaGetBufferInfo(paMgr, st->paBufInfo);
	}
	return ret;
}

void *hudLiveCtor(void *objPtr, void *paMgr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = (HudLiveState *)hudNewAligned(sizeof(HudLiveState), 8U);

	sess->paMgr = paMgr;
	st->threadId = -1;
	st->self = st;
	st->stopFlag = 0;
	st->state = 2U;
	hudPaDecoderCtor(st->decoder);
	st->entryCount = 0;
	st->overflowCount = 0;
	st->bufferSize = 0;
	st->bufferOffset = 0;
	st->startTimeNs = 0;
	st->endTimeNs = 0;
	st->resultBuffer = NULL;
	st->metricsGroup = 1U;
	sess->state = st;

	if (sceKernelCreateLwMutex(&st->mutex, "SceHudLiveTrace", 0U, 0, NULL) != 0) {
		st->state = 2U;
		return sess;
	}
	st->threadId = sceKernelCreateThread("SceHudLiveTrace", hudLiveThreadEntry,
		0x10000100 - 1, 0x2000U, 0U, 0, NULL);
	if (st->threadId >= 0) {
		st->state = 0U;
	} else {
		st->threadId = -1;
	}
	return sess;
}

void *hudLiveDtor(void *objPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = sess->state;

	if (st->state == 1U) {
		int status;
		sceKernelLockLwMutex(&st->mutex, 1, NULL);
		st->stopFlag = 1U;
		sceKernelUnlockLwMutex(&st->mutex, 1);
		if (sceKernelWaitThreadEnd(st->threadId, &status, NULL) != 0) {
			st->state = 2U;
		} else {
			st->state = 0U;
			if (hudPaIsRunning(sess->paMgr) != 0)
				hudPaStop(sess->paMgr);
			st->resultBuffer = NULL;
			st->entryCount = 0;
			st->overflowCount = 0;
			st->bufferSize = 0;
			st->bufferOffset = 0;
		}
	}
	if (sceKernelDeleteThread(st->threadId) == 0)
		sceKernelDeleteLwMutex(&st->mutex);
	return sess;
}

int hudLiveGetMetricsGroup(void *objPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	return (int)sess->state->metricsGroup;
}

int hudLiveSetMetricsGroup(void *objPtr, uint32_t group)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = sess->state;

	if (st->state == 1U)
		return HUD_ERR_ALREADY_STARTED;
	st->resultBuffer = NULL;
	st->entryCount = 0;
	st->overflowCount = 0;
	st->bufferSize = 0;
	st->bufferOffset = 0;
	if (group > 4U)
		return HUD_ERR_INVALID_PARAM;
	st->metricsGroup = group;
	return 0;
}

int hudLiveIsRunning(void *objPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	return sess->state->state == 1U ? 1 : 0;
}

int hudLiveStart(void *objPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = sess->state;
	uint32_t counters[16];
	int ret;

	if (st->state == 2U)
		return HUD_ERR_NOT_INITIALIZED;
	st->gpuFreq = hudGpuGetClockFreq();
	sceClibMemcpy_safe(&counters[0], &g_gpuLiveMetricsTable[st->metricsGroup][2], 0x20U);
	sceClibMemcpy_safe(&counters[8], &g_gpuLiveMetricsTable[st->metricsGroup][10], 0x20U);
	hudGpuSetPerfCounters(counters);

	if (st->state != 0U)
		return HUD_ERR_ALREADY_STARTED;
	st->stopFlag = 0;
	st->resultBuffer = NULL;
	st->entryCount = 0;
	st->overflowCount = 0;
	st->bufferSize = 0;
	st->bufferOffset = 0;
	ret = hudLiveStartPa(st, sess->paMgr);
	if (ret == 0) {
		if (st->threadId < 0 || (ret = sceKernelStartThread(st->threadId, 4U, &st->self)) != 0)
			st->state = 2U;
		else
			st->state = 1U;
	}
	return ret;
}

int hudLiveStop(void *objPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = sess->state;
	int ret, status;

	if (st->state == 2U)
		return HUD_ERR_NOT_INITIALIZED;
	if (st->state != 1U)
		return HUD_ERR_NOT_STARTED;

	sceKernelLockLwMutex(&st->mutex, 1, NULL);
	st->stopFlag = 1U;
	sceKernelUnlockLwMutex(&st->mutex, 1);
	ret = sceKernelWaitThreadEnd(st->threadId, &status, NULL);
	if (ret != 0) {
		st->state = 2U;
	} else {
		st->state = 0U;
		if (hudPaIsRunning(sess->paMgr) != 0)
			hudPaStop(sess->paMgr);
		st->resultBuffer = NULL;
		st->entryCount = 0;
		st->overflowCount = 0;
		st->bufferSize = 0;
		st->bufferOffset = 0;
	}
	return ret;
}

int hudLiveSetBuffer(void *objPtr, void *buf, uint32_t size, void *resultsPtr)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	HudLiveState *st = sess->state;
	SceRazorGpuLiveResultInfo *results = (SceRazorGpuLiveResultInfo *)resultsPtr;
	uint32_t wrPtr, tbFreq;
	uint64_t tbVal;
	int ret;

	if (st->state == 2U)
		return HUD_ERR_NOT_INITIALIZED;
	if (st->state != 1U)
		return HUD_ERR_NOT_STARTED;

	sceKernelLockLwMutex(&st->mutex, 1, NULL);
	wrPtr = sceKernelPaGetWritePointer();
	tbVal = sceKernelPaGetTimebaseValue();
	tbFreq = st->decoder[744 / 4];
	st->endTimeNs = (tbVal / (uint64_t)tbFreq) * 1000ULL;
	hudPaDecoderDrain(st->decoder, st->paBufInfo, 1, wrPtr,
		st->resultBuffer, st->bufferSize, &st->bufferOffset,
		&st->entryCount, &st->overflowCount);
	st->gpuFreq = hudGpuGetClockFreq();
	hudPaDecoderSetGpuFreq(st->decoder, st->gpuFreq);

	if (results != NULL) {
		results->resultData = st->resultBuffer;
		results->entryCount = st->entryCount;
		results->bufferSize = st->bufferSize;
		results->overflowCount = st->overflowCount;
		results->startTime = st->startTimeNs / 1000ULL;
		results->endTime = st->endTimeNs / 1000ULL;
		results->metricGroup = st->metricsGroup;
	}

	if (((uintptr_t)buf & 7U) != 0) {
		ret = HUD_ERR_UNALIGNED;
	} else {
		st->resultBuffer = buf;
		st->entryCount = 0;
		st->overflowCount = 0;
		st->bufferSize = size;
		st->bufferOffset = 0;
		ret = 0;
	}
	st->startTimeNs = st->endTimeNs;
	sceKernelUnlockLwMutex(&st->mutex, 1);
	return ret;
}

int hudLiveGetCounters(void *objPtr, void *dst)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	uint32_t grp = sess->state->metricsGroup;
	sceClibMemcpy_safe(dst, &g_gpuLiveMetricsTable[grp][2], 0x20U);
	sceClibMemcpy_safe((uint8_t *)dst + 32, &g_gpuLiveMetricsTable[grp][10], 0x20U);
	return 0;
}

int hudLiveDump(void *objPtr, const char *path)
{
	HudLiveSession *sess = (HudLiveSession *)objPtr;
	uint32_t freq = hudGpuGetClockFreq();
	return hudPaWritePmonFile(sess->paMgr, freq, path);
}
