/*
 * Performance Analyzer (PA) driver interface, .pmon writer, and live PA packet decoder.
 */
#include "razor_hud_internal.h"

typedef struct HudPaHeader {
	uint32_t magic;            /* 0: 'PMON' = 0x4E4F4D50 (1313819984) */
	uint32_t headerSize;       /* 4: 224 */
	uint32_t bufSize;          /* 8 */
	uint32_t version;          /* 12: 0x10003 */
	uint32_t status;           /* 16 */
	uint32_t writePtr;         /* 20 */
	uint32_t traceFlags;       /* 24 */
	uint32_t bufParams[4];     /* 28: [0]=1, [1]=addr, [2]=size, [3]=0 */
	uint32_t traceMode;        /* 44: 1/2/3 */
	uint16_t sampleRate0;      /* 48 */
	uint16_t sampleRate1;      /* 50 */
	uint32_t maskHi[4];        /* 52..67 */
	uint32_t maskLo[4];        /* 68..83 */
	uint32_t counterReserved[8]; /* 84..115 */
	uint32_t counterCfg[16];   /* 116..179 */
	uint32_t reservedEnd[11];  /* 180..223 */
} HudPaHeader;

typedef struct HudPaManager {
	void *hudMgr;              /* 0 */
	HudPaHeader *hdr;          /* 4 */
	uint32_t fileSeq;          /* 8 */
	char filenameBase[1024];   /* 12..1035 */
	uint32_t countLen;         /* 1036 */
	int32_t paKey;             /* 1040 */
	uint32_t traceBufAddr;     /* 1044 */
	void *poolBuf;             /* 1048 */
	uint32_t poolBufSize;      /* 1052 */
	uint8_t running;           /* 1056 */
	uint8_t configured;        /* 1057 */
	uint8_t perfLoaded;        /* 1058 */
	uint8_t paTailPad;
} HudPaManager;

typedef struct HudPaJobSlot {
	SceRazorGpuLiveEntryJob entry; /* 0..71 */
	uint32_t accumCycles;          /* 72 */
	uint32_t accumFragCycles;      /* 76 */
	uint32_t slotPad[6];             /* 80..103 */
	uint32_t currentPid;           /* 104 */
	uint32_t prevFlags;            /* 108 */
	uint32_t prevFrame;            /* 112 */
	uint32_t bookmarkFlags;        /* 116 */
	uint32_t pendingMask;          /* 120 */
	uint32_t corePending[4];       /* 124..139 */
	uint16_t sceneIndex;           /* 140 */
	uint8_t slotTailPad[2];
} HudPaJobSlot;

typedef struct HudPaDrainCtx {
	uint8_t *base;
	uint8_t *cur;
	uint32_t capacity;
	uint32_t written;
	uint32_t *entryCount;
	uint32_t *overflowCount;
} HudPaDrainCtx;

typedef struct HudPaDecoder {
	uint8_t headerPad[128];
	uint64_t counterTs;            /* 128 */
	uint32_t counterOverflow;      /* 136 */
	uint16_t counterSamples[256];  /* 140..651 */
	uint16_t counterSampleIdx;     /* 652 */
	uint16_t samplePad;
	uint64_t pbTs;                 /* 656 */
	uint32_t pbOverflow;           /* 664 */
	uint32_t pbWords[8];           /* 668..699 */
	uint16_t pbWordIdx;            /* 700 */
	uint16_t pbPad;
	HudPaManager *paMgr;           /* 704 */
	uint32_t numCounters;          /* 708 */
	uint64_t firstTs;              /* 712 */
	uint64_t lastTs;               /* 720 */
	int32_t lastSeq;               /* 728 */
	uint32_t metricsGroup;         /* 732 */
	uint32_t readPtr;              /* 736 */
	void *stagingBuf;              /* 740 */
	uint32_t tbFreq;               /* 744 */
	uint32_t gpuFreq;              /* 748 */
	uint64_t frameStartUs;         /* 752 */
	int64_t gpuActiveUs;           /* 760 */
	uint32_t activeJobs;           /* 768 */
	uint32_t lastFrameNum;         /* 772 */
	HudPaJobSlot jobs[12];         /* 776..2503 */
	uint32_t samplePeriod;         /* 2504 */
	uint32_t sampleCycles;         /* 2508 */
	uint32_t corePendingCount[4];  /* 2512..2527 */
	uint8_t counterSync;           /* 2528 */
	uint8_t pbSync;                /* 2529 */
	uint8_t useStaging;            /* 2530 */
	uint8_t syncPad[5];
	SceRazorGpuLiveEntryParameterBuffer pbEntry; /* 2536..2559 */
} HudPaDecoder;

static inline __attribute__((always_inline)) uint32_t bswap32_u(uint32_t v)
{
	return ((v & 0xFFU) << 24) | ((v & 0xFF00U) << 8) | ((v >> 8) & 0xFF00U) | ((v >> 24) & 0xFFU);
}

static inline __attribute__((always_inline)) int writeIffChunk(int fd, uint32_t tag, const void *data, uint32_t dataSize)
{
	uint32_t hdr[2];
	hdr[0] = (tag & 0x00FFFFFFU) | 0x80000000U;
	hdr[1] = dataSize + 8U;
	if (sceIoWrite(fd, hdr, 8U) != 8) {
		sceClibPrintf("[SceRazorHud]: Failed to write IFF header (tag: 0x%08x)\n", tag);
		return -1;
	}
	if (dataSize != 0 && sceIoWrite(fd, data, dataSize) != (int)dataSize) {
		sceClibPrintf("[SceRazorHud]: Failed to write IFF leaf chunk (tag: 0x%08x)\n", tag);
		return -1;
	}
	return 0;
}

uint32_t hudGpuGetClockFreq(void)
{
	void *ctx = sceGxmGetTopContextInternal();
	uint32_t req[19];

	req[0] = 0;
	if (SceGpuEs4User_09BB98B2(ctx, req) != 0)
		return 1U;
	if (req[2] == 166000000U)
		return 166500000U;
	return req[2];
}

int hudGpuSetPerfCounters(const void *counters)
{
	void *ctx = sceGxmGetTopContextInternal();
	uint32_t req[19];

	req[0] = 3U;
	req[2] = 3U;
	sceClibMemcpy_safe(&req[3], counters, 0x20U);
	const uint32_t *counterWords = (const uint32_t *)counters;
	sceClibMemcpy_safe(&req[11], &counterWords[8], 0x20U);
	if (SceGpuEs4User_09BB98B2(ctx, req) != 0)
		return HUD_ERR_INTERNAL;
	return 0;
}

int hudGpuSetTraceEnable(int enable)
{
	void *ctx = sceGxmGetTopContextInternal();
	uint32_t req[19];

	req[0] = 13U;
	req[2] = (enable != 0) ? 3U : 0U;
	return SceGpuEs4User_09BB98B2(ctx, req);
}

void *hudPaDtor(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (pa->poolBuf != NULL)
		hudPoolFree(0U, pa->poolBuf);
	pa->poolBuf = NULL;
	hudDelete(pa->hdr);
	return pa;
}

