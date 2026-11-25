/*
 * SceRazorCapture: GXM init callbacks, capture trigger & frame lifecycle,
 * 20-job salvage ring buffer, and scene/kick capture handlers.
 */

#include "razor_capture_internal.h"

/* External helpers from analyse.c, capture_file.c, sgx_decode.c, usse_decode.c */
extern void razorAnalyseVdmStream(void *ctx, const uint32_t *jobDesc);
extern int razorAnalyseWrite3dState(void *ctx, const void *kickInfo);
extern int razorAnalyse3dPdsState(void *ctx, const uint32_t *kickInfo);
extern void razorAnalyseSinglePds(void *ctx, uint32_t gpuAddr, uint32_t dataSize);
extern void razorPageDirClear(RazorPageBlock **pageDir);
extern uint64_t razorCaptureFileOpen(RazorCaptureFile *file, const char *filename, const void *initData);
extern int razorCaptureFileClose(RazorCaptureFile *file);
extern int razorCaptureFileWriteFrameInfo(RazorCaptureFile *file, uint32_t argB, uint32_t argC);
extern uint64_t razorCaptureFileFlushMemory(RazorCaptureFile *file, int captureAll);
extern int razorCaptureFileWriteSync(RazorCaptureFile *file);
extern int razorCaptureFileWriteColorSurface(RazorCaptureFile *file, uint32_t surfaceId, const uint32_t *rtWords,
					     uint32_t flags, const uint32_t *extra);
extern int razorCaptureFileWriteDepthStencil(RazorCaptureFile *file, const void *dsData);
extern int razorCaptureFileWriteSceneInfo(RazorCaptureFile *file, const void *infoA, const void *infoB, const void *infoC);
extern int razorCaptureFileWritePdsState(RazorCaptureFile *file, const void *data);
extern int razorCaptureFileWriteMarkerRange(RazorCaptureFile *file, uint32_t startAddr, uint32_t endAddr);
extern void *sgxPdsVmInit(void *vm);
extern void *sgxUsseVmInit(void *vm);
extern void *sgxRegBankInit(void *bank);

/* Trigger request (528 bytes = 0x210) */
typedef struct RazorTriggerReq {
	uint32_t startFrame;
	uint32_t endFrame;
	char filename[512];
	uint8_t fromHost;
	uint8_t captureAllMem;
	uint8_t captureBeforeKick;
	uint8_t pad[5];
} RazorTriggerReq;

/* Full capture state (161448 bytes = 0x276A8 at 0x81014EC0) */
typedef struct RazorCaptureState {
	uint32_t jobId;                  /* +0 */
	uint32_t jobFlags;               /* +4 */
	uint32_t vdmCtrlCpuAddr;         /* +8 */
	uint32_t jobWord3;               /* +12 */
	uint32_t jobWord4;               /* +16 */
	uint32_t pdsBaseGpuAddr;         /* +20 */
	uint32_t useBaseGpuAddrs[16];    /* +24 .. +87 */
	RazorCaptureFile file;           /* +88 .. +122487 (errorFlags at +122480) */
	uint64_t errorFlags;             /* +122488 */
	uint32_t pdsVm0[136];            /* +122496 */
	uint32_t usseList0[182];         /* +123040 */
	uint32_t tagEmitList[92];        /* +123768 */
	uint32_t usseVm[2068];           /* +124136 */
	SgxRegBank paBank;               /* +132408 */
	uint32_t pdsRes0[382];           /* +134520 */
	uint32_t pdsVm1[136];            /* +136048 */
	uint32_t usseList1[182];         /* +136592 */
	uint32_t usseList2[182];         /* +137320 */
	uint32_t pdsRes1[382];           /* +138048 */
	uint32_t pdsVm2[136];            /* +139576 */
	uint32_t pdsRes2[564];           /* +140120 */
	uint32_t pdsVm3[136];            /* +142376 */
	uint32_t pdsRes3[564];           /* +142920 */
	uint32_t saPdsRes[382];          /* +145176 */
	SgxRegBank saBank;               /* +146704 */
	uint32_t saPdsGpuAddr;           /* +148816 */
	uint32_t saPdsPad;               /* +148820 */
	uint32_t fragPdsRes[382];        /* +148824 */
	SgxRegBank fragBank;             /* +150352 */
	uint32_t mergedTagState[26];     /* +152464 */
	RazorProgramCache progCache;     /* +152568 */
	RazorRangeList ranges;           /* +152704 .. +160903 */
	RazorTriggerReq activeReq;       /* +160904 .. +161431 */
	uint32_t initParams[4];          /* +161432 .. +161447 */
} RazorCaptureState;