int hudPaSetup(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	uint32_t bufParams[4];
	int ret;

	if (pa->perfLoaded == 0) {
		ret = sceSysmoduleLoadModule(SCE_SYSMODULE_PERF);
		if (ret != 0) {
			sceClibPrintf("[SceRazorHud]: Failed to load sys module (0x%08x)\n", ret);
			return ret;
		}
		pa->perfLoaded = 1;
	}
	if (pa->paKey == 0) {
		ret = sceKernelPaRegisterShared(HUD_PA_KEY_MAGIC_1, HUD_PA_KEY_MAGIC_2);
		if (ret < 0)
			return ret;
		pa->paKey = ret;
	}
	if (pa->perfLoaded == 0 || (sceKernelPaGetTraceBufferStatus() & 1U) != 0) {
		sceKernelPaStopByKey(pa->paKey);
		if (pa->perfLoaded == 0)
			return HUD_ERR_INTERNAL;
	}
	if (pa->perfLoaded == 0 || (sceKernelPaGetTraceBufferStatus() & 1U) != 0)
		return HUD_ERR_INTERNAL;

	sceClibMemset(bufParams, 0, sizeof(bufParams));
	bufParams[0] = pa->hdr->bufParams[0];
	bufParams[1] = pa->hdr->bufParams[1];
	bufParams[2] = pa->hdr->bufParams[2];
	bufParams[3] = pa->hdr->bufParams[3];
	ret = ScePerfInternal_26A6DF2C(pa->paKey, bufParams);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to initialize GPU Trace buffer (0x%08x)\n", ret);
		return ret;
	}

	if ((pa->hdr->traceFlags & 1U) != 0) {
		uint8_t traceParams[72];
		sceClibMemcpy_safe(traceParams, &pa->hdr->traceMode, 72U);
		ret = ScePerfInternal_37D9191E(pa->paKey, traceParams);
		if (ret != 0) {
			sceClibPrintf("[SceRazorHud]: Failed to add GPU Trace (0x%08x)\n", ret);
			return ret;
		}
	} else {
		ret = sceKernelPaRemoveGpuTraceByKey(pa->paKey);
		if (ret != 0) {
			sceClibPrintf("[SceRazorHud]: Failed to remove GPU Trace (0x%08x)\n", ret);
			return ret;
		}
	}

	ret = sceKernelPaSetBookmarkChannelEnableByKey(pa->paKey, 0U, 0xFFFFU);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to enable GPU Trace user bookmark channels (0x%08x)\n", ret);
		return ret;
	}
	ret = sceKernelPaSetBookmarkChannelEnableByKey(pa->paKey, 1U, 0xFFFFU);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to enable GPU Trace kernel bookmark channels (0x%08x)\n", ret);
		return ret;
	}
	pa->running = 0;
	pa->configured = 1;
	return 0;
}

int hudPaStart(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	int ret;

	if (pa->configured == 0 || pa->perfLoaded == 0 || (sceKernelPaGetTraceBufferStatus() & 1U) != 0)
		return HUD_ERR_INTERNAL;
	ret = sceKernelPaStartByKey(pa->paKey);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to start GPU Trace (0x%08x)\n", ret);
		return ret;
	}
	pa->running = 1;
	g_paIoBaseAddress = sceKernelPaGetIoBaseAddress();
	hudGpuSetTraceEnable(1);
	return 0;
}

int hudPaInsertBookmark(void *paPtr, uint32_t data)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (pa->running == 0)
		return 0;
	{
		int ret = sceKernelPaInsertBookmark(0U, 8U, data);
		if (ret != 0) {
			sceClibPrintf("[SceRazorHud]: Failed to insert GPU Trace user bookmark (0x%08x)\n", ret);
			return ret;
		}
	}
	return 0;
}

int hudPaStop(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (pa->running == 0)
		return 0;
	{
		int ret = sceKernelPaStopByKey(pa->paKey);
		if (ret != 0) {
			sceClibPrintf("[SceRazorHud]: Failed to stop GPU Trace (0x%08x)\n", ret);
			return ret;
		}
		pa->running = 0;
		g_paIoBaseAddress = 0;
		hudGpuSetTraceEnable(0);
	}
	return 0;
}