/* Salvage job ring entry (976 bytes = 244 dwords) */
typedef struct RazorSalvageJob {
	uint32_t flags;                  /* +0 (bit 0=sync, bit 1=colorSurf, bit 2=kick) */
	uint32_t frameNum;               /* +4 */
	uint32_t jobDesc[22];            /* +8 .. +95 (88 bytes) */
	uint32_t kickInfo[189];          /* +96 .. +851 (756 bytes) */
	uint32_t surfaceId;              /* +852 (dword 213) */
	uint32_t rtWords[6];             /* +856 .. +879 (dwords 214..219) */
	uint32_t surfaceFlags;           /* +880 (dword 220) */
	uint32_t surfaceExtra[4];        /* +884 .. +899 (dwords 221..224) */
	uint32_t queryInfo[19];          /* +900 .. +975 (76 bytes, dwords 225..243) */
} RazorSalvageJob;

typedef struct RazorSalvageRing {
	int32_t head;
	int32_t count;
	RazorSalvageJob jobs[20];
} RazorSalvageRing;

static RazorSalvageRing s_salvageRing;
static RazorCaptureState *s_statePtr;
static RazorTriggerReq s_pendingReq;
static uint8_t s_salvageActive;
static uint8_t s_triggerPending;
static uint8_t s_salvageDeferred;
static SceKernelLwMutexWork s_salvageMutex;
static RazorCaptureState s_stateStorage;
static uint32_t s_frameCounter;
static char s_salvageFilename[512];
static uint8_t s_isCapturing;

/* --- Small callbacks --- */

int razorCaptureIsCapturingCb(void)
{
	return s_isCapturing;
}

int razorCaptureIsSalvageCb(void)
{
	return s_salvageActive;
}

void razorCaptureSetInitParamsCb(uint32_t partA, uint32_t partB, uint32_t partC)
{
	if (s_statePtr != NULL) {
		s_statePtr->initParams[0] = partA;
		s_statePtr->initParams[1] = partB;
		s_statePtr->initParams[2] = partC;
		s_statePtr->initParams[3] = 0;
	}
}

int razorCaptureBeforeKickCb(void)
{
	RazorCaptureState *st = s_statePtr;

	if (!st->activeReq.captureBeforeKick || !s_isCapturing)
		return 0;

	sceClibPrintf(st->activeReq.fromHost
		      ? "razor_capture: Writing memory...\n"
		      : "razor_capture: Writing memory to file...\n");
	st->errorFlags |= razorCaptureFileFlushMemory(&st->file, st->activeReq.captureAllMem);
	sceClibPrintf("razor_capture: Writing memory complete.\n");
	return 0;
}

/* --- 0x810004b4: write scene info from queryInfo --- */

int razorCaptureWriteSceneQuery(RazorCaptureState *st, const uint32_t *queryInfo)
{
	uint32_t infoA[10];
	uint32_t infoB[8];
	uint32_t infoC[4];
	uint32_t flags = queryInfo[18] & 0x1FU;
	int i;

	infoA[0] = queryInfo[0];
	infoA[1] = queryInfo[1];
	infoA[2] = queryInfo[2];
	infoA[3] = queryInfo[3];
	infoA[4] = queryInfo[4];
	infoA[5] = queryInfo[5];
	infoA[6] = queryInfo[6];
	infoA[7] = queryInfo[7];
	infoA[8] = queryInfo[8];
	infoA[9] = queryInfo[9];

	for (i = 0; i < 8; i++)
		infoB[i] = queryInfo[10 + i];

	infoC[0] = flags;
	infoC[1] = 0;
	infoC[2] = 0;
	infoC[3] = 0;
	return razorCaptureFileWriteSceneInfo(&st->file, infoA, infoB, infoC);
}

/* --- 0x810005b8: salvage dump callback --- */

static inline __attribute__((always_inline)) void razorCaptureWriteAllFrameInfo(RazorCaptureFile *file)
{
	razorCaptureFileWriteFrameInfo(file, 0, 1);
	razorCaptureFileWriteFrameInfo(file, 1, 14);
	razorCaptureFileWriteFrameInfo(file, 2, 1);
	razorCaptureFileWriteFrameInfo(file, 3, 1);
	razorCaptureFileWriteFrameInfo(file, 4, 16);
	razorCaptureFileWriteFrameInfo(file, 5, 16);
	razorCaptureFileWriteFrameInfo(file, 6, 1);
	razorCaptureFileWriteFrameInfo(file, 7, 1);
}

void razorCaptureSalvageDump(void)
{
	RazorCaptureState *st = s_statePtr;
	uint64_t openErr;
	int lockRes;
	int idx;

	sceClibPrintf("razor_capture: Attempting salvage to %s...\n", s_salvageFilename);
	openErr = razorCaptureFileOpen(&st->file, s_salvageFilename, st->initParams);
	st->errorFlags = openErr;
	if (openErr != 0ULL) {
		sceClibPrintf("razor_capture: Failed to open salvage file\n");
		return;
	}

	razorCaptureWriteAllFrameInfo(&st->file);

	do {
		unsigned int timeout = 1000000U;
		lockRes = sceKernelLockLwMutex(&s_salvageMutex, 1, &timeout);
	} while (lockRes == (int)0x80028005);

	if (lockRes != 0)
		return;

	for (idx = 19; idx >= 0; idx--) {
		RazorSalvageJob *job;
		int i;

		if (s_salvageRing.count <= idx)
			continue;

		job = &s_salvageRing.jobs[(idx + s_salvageRing.head) % 20];
		st->errorFlags = 0ULL;
		sceClibPrintf("razor_capture: Trying to capture job\n");
		razorCaptureFileClearPages(&st->file);
		razorProgramCacheReset(&st->progCache);

		st->jobId = 0;
		st->jobFlags = (uint8_t)job->jobDesc[2];
		st->vdmCtrlCpuAddr = razorCaptureFileGpuToCpu(&st->file, job->jobDesc[3]);
		st->jobWord3 = 0;
		st->jobWord4 = 0;
		st->pdsBaseGpuAddr = job->jobDesc[4];
		for (i = 0; i < 16; i++)
			st->useBaseGpuAddrs[i] = job->jobDesc[6 + i];

		razorAnalyseVdmStream(st, job->jobDesc);

		if (job->flags & 2U)
			razorCaptureFileWriteColorSurface(&st->file, job->surfaceId, job->rtWords,
							  job->surfaceFlags, job->surfaceExtra);

		if (job->flags & 4U) {
			uint32_t dsChunk[8];
			uint32_t pdsChunk[12];

			razorCaptureWriteSceneQuery(st, job->queryInfo);

			dsChunk[0] = job->kickInfo[23];
			dsChunk[1] = job->queryInfo[0];
			dsChunk[2] = job->queryInfo[1];
			dsChunk[3] = job->queryInfo[2];
			dsChunk[4] = job->kickInfo[19];
			dsChunk[5] = job->kickInfo[20];
			dsChunk[6] = job->kickInfo[17];
			dsChunk[7] = job->kickInfo[18];
			razorCaptureFileWriteDepthStencil(&st->file, dsChunk);

			pdsChunk[0] = job->kickInfo[9];
			pdsChunk[1] = job->queryInfo[3];
			pdsChunk[2] = job->kickInfo[10];
			pdsChunk[3] = job->kickInfo[26];
			pdsChunk[4] = job->kickInfo[58];
			pdsChunk[5] = job->kickInfo[59];
			pdsChunk[6] = job->kickInfo[60];
			pdsChunk[7] = 0;
			pdsChunk[8] = 0;
			pdsChunk[9] = 0;
			pdsChunk[10] = job->queryInfo[4];
			pdsChunk[11] = job->queryInfo[5];
			razorCaptureFileWritePdsState(&st->file, pdsChunk);

			razorAnalyseWrite3dState(st, job->kickInfo);
		}

		sceClibPrintf("\nrazor_capture: Writing memory to file...\n");
		st->errorFlags |= razorCaptureFileFlushMemory(&st->file, 0);
		sceClibPrintf("razor_capture: Errors after this job: %llx...\n", st->errorFlags);

		if (job->flags & 1U)
			razorCaptureFileWriteSync(&st->file);
	}

	sceKernelUnlockLwMutex(&s_salvageMutex, 1);
	razorCaptureFileClose(&s_statePtr->file);
	sceClibPrintf("razor_capture: Salvage complete.\n");
}