int hudPaWritePmonFile(void *paPtr, uint32_t gpuFreq, const char *path)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	uint32_t status;
	int fd, written, ret;
	uint32_t hdr2[4];
	uint32_t traceSize;
	void *topCtx;
	uint32_t gpuInfo[9];
	uint32_t verChunk[3];
	SceDateTime dt;
	uint16_t dtChunk[6];
	uint32_t zeroChunk = 0;
	struct {
		int32_t pid;
		char name[32];
		uint32_t hwRev;
		uint32_t gxmBuild;
		uint32_t gxmVerMajor;
		uint32_t gxmVerMinor;
	} procChunk;
	struct {
		uint32_t ids[8];
		char names[8][32];
	} sceneRaw, sceneOut;
	int i;

	pa->hdr->status = sceKernelPaGetTraceBufferStatus();
	pa->hdr->writePtr = sceKernelPaGetWritePointer();
	status = pa->hdr->status;
	if ((status & 2U) == 0) {
		sceClibPrintf("[SceRazorHud]: An error occured during the GPU Trace (0x%08x)\n", status);
		return 0;
	}
	if ((status & 4U) != 0)
		sceClibPrintf("[SceRazorHud]: Packet overflow error during GPU Trace, please use a lower sampling frequency\n");

	fd = sceIoOpen(path, 0x0602, 0x180);
	if (fd < 0) {
		sceClibPrintf("[SceRazorHud]: Failed to open file \"%s\" (0x%08x)\n", path, fd);
		return -1;
	}

	written = sceIoWrite(fd, pa->hdr, sizeof(HudPaHeader));
	if (written != (int)sizeof(HudPaHeader)) {
		sceClibPrintf("[SceRazorHud]: Failed to write to GPU Trace .pmon file (0x%08x)\n", written);
		return -1;
	}

	hdr2[0] = 0x4E4F4D51U; /* 'QMON' = 1313819985 */
	hdr2[1] = 16U;
	hdr2[2] = FW_VERSION | 0x11U;
	hdr2[3] = gpuFreq;
	written = sceIoWrite(fd, hdr2, sizeof(hdr2));
	if (written != (int)sizeof(hdr2)) {
		sceClibPrintf("[SceRazorHud]: Failed to write to GPU Trace .pmon file (0x%08x)\n", written);
		return -1;
	}

	traceSize = sceKernelPaGetTraceBufferSize(0U);
	sceClibPrintf("[SceRazorHud]: Writing GPU Trace \"%s\", size: %s MB.\n",
		path, hudFormatInt64Commas((int64_t)(traceSize >> 20)));

	if (traceSize != 0) {
		int total = 0;
		if ((pa->hdr->status & 8U) != 0) {
			uint32_t firstPart = pa->hdr->writePtr - pa->hdr->bufParams[1];
			uint32_t tailRem = traceSize - firstPart;
			const uint8_t *p = (const uint8_t *)(uintptr_t)(pa->traceBufAddr + firstPart);
			while (tailRem != 0) {
				uint32_t chunk;
				int w;
				if (hudHostToolPollCancel() != 0) {
					sceClibPrintf("[SceRazorHud]: GPU Trace cancelled by host\n");
					break;
				}
				sceClibPrintf(".");
				chunk = (tailRem >= 0x100000U) ? 0x100000U : tailRem;
				w = sceIoWrite(fd, p, chunk);
				if (w < 0) {
					total = -1;
					sceClibPrintf("[SceRazorHud]: Failed to write to GPU Trace .pmon file (0x%08x)\n", w);
					break;
				}
				tailRem -= (uint32_t)w;
				total += w;
				p += w;
			}
			p = (const uint8_t *)(uintptr_t)pa->traceBufAddr;
			while (firstPart != 0) {
				uint32_t chunk;
				int w;
				if (hudHostToolPollCancel() != 0) {
					sceClibPrintf("[SceRazorHud]: GPU Trace cancelled by host\n");
					break;
				}
				sceClibPrintf(".");
				chunk = (firstPart >= 0x100000U) ? 0x100000U : firstPart;
				w = sceIoWrite(fd, p, chunk);
				if (w < 0) {
					total--;
					sceClibPrintf("[SceRazorHud]: Failed to write to GPU Trace .pmon file (0x%08x)\n", w);
					break;
				}
				firstPart -= (uint32_t)w;
				total += w;
				p += w;
			}
		} else {
			uint32_t rem = traceSize;
			const uint8_t *p = (const uint8_t *)(uintptr_t)pa->traceBufAddr;
			while (rem != 0) {
				uint32_t chunk;
				int w;
				if (hudHostToolPollCancel() != 0) {
					sceClibPrintf("[SceRazorHud]: GPU Trace cancelled by host\n");
					break;
				}
				sceClibPrintf(".");
				chunk = (rem >= 0x100000U) ? 0x100000U : rem;
				w = sceIoWrite(fd, p, chunk);
				if (w < 0) {
					total = -1;
					sceClibPrintf("[SceRazorHud]: Failed to write to GPU Trace .pmon file (0x%08x)\n", w);
					break;
				}
				rem -= (uint32_t)w;
				total += w;
				p += w;
			}
		}
		sceClibPrintf("\n");
		if ((uint32_t)total != traceSize)
			sceClibPrintf("[SceRazorHud]: Error during GPU Trace data write, GPU Trace .pmon file is incomplete\n");
	} else {
		sceClibPrintf("\n");
	}

	writeIffChunk(fd, 0x1C2B00U, NULL, 0U);

	topCtx = sceGxmGetTopContextInternal();
	SceGpuEs4User_768482F6(topCtx, gpuInfo);
	writeIffChunk(fd, 0x1C2B02U, &gpuInfo[0], 4U);
	writeIffChunk(fd, 0x1C2B03U, &gpuInfo[1], 32U);

	verChunk[0] = 5029U;
	verChunk[1] = 1U;
	verChunk[2] = 0x00110007U;
	writeIffChunk(fd, 0x1C2B04U, verChunk, 12U);

	sceClibMemset(&dt, 0, sizeof(dt));
	sceRtcGetCurrentClockLocalTime(&dt);
	dtChunk[0] = dt.year;
	dtChunk[1] = dt.month;
	dtChunk[2] = dt.day;
	dtChunk[3] = dt.hour;
	dtChunk[4] = dt.minute;
	dtChunk[5] = dt.second;
	writeIffChunk(fd, 0x1C2B05U, dtChunk, 12U);

	writeIffChunk(fd, 0x1C2B06U, &zeroChunk, 4U);

	procChunk.pid = sceKernelGetProcessId();
	sceKernelGetProcessName(procChunk.name, sizeof(procChunk.name));
	procChunk.hwRev = SceLibKernel_9F793F84();
	procChunk.gxmBuild = (sceGxmIsDebugVersion() != 0) ? 2U : 1U;
	hudMgrGetGxmVersions(pa->hudMgr, &procChunk.gxmVerMajor, &procChunk.gxmVerMinor);
	writeIffChunk(fd, 0x1C2B07U, &procChunk, sizeof(procChunk));

	SceGpuEs4User_C0D16CE5(topCtx, &sceneRaw);
	sceClibMemset(&sceneOut, 0, sizeof(sceneOut));
	for (i = 0; i < 8; i++) {
		sceneOut.ids[i] = sceneRaw.ids[i];
		sceClibStrncpy(sceneOut.names[i], sceneRaw.names[i], 31U);
	}
	writeIffChunk(fd, 0x1C2B08U, &sceneOut, sizeof(sceneOut));

	writeIffChunk(fd, 0x1C2B01U, NULL, 0U);
	sceClibPrintf("[SceRazorHud]: GPU Trace write complete\n");

	ret = sceIoClose(fd);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to close GPU Trace .pmon file (0x%08x)\n", ret);
		return ret;
	}
	pa->fileSeq++;
	return 0;
}

int hudPaWriteNextPmon(void *paPtr, uint32_t gpuFreq)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	char *path = (char *)hudNewArray(256U);
	char *ext;

	sceClibSnprintf(path, 256U, "%s", pa->filenameBase);
	ext = sceClibStrstr(path, ".pmon");
	if (ext != NULL)
		*ext = '\0';
	if (pa->countLen != 0)
		sceClibSnprintf(path, 256U, "%s%0*d.pmon", path, pa->countLen, pa->fileSeq);
	else
		sceClibSnprintf(path, 256U, "%s.pmon", path);

	hudPaWritePmonFile(pa, gpuFreq, path);
	if (path != NULL)
		hudDeleteArray(path);
	return 0;
}

int hudPaTeardown(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	int ret;

	pa->running = 0;
	sceKernelPaStopByKey(pa->paKey);
	if (pa->configured == 0)
		return 0;
	pa->configured = 0;
	ret = sceKernelPaRemoveCounterTraceByKey(pa->paKey);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to remove counter trace (0x%08x)\n", ret);
		return ret;
	}
	ret = sceKernelPaRemoveGpuTraceByKey(pa->paKey);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to remove GPU trace (0x%08x)\n", ret);
		return ret;
	}
	sceKernelPaUnregister(pa->paKey);
	pa->paKey = 0;
	if (pa->perfLoaded == 0)
		return 0;
	ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_PERF);
	if (ret != 0) {
		sceClibPrintf("[SceRazorHud]: Failed to unload sys module (0x%08x)\n", ret);
		return ret;
	}
	pa->perfLoaded = 0;
	return 0;
}

int hudPaIsSetup(void *paPtr)
{
	return ((HudPaManager *)paPtr)->configured;
}

int hudPaIsRunning(void *paPtr)
{
	return ((HudPaManager *)paPtr)->running;
}