/* --- Mapping & marker callbacks --- */

int razorCaptureUnregisterRangeCb(uint32_t addr)
{
	if (s_statePtr == NULL)
		return 2;
	return razorRangeListRemove(&s_statePtr->ranges, addr) ? 0 : 2;
}

int razorCaptureRegisterRangeCb(uint32_t addr, uint32_t size)
{
	if (s_statePtr == NULL)
		return 2;
	return razorRangeListAdd(&s_statePtr->ranges, addr, size) ? 0 : 2;
}

int razorCaptureUnmapMemCb(uint32_t handle, uint32_t cpuAddr)
{
	return razorCaptureFileRemoveMapping(&s_statePtr->file, handle, cpuAddr);
}

int razorCaptureMapMemCb(uint32_t handle, uint32_t cpuAddr, uint32_t gpuAddr, uint32_t size)
{
	return razorCaptureFileAddMapping(&s_statePtr->file, handle, cpuAddr, gpuAddr, size);
}

int razorCapturePushMarkerCb(uint32_t contextId, const char *name)
{
	return razorMarkerPush(contextId, name) ^ 1;
}

int razorCaptureResetMarkerCb(uint32_t contextId)
{
	return razorMarkerReset(contextId);
}

int razorCaptureTermCb(void)
{
	razorMarkerTerm();
	razorCaptureFileReset(&s_statePtr->file);
	razorProgramCacheNoop();
	razorPageDirClear(s_statePtr->file.pageDir);
	if (s_salvageActive)
		razorSalvageStopThread();
	sceKernelDeleteLwMutex(&s_salvageMutex);
	return 0;
}

/* --- Color surface / sync / kick callbacks --- */

int razorCaptureColorSurfaceCb(uint32_t surfaceId, const uint32_t *rtWords, uint32_t flags, const uint32_t *extra)
{
	if (s_salvageActive) {
		unsigned int timeout;
		do {
			timeout = 1000000U;
		} while (sceKernelLockLwMutex(&s_salvageMutex, 1, &timeout) == (int)0x80028005);

		if (s_salvageRing.count > 0) {
			RazorSalvageJob *job = &s_salvageRing.jobs[s_salvageRing.head % 20];
			int i;
			job->flags |= 2U;
			job->surfaceId = surfaceId;
			for (i = 0; i < 6; i++)
				job->rtWords[i] = rtWords[i];
			job->surfaceFlags = flags;
			for (i = 0; i < 4; i++)
				job->surfaceExtra[i] = extra[i];
		}
		sceKernelUnlockLwMutex(&s_salvageMutex, 1);
	}

	if (s_isCapturing) {
		uint32_t rtState[12];
		sgxRtStateInit(rtState, rtWords);
		if ((rtState[2] & ~3U) != 0U)
			razorCaptureFileWriteColorSurface(&s_statePtr->file, surfaceId, rtWords, flags, extra);
	}
	return 0;
}

int razorCaptureSyncCb(void)
{
	if (s_salvageActive) {
		unsigned int timeout;
		do {
			timeout = 1000000U;
		} while (sceKernelLockLwMutex(&s_salvageMutex, 1, &timeout) == (int)0x80028005);

		if (s_salvageRing.count > 0)
			s_salvageRing.jobs[s_salvageRing.head % 20].flags |= 1U;
		sceKernelUnlockLwMutex(&s_salvageMutex, 1);
	}

	if (s_isCapturing)
		razorCaptureFileWriteSync(&s_statePtr->file);
	return 0;
}

static inline __attribute__((always_inline)) void razorCaptureFinishSession(RazorCaptureState *st)
{
	uint64_t errs = st->file.errorFlags | st->errorFlags;

	if (errs != 0ULL) {
		sceClibPrintf("razor_capture: WARNING: Errors were detected during capture. Your file might not load in the Razor tool.\n");
		sceClibPrintf("razor_capture: WARNING: Error flags = (0x%llx)\n", errs);
	}
	s_isCapturing = 0;
	if (st->activeReq.fromHost)
		razorHostNotifyComplete(errs);
	st->activeReq.startFrame = 0;
	st->activeReq.endFrame = 0;
	sceClibMemset(st->activeReq.filename, 0, sizeof(st->activeReq.filename));
	st->activeReq.fromHost = 0;
}

int razorCaptureKickCb(const uint32_t *jobDesc)
{
	uint32_t queryInfo[19];

	if (s_statePtr == NULL) {
		sceClibPrintf("razor_capture: WARNING: Razor capture failed to initialise properly.\n");
		sceClibPrintf("razor_capture: WARNING: Please check that you have loaded the module before calling sceGxmInitialize()\n");
		return 2;
	}

	if (s_salvageActive) {
		const uint32_t *kickInfo = (const uint32_t *)(uintptr_t)jobDesc[5];
		unsigned int timeout;
		RazorSalvageJob *job;

		razorSalvageQueryJobInfo(kickInfo[188], queryInfo);
		do {
			timeout = 1000000U;
		} while (sceKernelLockLwMutex(&s_salvageMutex, 1, &timeout) == (int)0x80028005);

		if (s_salvageRing.count <= 19)
			s_salvageRing.count++;
		s_salvageRing.head = (s_salvageRing.head + 19) % 20;
		job = &s_salvageRing.jobs[s_salvageRing.head];
		job->flags = 0;
		job->frameNum = s_frameCounter;
		razorMemcpy(job->jobDesc, jobDesc, 88U);
		razorMemcpy(job->kickInfo, kickInfo, 756U);
		job->jobDesc[5] = (uint32_t)(uintptr_t)job->kickInfo;
		job->flags = 4U;
		razorMemcpy(job->queryInfo, queryInfo, 76U);
		sceKernelUnlockLwMutex(&s_salvageMutex, 1);
	}

	if (!s_isCapturing)
		return 0;

	if (s_statePtr->activeReq.fromHost && !razorHostPollTrigger(NULL, 0)) {
		sceClibPrintf("razor_capture: Host capture was cancelled.\n");
		razorCaptureFileClose(&s_statePtr->file);
		razorCaptureFinishSession(s_statePtr);
		return 2;
	}

	{
		RazorCaptureState *st = s_statePtr;
		const uint32_t *kickInfo = (const uint32_t *)(uintptr_t)jobDesc[5];
		uint32_t mStart, mEnd;
		int i;

		if (razorCaptureFileIsMappingsFull(&st->file)) {
			sceClibPrintf("razor_capture: WARNING: Too many registered libgxm memory areas for capture to track\n");
			sceClibPrintf("razor_capture: WARNING: This capture might not load into the Host Tool without errors.\n");
		}

		st->jobId = s_frameCounter;
		st->jobFlags = (uint8_t)jobDesc[2];
		st->vdmCtrlCpuAddr = razorCaptureFileGpuToCpu(&st->file, jobDesc[3]);
		st->jobWord3 = 0;
		st->jobWord4 = 0;
		st->pdsBaseGpuAddr = jobDesc[4];
		for (i = 0; i < 16; i++)
			st->useBaseGpuAddrs[i] = jobDesc[6 + i];

		razorProgramCacheReset(&st->progCache);
		razorAnalyseVdmStream(st, jobDesc);
		razorSalvageQueryJobInfo(kickInfo[188], queryInfo);
		razorCaptureWriteSceneQuery(st, queryInfo);

		if (jobDesc[2] != 0U) {
			uint32_t dsChunk[8];
			dsChunk[0] = kickInfo[23];
			dsChunk[1] = queryInfo[0];
			dsChunk[2] = queryInfo[1];
			dsChunk[3] = queryInfo[2];
			dsChunk[4] = kickInfo[19];
			dsChunk[5] = kickInfo[20];
			dsChunk[6] = kickInfo[17];
			dsChunk[7] = kickInfo[18];
			razorCaptureFileWriteDepthStencil(&st->file, dsChunk);
		}

		razorAnalyse3dPdsState(st, kickInfo);
		razorAnalyseWrite3dState(st, kickInfo);
		razorAnalyseSinglePds(st, kickInfo[27], 16U * kickInfo[28]);

		if (razorMarkerGetRange(jobDesc[0], &mStart, &mEnd))
			razorCaptureFileWriteMarkerRange(&st->file, mStart, mEnd);

		sceClibPrintf("\nrazor_capture: Analysis done.\n");
	}
	return 0;
}