char *hudPaSetFilename(void *paPtr, const char *name, uint32_t countLen)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (name != NULL && name[0] != '\0') {
		if (sceClibStrchr(name, ':') != NULL)
			pa->filenameBase[0] = '\0';
		else
			sceClibStrncpy(pa->filenameBase, "host0:", 1023U);
		pa->countLen = (countLen <= 16U) ? countLen : 16U;
		return sceClibStrncat(pa->filenameBase, name, 1023U);
	}
	if (sceClibStrchr("host0:gpu_pa_trace_", ':') != NULL)
		pa->filenameBase[0] = '\0';
	else
		sceClibStrncpy(pa->filenameBase, "host0:", 1023U);
	pa->countLen = 3U;
	return sceClibStrncat(pa->filenameBase, "host0:gpu_pa_trace_", 1023U);
}

void *hudPaCtor(void *paPtr, void *mgr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	HudPaHeader *hdr;
	int i;

	pa->hudMgr = mgr;
	pa->running = 0;
	pa->configured = 0;
	pa->perfLoaded = 0;
	pa->fileSeq = 0;
	hdr = (HudPaHeader *)hudNew(sizeof(HudPaHeader));
	pa->hdr = hdr;
	sceClibMemset(hdr, 0, sizeof(HudPaHeader));
	pa->poolBuf = NULL;
	pa->poolBufSize = 0;
	pa->traceBufAddr = 0x40000000U;
	if (sceClibStrchr("host0:gpu_pa_trace_", ':') != NULL)
		pa->filenameBase[0] = '\0';
	else
		sceClibStrncpy(pa->filenameBase, "host0:", 1023U);
	sceClibStrncat(pa->filenameBase, "host0:gpu_pa_trace_", 1023U);
	pa->countLen = 3U;

	hdr->magic = 0x4E4F4D50U;
	hdr->headerSize = sizeof(HudPaHeader);
	hdr->bufSize = 0x1000000U;
	hdr->version = 0x10003U;
	hdr->traceFlags = 1U;
	hdr->bufParams[0] = 1U;
	hdr->bufParams[1] = pa->traceBufAddr;
	hdr->bufParams[2] = 0x1000000U;
	hdr->bufParams[3] = 0;
	hdr->traceMode = 3U;
	hdr->sampleRate0 = 4096U;
	hdr->sampleRate1 = 4096U;
	for (i = 0; i < 4; i++) {
		hdr->maskHi[i] = 0xFFFFFFFFU;
		hdr->maskLo[i] = 0xFFFFFFFFU;
	}
	for (i = 0; i < 8; i++)
		hdr->counterReserved[i] = 0;
	pa->paKey = 0;
	return pa;
}

void hudPaSetConfig(void *paPtr, const uint32_t *counters, uint32_t maskLo, uint32_t maskHi, uint32_t rate)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	HudPaHeader *hdr = pa->hdr;
	uint32_t hiSwapped = bswap32_u(maskHi);
	uint32_t loSwapped = bswap32_u(maskLo);
	uint16_t clampedRate;
	int i;

	for (i = 0; i < 4; i++) {
		hdr->maskHi[i] = hiSwapped;
		hdr->maskLo[i] = loSwapped;
	}
	if (rate <= 31U)
		clampedRate = 32U;
	else if (rate >= 0x10000U)
		clampedRate = 0xFFFFU;
	else
		clampedRate = (uint16_t)rate;
	hdr->sampleRate0 = clampedRate;
	hdr->sampleRate1 = clampedRate;

	for (i = 0; i < 4; i++) {
		hdr->counterCfg[i]      = counters[0] | (counters[1] << 16) | (counters[8] << 8)  | (counters[9] << 24);
		hdr->counterCfg[i + 4]  = counters[2] | (counters[3] << 16) | (counters[10] << 8) | (counters[11] << 24);
		hdr->counterCfg[i + 8]  = counters[4] | (counters[5] << 16) | (counters[12] << 8) | (counters[13] << 24);
		hdr->counterCfg[i + 12] = counters[6] | (counters[7] << 16) | (counters[14] << 8) | (counters[15] << 24);
	}
}

uint64_t hudPaGetMask(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	return ((uint64_t)pa->hdr->maskHi[0] << 32) | (uint64_t)pa->hdr->maskLo[0];
}

void hudPaSetBuffer(void *paPtr, int usePool, uint32_t size)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	uint32_t aligned = size & ~0x7FU;

	if (aligned < 0x100000U)
		aligned = 0x100000U;
	else if (aligned > 0x20000000U)
		aligned = 0x20000000U;

	if (usePool == 1) {
		if (pa->poolBuf == NULL || pa->poolBufSize != aligned) {
			if (pa->poolBuf != NULL)
				hudPoolFree(0U, pa->poolBuf);
			pa->poolBufSize = aligned;
			pa->poolBuf = hudPoolAlloc(0U, aligned, 128U);
			pa->traceBufAddr = (uint32_t)(uintptr_t)pa->poolBuf;
		}
	} else {
		pa->traceBufAddr = 0x40000000U;
		if (pa->poolBuf != NULL)
			hudPoolFree(0U, pa->poolBuf);
		pa->poolBufSize = 0;
	}
	pa->hdr->bufSize = aligned;
	pa->hdr->bufParams[2] = aligned;
	pa->hdr->bufParams[1] = pa->traceBufAddr;
}

int hudPaSetTraceFlags(void *paPtr, int mode, int enableCounter, int enableGpu)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	(void)mode;
	if (!enableCounter && !enableGpu)
		return HUD_ERR_INVALID_PARAM;
	if (enableCounter && enableGpu)
		pa->hdr->traceMode = 3U;
	else if (enableCounter)
		pa->hdr->traceMode = 2U;
	else
		pa->hdr->traceMode = 1U;
	return 0;
}

void hudPaGetBufferInfo(void *paPtr, uint32_t *outInfo)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (pa->hdr != NULL) {
		outInfo[0] = pa->hdr->bufParams[1];
		outInfo[1] = pa->hdr->bufParams[2];
	} else {
		outInfo[0] = 0;
		outInfo[1] = 0;
	}
}

int hudPaGetTraceUsed(void *paPtr)
{
	HudPaManager *pa = (HudPaManager *)paPtr;
	if (pa->running == 0)
		return HUD_ERR_INTERNAL;
	if ((sceKernelPaGetTraceBufferStatus() & 4U) != 0)
		return HUD_ERR_OVERFLOW;
	return (int)sceKernelPaGetTraceBufferSize(0U);
}

void *hudPaDecoderCtor(void *decPtr)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;

	dec->lastSeq = -1;
	dec->paMgr = NULL;
	dec->counterSync = 0;
	dec->pbSync = 0;
	dec->numCounters = 0;
	dec->metricsGroup = 0;
	dec->readPtr = 0;
	dec->firstTs = 0;
	dec->lastTs = 0;
	dec->tbFreq = 1U;
	dec->gpuFreq = 1U;
	dec->useStaging = 1;
	dec->stagingBuf = hudPoolAlloc(0U, 4096U, 4U);
	return dec;
}