int razorCaptureAfterKickCb(void)
{
	RazorCaptureState *st = s_statePtr;

	if (st->activeReq.captureBeforeKick || !s_isCapturing)
		return 0;

	sceClibPrintf(st->activeReq.fromHost
		      ? "razor_capture: Writing memory...\n"
		      : "razor_capture: Writing memory to file...\n");
	st->errorFlags |= razorCaptureFileFlushMemory(&st->file, st->activeReq.captureAllMem);
	sceClibPrintf("razor_capture: Writing memory complete.\n");
	return 0;
}

/* --- Begin / End frame callbacks --- */

void razorCaptureBeginFrameCb(void)
{
	RazorCaptureState *st;

	if (!s_triggerPending || s_isCapturing)
		return;
	if (s_frameCounter < s_pendingReq.startFrame || s_frameCounter > s_pendingReq.endFrame)
		return;

	st = s_statePtr;
	st->errorFlags = 0ULL;
	razorMemcpy(&st->activeReq, &s_pendingReq, sizeof(RazorTriggerReq));

	s_triggerPending = 0;
	s_pendingReq.startFrame = 0;
	s_pendingReq.endFrame = 0;
	sceClibMemset(s_pendingReq.filename, 0, sizeof(s_pendingReq.filename));
	s_pendingReq.fromHost = 0;

	if (st->activeReq.fromHost) {
		sceClibPrintf("razor_capture: Starting capture triggered from Host Tool\n");
		razorHostNotifyStart();
	} else {
		sceClibPrintf("razor_capture: Opening capture file '%s'\n", st->activeReq.filename);
	}

	st->errorFlags = razorCaptureFileOpen(&st->file, st->activeReq.filename, st->initParams);
	if (st->errorFlags != 0ULL) {
		st->errorFlags |= 0x40ULL;
		sceClibPrintf("razor_capture: failed to open capture file '%s'\n", st->activeReq.filename);
		razorCaptureFinishSession(st);
	} else {
		s_isCapturing = 1;
		razorCaptureWriteAllFrameInfo(&st->file);
	}
}

void razorCaptureEndFrameCb(void)
{
	char hostFile[512];

	if (s_isCapturing && s_frameCounter >= s_statePtr->activeReq.endFrame) {
		razorCaptureFileClose(&s_statePtr->file);
		sceClibPrintf("razor_capture: Capture completed\n");
		razorCaptureFinishSession(s_statePtr);
	}

	s_frameCounter++;

	if (razorHostPollTrigger(hostFile, sizeof(hostFile))) {
		uint32_t frame = s_frameCounter;
		if (s_triggerPending) {
			sceClibPrintf("WARNING: Razor GPU capture Host Tool trigger was set while another request was pending.\n");
			sceClibPrintf("WARNING: The first request (for frame %u) will not complete.\n", s_pendingReq.startFrame);
		}
		s_pendingReq.startFrame = 0;
		s_pendingReq.endFrame = 0;
		sceClibMemset(s_pendingReq.filename, 0, sizeof(s_pendingReq.filename));
		s_pendingReq.startFrame = frame;
		s_pendingReq.endFrame = frame;
		s_pendingReq.fromHost = 1;
		sceClibStrncpy(s_pendingReq.filename, hostFile, sizeof(s_pendingReq.filename) - 1U);
		s_pendingReq.filename[sizeof(s_pendingReq.filename) - 1U] = '\0';
		s_triggerPending = 1;
		sceClibPrintf("razor_capture: capture triggered from Host Tool\n");
	}
}