void hudPaDecoderInit(void *decPtr, void *paPtr, uint32_t group, uint32_t gpuFreq, uint32_t tbFreq, uint32_t samplePeriod)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	int i;

	dec->paMgr = (HudPaManager *)paPtr;
	dec->readPtr = 0;
	dec->counterSync = 0;
	dec->pbSync = 0;
	if (paPtr != NULL) {
		uint64_t m = hudPaGetMask(paPtr);
		uint32_t cnt = 0;
		while (m != 0) {
			cnt++;
			m &= m - 1ULL;
		}
		dec->numCounters = cnt * 4U;
	} else {
		dec->numCounters = (uint32_t)HUD_ERR_NOT_INITIALIZED;
	}

	dec->tbFreq = tbFreq;
	dec->firstTs = 0;
	dec->lastTs = 0;
	dec->metricsGroup = group;
	dec->samplePeriod = samplePeriod;
	dec->lastSeq = -1;
	sceClibMemset(dec->corePendingCount, 0, sizeof(dec->corePendingCount));
	if (gpuFreq == 166000000U)
		gpuFreq = 166500000U;
	dec->gpuFreq = gpuFreq;
	dec->sampleCycles = (uint32_t)(((uint64_t)gpuFreq * (uint64_t)samplePeriod) / (uint64_t)(1000000U * tbFreq));
	dec->activeJobs = 0;
	dec->gpuActiveUs = 0;
	dec->frameStartUs = 0;
	dec->lastFrameNum = 0xFFFFFFFFU;

	for (i = 0; i < 12; i++) {
		HudPaJobSlot *slot = &dec->jobs[i];
		sceClibMemset(&slot->entry, 0, sizeof(slot->entry));
		slot->entry.startTime = ~(uint64_t)0;
		slot->entry.endTime = ~(uint64_t)0;
		slot->entry.frameNumber = 0xFFFFFFFFU;
		slot->sceneIndex = 0;
		slot->prevFrame = 0xFFFFFFFFU;
		slot->bookmarkFlags = 0;
		slot->pendingMask = 0;
		slot->currentPid = 0xFFFFFFFFU;
		slot->prevFlags = 0;
		sceClibMemset(slot->corePending, 0, sizeof(slot->corePending));
	}
}

void hudPaDecoderSetGpuFreq(void *decPtr, uint32_t gpuFreq)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	if (gpuFreq == 166000000U)
		gpuFreq = 166500000U;
	dec->gpuFreq = gpuFreq;
	dec->sampleCycles = (uint32_t)(((uint64_t)gpuFreq * (uint64_t)dec->samplePeriod) / (uint64_t)(1000000U * dec->tbFreq));
}

void hudPaDecoderProcessSample(void *decPtr, void *jobPtr, uint64_t timestamp, uint8_t type, uint8_t core, uint16_t cycles)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	HudPaJobSlot *slot = (HudPaJobSlot *)jobPtr;

	if (slot->entry.startTime != ~(uint64_t)0 && slot->entry.endTime == ~(uint64_t)0) {
		if ((uint32_t)cycles < dec->sampleCycles) {
			uint64_t delta = ((uint64_t)(1000000U * dec->tbFreq) * (uint64_t)cycles) / (uint64_t)dec->gpuFreq;
			slot->entry.endTime = (timestamp + delta) / (uint64_t)dec->tbFreq;
		}
		return;
	}

	if (cycles != 0) {
		uint32_t rem = (dec->sampleCycles > cycles) ? (dec->sampleCycles - cycles) : 0U;
		uint64_t delta = ((uint64_t)rem * (uint64_t)(1000000U * dec->tbFreq)) / (uint64_t)dec->gpuFreq;
		slot->entry.startTime = (timestamp + delta) / (uint64_t)dec->tbFreq;
		slot->entry.type = type;
		slot->entry.core = (int8_t)core;
		sceClibMemset(&slot->entry.jobValues, 0, sizeof(slot->entry.jobValues));
		slot->accumCycles = 0;
		slot->accumFragCycles = 0;
		if (slot->entry.type != 0 && (slot->entry.type & 1U) == 0) {
			if (slot->corePending[core] != 0 && (slot->pendingMask & 0x2000U) != 0) {
				slot->corePending[core] = 0;
				dec->corePendingCount[core] = 0;
			}
		}
		if (dec->activeJobs == 0)
			dec->gpuActiveUs -= (int64_t)slot->entry.startTime;
		dec->activeJobs++;
	}
}

void hudPaDecoderEmitJob(void *decPtr, int isEnd, void *outCtxPtr, void *jobPtr)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	HudPaDrainCtx *ctx = (HudPaDrainCtx *)outCtxPtr;
	HudPaJobSlot *slot = (HudPaJobSlot *)jobPtr;
	uint8_t type = slot->entry.type;
	int core = slot->entry.core;
	int hasCorePending = 0;
	int incScene = 0;

	slot->entry.processId = slot->currentPid;

	if (core == 0 && (type == 2 || type == 4 || type == 6 || type == 8) &&
	    slot->entry.frameNumber != 0xFFFFFFFFU && slot->entry.frameNumber != dec->lastFrameNum) {
		uint64_t st = slot->entry.startTime;
		if (dec->activeJobs != 0)
			dec->gpuActiveUs += (int64_t)st;
		if ((uint32_t)((ctx->cur - ctx->base) + 32) < ctx->capacity) {
			SceRazorGpuLiveEntryFrame fr;
			fr.header.entrySize = 32;
			fr.header.entryType = 2;
			fr.header.reserved = 0;
			fr.startTime = dec->frameStartUs;
			fr.duration = (uint32_t)(st - dec->frameStartUs);
			fr.frameNumber = dec->lastFrameNum;
			fr.gpuActiveDuration = (uint32_t)dec->gpuActiveUs;
			fr.reserved = 0;
			sceClibMemcpy_safe(ctx->cur, &fr, sizeof(fr));
			ctx->cur += 32;
			ctx->written += 32;
			(*ctx->entryCount)++;
		} else if (ctx->overflowCount != NULL) {
			(*ctx->overflowCount)++;
		}
		dec->gpuActiveUs = (dec->activeJobs != 0) ? -(int64_t)st : 0;
		dec->frameStartUs = st;
		dec->lastFrameNum = slot->entry.frameNumber;
	}

	if (dec->activeJobs != 0) {
		dec->activeJobs--;
		if (dec->activeJobs == 0)
			dec->gpuActiveUs += (int64_t)slot->entry.endTime;
	}

	if (type != 0 && (type & 1U) == 0)
		hasCorePending = (slot->corePending[core] != 0);

	if (slot->entry.frameNumber != 0xFFFFFFFFU && slot->entry.frameNumber != slot->prevFrame) {
		slot->prevFrame = slot->entry.frameNumber;
		slot->sceneIndex = 0;
		if (hasCorePending)
			slot->corePending[core]--;
	} else {
		if (type >= 1 && type <= 8) {
			if ((type & 1U) == 0)
				incScene = !(slot->prevFlags & 1U);
			else
				incScene = (slot->bookmarkFlags & 1U) != 0;
		}
		if (hasCorePending) {
			if (dec->corePendingCount[core] != slot->corePending[core])
				incScene = 0;
			slot->corePending[core]--;
			slot->entry.frameNumber = slot->prevFrame;
		}
		if (incScene)
			slot->sceneIndex++;
	}

	slot->entry.sceneIndex = slot->sceneIndex;
	slot->prevFlags = slot->entry.flags;

	if (isEnd != 0) {
		if ((uint32_t)(ctx->cur - ctx->base) - 1U < ctx->capacity) {
			*(uint16_t *)(ctx->cur) = 0xFFFFU;
			uint16_t *curHalf = (uint16_t *)ctx->cur;
			curHalf[1] = (uint16_t)isEnd;
			ctx->cur += 72;
			ctx->written += 72;
			(*ctx->entryCount)++;
		} else if (ctx->overflowCount != NULL) {
			(*ctx->overflowCount)++;
		}
	} else {
		if ((uint32_t)((ctx->cur - ctx->base) + 72) < ctx->capacity) {
			slot->entry.header.entrySize = 72;
			slot->entry.header.entryType = 0;
			sceClibMemcpy_safe(ctx->cur, &slot->entry, 72U);
			ctx->cur += 72;
			ctx->written += 72;
			(*ctx->entryCount)++;
		} else if (ctx->overflowCount != NULL) {
			(*ctx->overflowCount)++;
		}
	}

	sceClibMemset(&slot->entry, 0, sizeof(slot->entry));
	slot->entry.frameNumber = 0xFFFFFFFFU;
	slot->entry.startTime = ~(uint64_t)0;
	slot->entry.endTime = ~(uint64_t)0;
}