/* --- 0x81001830: GXM init callback --- */

int razorCaptureInitCb(void)
{
	sceClibMemset(&s_stateStorage, 0, sizeof(s_stateStorage));
	s_pendingReq.captureAllMem = 0;
	s_pendingReq.captureBeforeKick = 0;
	s_salvageRing.head = 19;
	s_salvageRing.count = 0;

	if (sceKernelCreateLwMutex(&s_salvageMutex, "sceRazorCapture", 0, 0, NULL) != 0)
		return 2;

	razorMemset(&s_stateStorage, 0, sizeof(s_stateStorage));
	razorCaptureFileInit(&s_stateStorage.file);
	sgxPdsVmInit(s_stateStorage.pdsVm0);
	sgxUsseVmInit(s_stateStorage.usseVm);
	sgxRegBankInit(&s_stateStorage.paBank);
	sgxPdsVmInit(s_stateStorage.pdsVm1);
	sgxPdsVmInit(s_stateStorage.pdsVm2);
	sgxPdsVmInit(s_stateStorage.pdsVm3);
	sgxRegBankInit(&s_stateStorage.saBank);
	sgxRegBankInit(&s_stateStorage.fragBank);
	razorProgramCacheInit(&s_stateStorage.progCache);

	s_statePtr = &s_stateStorage;
	s_stateStorage.activeReq.captureAllMem = 0;
	s_stateStorage.activeReq.captureBeforeKick = 0;
	razorMarkerInit();

	if (s_salvageDeferred && !s_salvageActive) {
		s_salvageDeferred = 0;
		s_salvageActive = 0;
		if (razorSalvageStartThread(razorCaptureSalvageDump) == 0) {
			sceClibPrintf("razor_capture: Salvage mode started.\n");
			s_salvageActive = 1;
		}
	}
	return 0;
}

/* --- Public API implementations --- */

SceBool razorCaptureIsCapturingImpl(void)
{
	return (SceBool)s_isCapturing;
}

void razorCaptureSetTriggerImpl(SceUInt32 startFrame, SceUInt32 endFrame,
				const char *filename, const char *funcName, SceBool fromHost)
{
	uint32_t start = (startFrame < endFrame) ? startFrame : endFrame;

	if (s_triggerPending) {
		if (s_pendingReq.fromHost && !fromHost) {
			sceClibPrintf("WARNING: Razor GPU capture did not start, since a request from the Host Tool has already been made");
			return;
		}
		sceClibPrintf("WARNING: Razor GPU capture Host Tool trigger was set while another request was pending.\n");
		sceClibPrintf("WARNING: The first request (for frame %u) will not complete.\n", s_pendingReq.startFrame);
	}

	s_pendingReq.startFrame = 0;
	s_pendingReq.endFrame = 0;
	sceClibMemset(s_pendingReq.filename, 0, sizeof(s_pendingReq.filename));
	s_pendingReq.startFrame = start;
	s_pendingReq.endFrame = endFrame;
	s_pendingReq.fromHost = (uint8_t)fromHost;

	if (filename != NULL) {
		sceClibStrncpy(s_pendingReq.filename, filename, sizeof(s_pendingReq.filename) - 1U);
	} else {
		sceClibPrintf("WARNING: %s(): No capture filename specified, defaulting filename to host0:capture.sgx\n",
			      funcName);
		sceClibStrncpy(s_pendingReq.filename, "host0:capture.sgx", sizeof(s_pendingReq.filename) - 1U);
	}
	s_pendingReq.filename[sizeof(s_pendingReq.filename) - 1U] = '\0';
	s_triggerPending = 1;
}

void razorCaptureSetTriggerNextFrameImpl(const char *filename, const char *funcName, SceBool fromHost)
{
	uint32_t nextFrame = s_frameCounter + 1U;

	if (s_triggerPending) {
		if (s_pendingReq.fromHost && !fromHost) {
			sceClibPrintf("WARNING: Razor GPU capture did not start, since a request from the Host Tool has already been made");
			return;
		}
		sceClibPrintf("WARNING: Razor GPU capture Host Tool trigger was set while another request was pending.\n");
		sceClibPrintf("WARNING: The first request (for frame %u) will not complete.\n", s_pendingReq.startFrame);
	}

	s_pendingReq.startFrame = 0;
	s_pendingReq.endFrame = 0;
	sceClibMemset(s_pendingReq.filename, 0, sizeof(s_pendingReq.filename));
	s_pendingReq.startFrame = nextFrame;
	s_pendingReq.endFrame = nextFrame;
	s_pendingReq.fromHost = (uint8_t)fromHost;

	if (filename != NULL) {
		sceClibStrncpy(s_pendingReq.filename, filename, sizeof(s_pendingReq.filename) - 1U);
	} else {
		sceClibPrintf("WARNING: %s(): No capture filename specified, defaulting filename to host0:capture.sgx\n",
			      funcName);
		sceClibStrncpy(s_pendingReq.filename, "host0:capture.sgx", sizeof(s_pendingReq.filename) - 1U);
	}
	s_pendingReq.filename[sizeof(s_pendingReq.filename) - 1U] = '\0';
	s_triggerPending = 1;
}

void razorCaptureSetCaptureAllMemoryImpl(SceBool enable)
{
	s_pendingReq.captureAllMem = (uint8_t)enable;
}

void razorCaptureSetCaptureBeforeKickImpl(SceBool enable)
{
	s_pendingReq.captureBeforeKick = (uint8_t)enable;
}

void razorCaptureEnableSalvageImpl(const char *filename)
{
	if (filename == NULL) {
		sceClibPrintf("razor_capture: ERROR: Salvage mode output filename not set.\n");
		return;
	}

	if (s_statePtr != NULL) {
		s_salvageDeferred = 0;
		if (!s_salvageActive) {
			sceClibStrncpy(s_salvageFilename, filename, sizeof(s_salvageFilename) - 1U);
			s_salvageFilename[sizeof(s_salvageFilename) - 1U] = '\0';
			if (razorSalvageStartThread(razorCaptureSalvageDump) == 0) {
				sceClibPrintf("razor_capture: Salvage mode started.\n");
				s_salvageActive = 1;
			}
		}
	} else {
		sceClibStrncpy(s_salvageFilename, filename, sizeof(s_salvageFilename) - 1U);
		s_salvageFilename[sizeof(s_salvageFilename) - 1U] = '\0';
		s_salvageDeferred = 1;
	}
}

void razorCapturePreInit(void)
{
	s_pendingReq.startFrame = 0;
	s_pendingReq.endFrame = 0;
	sceClibMemset(s_pendingReq.filename, 0, sizeof(s_pendingReq.filename));
	s_pendingReq.fromHost = 0;
	s_pendingReq.captureAllMem = 0;
	s_triggerPending = 0;
	s_isCapturing = 0;
	s_frameCounter = 0;
	s_salvageActive = 0;
}

void razorCaptureGetInitCallbacks(SceUInt32 *magic, void *callbacks)
{
	void **cb = (void **)callbacks;

	cb[0] = (void *)razorCaptureInitCb;
	cb[1] = (void *)razorCaptureTermCb;
	cb[2] = (void *)razorCaptureSyncCb;
	cb[3] = (void *)razorCaptureBeginFrameCb;
	cb[4] = (void *)razorCaptureEndFrameCb;
	cb[5] = (void *)razorCaptureResetMarkerCb;
	cb[6] = (void *)razorCapturePushMarkerCb;
	cb[7] = (void *)razorCaptureMapMemCb;
	cb[8] = (void *)razorCaptureUnmapMemCb;
	cb[9] = (void *)razorCaptureIsCapturingCb;
	cb[10] = (void *)razorCaptureIsSalvageCb;
	cb[11] = (void *)razorCaptureBeforeKickCb;
	cb[12] = (void *)razorCaptureAfterKickCb;
	cb[13] = (void *)razorCaptureColorSurfaceCb;
	cb[14] = (void *)razorCaptureKickCb;
	cb[15] = (void *)razorCaptureRegisterRangeCb;
	cb[16] = (void *)razorCaptureUnregisterRangeCb;
	cb[17] = (void *)razorCaptureSetInitParamsCb;
	*magic = 0x10005U;
}