int hudPaDecoderProcessCounters(void *decPtr, void *outCtx)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	uint32_t stride = dec->numCounters >> 2;
	int core;

	if (dec->counterOverflow != 0)
		return HUD_ERR_OVERFLOW;

	switch (dec->metricsGroup) {
	case 0: {
		for (core = 0; core < 4; core++) {
			const uint16_t *s = &dec->counterSamples[stride * (uint32_t)core];
			if (s[0] != 0) dec->pbEntry.partialRender |= 2;
			if (s[1] != 0) dec->pbEntry.vertexJobPaused |= 2;
			if (s[2] != 0) {
				dec->pbEntry.partialRender |= 1;
				dec->pbEntry.vertexJobPaused |= 1;
			}
			hudPaDecoderProcessSample(dec, &dec->jobs[core * 3 + 0], dec->counterTs, 7, (uint8_t)core, s[3]);
			if (dec->jobs[core * 3 + 0].entry.startTime != ~(uint64_t)0 &&
			    dec->jobs[core * 3 + 0].entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, &dec->jobs[core * 3 + 0]);

			hudPaDecoderProcessSample(dec, &dec->jobs[core * 3 + 1], dec->counterTs, 8, (uint8_t)core, s[4]);
			if (dec->jobs[core * 3 + 1].entry.startTime != ~(uint64_t)0 &&
			    dec->jobs[core * 3 + 1].entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, &dec->jobs[core * 3 + 1]);

			hudPaDecoderProcessSample(dec, &dec->jobs[core * 3 + 2], dec->counterTs, 0, (uint8_t)core, s[5]);
			if (dec->jobs[core * 3 + 2].entry.startTime != ~(uint64_t)0 &&
			    dec->jobs[core * 3 + 2].entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, &dec->jobs[core * 3 + 2]);
		}
		break;
	}
	case 1: {
		for (core = 0; core < 4; core++) {
			const uint16_t *s = &dec->counterSamples[stride * (uint32_t)core];
			HudPaJobSlot *vSlot = &dec->jobs[core * 3 + 0];
			HudPaJobSlot *fSlot = &dec->jobs[core * 3 + 1];
			HudPaJobSlot *fwSlot = &dec->jobs[core * 3 + 2];

			hudPaDecoderProcessSample(dec, vSlot, dec->counterTs, 1, (uint8_t)core, s[14]);
			if (vSlot->entry.startTime != ~(uint64_t)0) {
				if (vSlot->entry.endTime == ~(uint64_t)0) {
					vSlot->accumCycles += (uint32_t)s[10] + s[11] + s[12] + s[13];
				} else {
					uint64_t us = ((uint64_t)vSlot->accumCycles * 1000000ULL) / (uint64_t)dec->gpuFreq;
					float dur = hudU64ToFloat(vSlot->entry.endTime - vSlot->entry.startTime);
					vSlot->entry.jobValues.vertexValues1.usseVertexProcessing = (hudU64ToFloat(us) * 25.0f) / dur;
					hudPaDecoderEmitJob(dec, 0, outCtx, vSlot);
				}
			}

			hudPaDecoderProcessSample(dec, fSlot, dec->counterTs, 2, (uint8_t)core, s[15]);
			if (fSlot->entry.startTime != ~(uint64_t)0) {
				if (fSlot->entry.endTime == ~(uint64_t)0) {
					fSlot->accumFragCycles += (uint32_t)s[6] + s[7] + s[8] + s[9];
					fSlot->entry.jobValues.fragmentValues1.usseDependentTextureReadRequest +=
						(float)(s[5] + s[4] + s[1] + s[2]);
					fSlot->entry.jobValues.fragmentValues1.usseNonDependentTextureReadRequest +=
						(float)(s[3] + s[0]);
				} else {
					float dur = hudU64ToFloat(fSlot->entry.endTime - fSlot->entry.startTime);
					uint64_t texUA = ((uint64_t)fSlot->accumFragCycles * 1000000ULL) / (uint64_t)dec->gpuFreq;
					uint64_t texUB = ((uint64_t)(uint32_t)fSlot->entry.jobValues.fragmentValues1.usseDependentTextureReadRequest * 1000000ULL) / (uint64_t)dec->gpuFreq;
					uint64_t texUC = ((uint64_t)(uint32_t)fSlot->entry.jobValues.fragmentValues1.usseNonDependentTextureReadRequest * 1000000ULL) / (uint64_t)dec->gpuFreq;
					fSlot->entry.jobValues.fragmentValues1.usseFragmentProcessing = (hudU64ToFloat(texUA) * 25.0f) / dur;
					fSlot->entry.jobValues.fragmentValues1.usseDependentTextureReadRequest = (hudU64ToFloat(texUB) * 25.0f) / dur;
					fSlot->entry.jobValues.fragmentValues1.usseNonDependentTextureReadRequest = (hudU64ToFloat(texUC) * 50.0f) / dur;
					hudPaDecoderEmitJob(dec, 0, outCtx, fSlot);
				}
			}

			hudPaDecoderProcessSample(dec, fwSlot, dec->counterTs, 0, (uint8_t)core, s[16]);
			if (fwSlot->entry.startTime != ~(uint64_t)0 && fwSlot->entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, fwSlot);
		}
		break;
	}
	case 2: {
		for (core = 0; core < 4; core++) {
			const uint16_t *s = &dec->counterSamples[stride * (uint32_t)core];
			HudPaJobSlot *vSlot = &dec->jobs[core * 3 + 0];
			HudPaJobSlot *fSlot = &dec->jobs[core * 3 + 1];
			HudPaJobSlot *fwSlot = &dec->jobs[core * 3 + 2];

			hudPaDecoderProcessSample(dec, vSlot, dec->counterTs, 3, (uint8_t)core, s[19]);
			if (vSlot->entry.startTime != ~(uint64_t)0) {
				if (vSlot->entry.endTime == ~(uint64_t)0) {
					vSlot->entry.jobValues.vertexValues2.vdmPrimitivesInput += s[16];
					vSlot->entry.jobValues.vertexValues2.mtePrimitivesOutput += s[18];
					vSlot->entry.jobValues.vertexValues2.vdmVerticesInput += s[15];
					vSlot->entry.jobValues.vertexValues2.mteVerticesOutput += s[17];
				} else {
					hudPaDecoderEmitJob(dec, 0, outCtx, vSlot);
				}
			}

			hudPaDecoderProcessSample(dec, fSlot, dec->counterTs, 4, (uint8_t)core, s[20]);
			if (fSlot->entry.startTime != ~(uint64_t)0) {
				if (fSlot->entry.endTime == ~(uint64_t)0) {
					fSlot->entry.jobValues.fragmentValues2.rasterizedPixelsBeforeHsr +=
						(uint32_t)s[10] + 2U * s[11] + 4U * s[12] + 8U * s[13] + 16U * s[14];
					fSlot->entry.jobValues.fragmentValues2.rasterizedOutputPixels +=
						(uint32_t)s[5] + 2U * s[6] + 4U * s[7] + 8U * s[8] + 16U * s[9];
					fSlot->entry.jobValues.fragmentValues2.rasterizedOutputSamples +=
						(uint32_t)s[0] + 2U * s[1] + 4U * s[2] + 8U * s[3] + 16U * s[4];
				} else {
					hudPaDecoderEmitJob(dec, 0, outCtx, fSlot);
				}
			}

			hudPaDecoderProcessSample(dec, fwSlot, dec->counterTs, 0, (uint8_t)core, s[21]);
			if (fwSlot->entry.startTime != ~(uint64_t)0 && fwSlot->entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, fwSlot);
		}
		break;
	}
	case 3: {
		for (core = 0; core < 4; core++) {
			const uint16_t *s = &dec->counterSamples[stride * (uint32_t)core];
			HudPaJobSlot *vSlot = &dec->jobs[core * 3 + 0];
			HudPaJobSlot *fSlot = &dec->jobs[core * 3 + 1];
			HudPaJobSlot *fwSlot = &dec->jobs[core * 3 + 2];

			hudPaDecoderProcessSample(dec, vSlot, dec->counterTs, 5, (uint8_t)core, s[2]);
			if (vSlot->entry.startTime != ~(uint64_t)0) {
				if (vSlot->entry.endTime == ~(uint64_t)0)
					vSlot->entry.jobValues.vertexValues3.bifTaMemoryWrite += 16U * s[0];
				else
					hudPaDecoderEmitJob(dec, 0, outCtx, vSlot);
			}

			hudPaDecoderProcessSample(dec, fSlot, dec->counterTs, 6, (uint8_t)core, s[3]);
			if (fSlot->entry.startTime != ~(uint64_t)0) {
				if (fSlot->entry.endTime == ~(uint64_t)0)
					fSlot->entry.jobValues.fragmentValues3.bifIspParameterFetchMemoryRead += 16U * s[1];
				else
					hudPaDecoderEmitJob(dec, 0, outCtx, fSlot);
			}

			hudPaDecoderProcessSample(dec, fwSlot, dec->counterTs, 0, (uint8_t)core, s[4]);
			if (fwSlot->entry.startTime != ~(uint64_t)0 && fwSlot->entry.endTime != ~(uint64_t)0)
				hudPaDecoderEmitJob(dec, 0, outCtx, fwSlot);
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

int hudPaDecoderProcessBlock(void *decPtr, const uint16_t *block, void *outCtx)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	int pkt;

	dec->lastSeq = (int32_t)(uint16_t)(block[0] + 1U);
	for (pkt = 0; pkt < 7; pkt++) {
		uint16_t tagWord = block[pkt + 1];
		uint8_t pktType = (uint8_t)((tagWord >> 8) & 0x7FU);
		uint8_t overflow = (uint8_t)(tagWord >> 15);
		uint8_t seq = (uint8_t)(tagWord & 0xFFU);
		const uint16_t *payload = &block[8 + pkt * 8];
		const uint32_t *payload32 = (const uint32_t *)payload;

		if (pktType == 0x60) {
			if (overflow != 0) {
				dec->pbWordIdx = 0;
				dec->pbOverflow++;
			} else if (seq == 0) {
				dec->pbSync = 1;
				dec->pbTs = (uint64_t)payload32[0] | ((uint64_t)payload[2] << 32);
				dec->pbWords[0] = payload32[2];
				dec->pbWords[1] = payload32[3];
				dec->pbWordIdx = 2;
			} else if (dec->pbSync != 0) {
				int k;
				for (k = 0; k < 4 && dec->pbWordIdx < 8; k++)
					dec->pbWords[dec->pbWordIdx++] = payload32[k];
			}
			if (dec->pbTs != 0) {
				if (dec->firstTs == 0)
					dec->firstTs = dec->pbTs;
				dec->lastTs = dec->pbTs;
			}
			if (dec->pbWordIdx == 8) {
				if (dec->pbOverflow != 0)
					return HUD_ERR_OVERFLOW;
				if (dec->metricsGroup == 0) {
					uint32_t usage = (uint32_t)(dec->pbWords[0] & 0xFFFFU) << 12;
					if (usage > dec->pbEntry.peakUsage) {
						dec->pbEntry.peakUsage = usage;
						dec->pbEntry.peakUsageTime = dec->pbTs / (uint64_t)dec->tbFreq;
					}
				}
			}
		} else if (pktType == 0x61) {
			if (overflow != 0) {
				dec->counterSampleIdx = 0;
				dec->counterOverflow++;
			} else if (seq == 0) {
				int k;
				dec->counterSampleIdx = 0;
				dec->counterSync = 1;
				dec->counterTs = (uint64_t)payload32[0] | ((uint64_t)payload[2] << 32);
				for (k = 0; k < 5 && dec->counterSampleIdx < dec->numCounters; k++)
					dec->counterSamples[dec->counterSampleIdx++] = payload[3 + k];
			} else if (dec->counterSync != 0) {
				int k;
				for (k = 0; k < 8 && dec->counterSampleIdx < dec->numCounters; k++)
					dec->counterSamples[dec->counterSampleIdx++] = payload[k];
			}
			if (dec->counterTs != 0) {
				if (dec->firstTs == 0)
					dec->firstTs = dec->counterTs;
				dec->lastTs = dec->counterTs;
			}
			if (dec->counterSampleIdx == dec->numCounters) {
				int ret = hudPaDecoderProcessCounters(dec, outCtx);
				if (ret != 0)
					return ret;
			}
		} else if (pktType == 0x7F) {
			const uint8_t *payloadBytes = (const uint8_t *)payload;
			if ((payloadBytes[11] & 0x1FU) == 0x12U) {
				uint32_t val = payload32[3];
				uint32_t cmd = val & 0xF0000000U;
				int c;
				if (cmd == 0x20000000U) {
					uint32_t v = val & 0x0FFFFFFFU;
					for (c = 0; c < 4; c++)
						dec->jobs[c * 3 + 0].bookmarkFlags = v;
				} else if (cmd == 0x10000000U) {
					uint32_t sub = val & 0x0F000000U;
					uint32_t data = val & 0x00FFFFFFU;
					if (sub == 0) {
						for (c = 0; c < 4; c++)
							dec->jobs[c * 3 + 0].entry.frameNumber = data;
					} else if (sub == 0x01000000U) {
						for (c = 0; c < 4; c++)
							dec->jobs[c * 3 + 1].entry.frameNumber = data;
					} else if (sub == 0x02000000U) {
						for (c = 0; c < 4; c++) {
							dec->jobs[c * 3 + 1].entry.frameNumber = data;
							dec->jobs[c * 3 + 1].entry.flags |= 1U;
						}
					} else if (sub == 0x04000000U) {
						uint32_t cnt = val & 0xFFU;
						for (c = 0; c < 4; c++) {
							dec->jobs[c * 3 + 1].entry.frameNumber = data;
							dec->jobs[c * 3 + 1].corePending[c] = cnt;
							dec->corePendingCount[c] = cnt;
						}
					}
				} else if (cmd == 0x30000000U) {
					uint32_t target = val & 0x0C000000U;
					int isHi = (val & 0x02000000U) != 0;
					uint32_t part = val & 0xFFFFU;
					if (target == 0 || target == 0x04000000U) {
						int slotOff = (target == 0) ? 0 : 1;
						for (c = 0; c < 4; c++) {
							HudPaJobSlot *s = &dec->jobs[c * 3 + slotOff];
							if (isHi)
								s->currentPid = (s->currentPid & 0xFFFFU) | (part << 16);
							else
								s->currentPid = part;
						}
					}
				} else if (cmd == 0x50000000U) {
					uint32_t v = val & 0x0FFFFFFFU;
					for (c = 0; c < 4; c++)
						dec->jobs[c * 3 + 1].pendingMask = v;
				}
			}
		}
	}
	return 0;
}

int hudPaDecoderDrain(void *decPtr, const uint32_t *bufInfo, int flushFinal, uint32_t writePtr,
                      void *outBuf, uint32_t outSize, uint32_t *outOffset,
                      uint32_t *outEntries, uint32_t *outOverflows)
{
	HudPaDecoder *dec = (HudPaDecoder *)decPtr;
	HudPaDrainCtx ctx;
	uint32_t startOff, readPtr, firstLen, wrapLen;
	int ret = 0;

	if (dec->paMgr == NULL)
		return HUD_ERR_NOT_INITIALIZED;

	if (writePtr < bufInfo[0] || writePtr >= bufInfo[0] + bufInfo[1]) {
		hudPaStop(dec->paMgr);
		hudPaSetup(dec->paMgr);
		hudPaStart(dec->paMgr);
		dec->readPtr = 0;
		if (outOffset != NULL)
			*outOffset = 0;
		return HUD_ERR_INTERNAL;
	}

	startOff = (outOffset != NULL) ? *outOffset : 0U;
	ctx.base = (uint8_t *)outBuf;
	ctx.cur = (uint8_t *)outBuf + startOff;
	ctx.capacity = outSize;
	ctx.written = startOff;
	ctx.entryCount = outEntries;
	ctx.overflowCount = outOverflows;

	dec->counterOverflow = 0;
	dec->pbOverflow = 0;
	readPtr = (dec->readPtr != 0) ? dec->readPtr : bufInfo[0];
	dec->readPtr = writePtr;

	if (writePtr >= readPtr) {
		firstLen = writePtr - readPtr;
		wrapLen = 0;
	} else {
		firstLen = (bufInfo[0] + bufInfo[1]) - readPtr;
		wrapLen = writePtr - bufInfo[0];
	}

	if (outEntries != NULL && dec->gpuFreq != 1U && outSize != 0 && ctx.cur != NULL) {
		if (*outEntries == 0) {
			sceClibMemset(&dec->pbEntry, 0, sizeof(dec->pbEntry));
			dec->pbEntry.header.entrySize = 24;
			dec->pbEntry.header.entryType = 1;
		}
		if (dec->useStaging != 0) {
			const uint8_t *src = (const uint8_t *)(uintptr_t)readPtr;
			while (firstLen != 0) {
				uint32_t chunk = (firstLen >= 4096U) ? 4096U : firstLen;
				uint32_t off;
				sceClibMemcpy_safe(dec->stagingBuf, src, chunk);
				src += (chunk & ~0x7FU);
				for (off = 0; off < chunk; off += 128U) {
					ret = hudPaDecoderProcessBlock(dec, (const uint16_t *)((uint8_t *)dec->stagingBuf + off), &ctx);
					if (ret != 0)
						return ret;
				}
				firstLen -= chunk;
			}
			src = (const uint8_t *)(uintptr_t)bufInfo[0];
			while (wrapLen != 0) {
				uint32_t chunk = (wrapLen >= 4096U) ? 4096U : wrapLen;
				uint32_t off;
				sceClibMemcpy_safe(dec->stagingBuf, src, chunk);
				src += (chunk & ~0x7FU);
				for (off = 0; off < chunk; off += 128U) {
					ret = hudPaDecoderProcessBlock(dec, (const uint16_t *)((uint8_t *)dec->stagingBuf + off), &ctx);
					if (ret != 0)
						return ret;
				}
				wrapLen -= chunk;
			}
		} else {
			const uint8_t *src = (const uint8_t *)(uintptr_t)readPtr;
			while (firstLen != 0) {
				ret = hudPaDecoderProcessBlock(dec, (const uint16_t *)src, &ctx);
				if (ret != 0)
					return ret;
				src += 128;
				firstLen -= 128U;
			}
			src = (const uint8_t *)(uintptr_t)bufInfo[0];
			while (wrapLen != 0) {
				ret = hudPaDecoderProcessBlock(dec, (const uint16_t *)src, &ctx);
				if (ret != 0)
					return ret;
				src += 128;
				wrapLen -= 128U;
			}
		}
	}

	if (flushFinal != 0 && ctx.cur != NULL && ctx.entryCount != NULL && ctx.capacity != 0 && dec->metricsGroup == 0) {
		dec->pbEntry.partialRender = (dec->pbEntry.partialRender == 3) ? 1 : 0;
		dec->pbEntry.vertexJobPaused = (dec->pbEntry.vertexJobPaused == 3) ? 1 : 0;
		if ((uint32_t)((ctx.cur - ctx.base) + 24) < ctx.capacity) {
			sceClibMemcpy_safe(ctx.cur, &dec->pbEntry, 24U);
			ctx.written += 24;
			ctx.cur += 24;
			(*ctx.entryCount)++;
		} else if (ctx.overflowCount != NULL) {
			(*ctx.overflowCount)++;
		}
	}

	if (outOffset != NULL)
		*outOffset = ctx.written;
	return 0;
}
