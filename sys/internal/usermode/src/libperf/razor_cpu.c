/*
 * ScePerf: Razor CPU user markers, capture control, and fiber/ULT trace packets.
 */

#include "libperf_internal.h"

#define RAZOR_MAX_TRACKED_THREADS	10
#define RAZOR_MAX_MARKER_DEPTH		64
#define RAZOR_MAX_LABEL_LEN		467
#define RAZOR_MAX_LABEL_WORDS		117

#define RAZOR_CAPTURE_FLAG_ACTIVE	0x1U
#define RAZOR_CAPTURE_FLAG_USER_TRACE	0x2U

typedef struct {
	SceUInt32 version;
	void (*debugMarker)(void);
	SceUInt32 numHooks;
	void (*syncHook)(void);
	void (*stopCaptureHook)(void);
	void (*startCaptureHook)(void);
	SceUInt32 structSize;
	volatile SceUInt32 stopFrameCountdown;
	volatile SceUInt32 captureFlags;
	void (*stopAfterFramesHook)(void);
} RazorCpuDebugInfo;

typedef struct {
	SceUID threadId;
	SceUInt32 stackLevel;
	SceUInt64 hudMask;
} RazorCpuThreadSlot;

typedef union {
	struct {
		SceUInt8 eventTypes[SCE_PERF_ARM_PMON_PMCNT_NUM];
		SceUInt16 numCounters;
	} fields;
	SceUInt32 words[2];
} PmonEventInfo;

RazorCpuDebugInfo g_razorCpuDebugInfo = {
	5,
	_razorCpuDebugMarker,
	4,
	_razorCpuSyncHook,
	_razorCpuStopCaptureHook,
	_razorCpuStartCaptureHook,
	sizeof(RazorCpuDebugInfo),
	0,
	0,
	_razorCpuStopAfterFramesHook,
};

SceUInt32 *_pLibPerfCaptureFlagPtr = (SceUInt32 *)&g_razorCpuDebugInfo.captureFlags;

static SceKernelLwMutexWork g_userTraceLock;
volatile SceUInt32 *g_paIoBase;
static SceRazorCpuUserMarkerTracePacket *g_userMarkerBufs[2];
static PmonEventInfo g_pmonEventInfo;
static SceUInt32 g_userMarkerBufIdx;
static SceUInt32 g_userMarkerMaxPackets;
static SceUInt32 g_userMarkerCount;
static RazorCpuThreadSlot g_threadSlots[RAZOR_MAX_TRACKED_THREADS];
static SceInt32 g_razorCpuInitialized;
static SceUInt32 g_numTrackedThreads;

static inline SceUInt32 readPmccntr(void)
{
	SceUInt32 val;

	__asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
	return val;
}

static inline SceUInt32 readPmcntenset(void)
{
	SceUInt32 val;

	__asm__ volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(val));
	return val;
}

static inline void writePmcntenset(SceUInt32 val)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(val));
}

static inline void writePmcntenclr(SceUInt32 val)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 2" :: "r"(val));
}

static inline SceUInt32 readPmxevtyper(void)
{
	SceUInt32 val;

	__asm__ volatile("mrc p15, 0, %0, c9, c13, 1" : "=r"(val));
	return val;
}

static inline int lockUserTrace(void)
{
	int tries;
	int ret;

	for (tries = 100; tries > 0; tries--) {
		ret = sceKernelTryLockLwMutex(&g_userTraceLock, 1);
		if (ret == 0)
			return 0;
		if (ret != SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN) {
			sceClibPrintf("err = %x\n", ret);
			if (ret >= 0)
				return 0;
			return SCE_ERROR_ERRNO_ENOLCK;
		}
		{
			SceUInt32 cnt = (readPmccntr() & 0xFFU) + 511U;

			do {
				(void)readPmccntr();
			} while (cnt-- != 0);
		}
	}

	if (sceKernelLockLwMutex(&g_userTraceLock, 1, NULL) < 0)
		return SCE_ERROR_ERRNO_ENOLCK;
	return 0;
}

static inline RazorCpuThreadSlot *findOrAllocThreadSlot(SceUID threadId)
{
	SceUInt32 i;

	for (i = 0; i < RAZOR_MAX_TRACKED_THREADS; i++) {
		if (i >= g_numTrackedThreads) {
			RazorCpuThreadSlot *slot = &g_threadSlots[g_numTrackedThreads];

			slot->threadId = threadId;
			slot->stackLevel = 0;
			slot->hudMask = 0;
			g_numTrackedThreads++;
			return slot;
		}
		if (g_threadSlots[i].threadId == threadId)
			return &g_threadSlots[i];
	}

	return NULL;
}

static inline void resetThreadSlotsIfInactive(void)
{
	if (g_razorCpuDebugInfo.captureFlags == 0) {
		int i;

		g_numTrackedThreads = 0;
		for (i = 0; i < RAZOR_MAX_TRACKED_THREADS; i++) {
			g_threadSlots[i].threadId = 0;
			g_threadSlots[i].stackLevel = 0;
		}
	}
}

static inline void writeStringWordsToPa(volatile SceUInt32 *paReg, const char *str,
					SceUInt32 numWords)
{
	if (((SceUIntPtr)str & 3U) != 0) {
		for (;;) {
			SceInt32 b0 = (SceInt8)str[0];
			SceInt32 b1 = (SceInt8)str[1];
			SceInt32 b2 = (SceInt8)str[2];
			SceInt32 b3 = (SceInt8)str[3];

			*paReg = (SceUInt32)((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
			if (b0 == 0 || b1 == 0 || b2 == 0 || b3 == 0)
				break;
			str += 4;
		}
	} else if (numWords != 0) {
		const SceUInt32 *w = (const SceUInt32 *)str;
		SceUInt32 i = 0;

		while (i + 8 < numWords) {
			__builtin_prefetch(w + 34);
			*paReg = w[0];
			*paReg = w[1];
			*paReg = w[2];
			*paReg = w[3];
			*paReg = w[4];
			*paReg = w[5];
			*paReg = w[6];
			*paReg = w[7];
			w += 8;
			i += 8;
		}
		do {
			*paReg = *w++;
			i++;
		} while (i < numWords);
	}
}

static int writePushMarkerPacket(const char *szLabel, SceUInt32 color, int hasColor)
{
	SceUID threadId;
	RazorCpuThreadSlot *slot;
	volatile SceUInt32 *paReg;
	SceUInt32 labelWords;
	SceUInt32 hdrType;
	SceUInt32 totalWords;
	SceUInt32 i;
	SceUInt32 value;

	threadId = sceKernelGetThreadId();
	slot = findOrAllocThreadSlot(threadId);
	if (slot == NULL)
		return 0;

	labelWords = 1;
	paReg = &g_paIoBase[9];
	if (szLabel != NULL) {
		const char *p = szLabel;
		int len;

		labelWords = RAZOR_MAX_LABEL_WORDS;
		for (len = 0; len < RAZOR_MAX_LABEL_LEN; len++) {
			if (*p++ == '\0') {
				labelWords = (SceUInt32)(len + 4) >> 2;
				break;
			}
		}
	}

	hdrType = (hasColor == 1) ? 0x120000U : 0x020000U;
	totalWords = g_pmonEventInfo.fields.numCounters + 5U + labelWords;
	if (hasColor == 1)
		totalWords++;

	*paReg = hdrType | (totalWords * 4U);
	*paReg = (SceUInt32)threadId;
	*paReg = readPmccntr();
	*paReg = slot->stackLevel;

	for (i = 0; i < SCE_PERF_ARM_PMON_PMCNT_NUM; i++) {
		_razorCpuSelectPmonCounter(i);
		g_pmonEventInfo.fields.eventTypes[i] = (SceUInt8)readPmxevtyper();
	}

	*paReg = g_pmonEventInfo.words[0];
	*paReg = g_pmonEventInfo.words[1];
	if (hasColor == 1)
		*paReg = color;

	for (i = 0; i < g_pmonEventInfo.fields.numCounters; i++) {
		scePerfArmPmonGetCounterValue(0, i, &value);
		*paReg = value;
	}

	if (szLabel != NULL)
		writeStringWordsToPa(paReg, szLabel, labelWords);
	else
		*paReg = 0;

	__asm__ volatile("dmb" ::: "memory");
	resetThreadSlotsIfInactive();
	return 0;
}

int sceRazorCpuGetUserMarkerTraceBuffer(SceRazorCpuUserMarkerTracePacket **pTrace)
{
	int ret;
	SceUInt32 count;

	if (!g_razorCpuInitialized)
		return SCE_ERROR_ERRNO_EINVAL;

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_USER_TRACE) != 0) {
		count = g_userMarkerCount;
		*pTrace = g_userMarkerBufs[g_userMarkerBufIdx];
		g_userMarkerCount = 0;
		g_userMarkerBufIdx = 1U - g_userMarkerBufIdx;
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		return (int)count;
	}

	*pTrace = NULL;
	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	return SCE_ERROR_ERRNO_EINVAL;
}

int sceRazorCpuSync(void)
{
	SceUInt32 savedCntEn;
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	savedCntEn = readPmcntenset();
	writePmcntenclr(savedCntEn);

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0) {
		volatile SceUInt32 *paReg = &g_paIoBase[9];

		*paReg = 0x10004U;
		*paReg = (SceUInt32)sceKernelGetThreadId();
		__asm__ volatile("dmb" ::: "memory");
		_razorCpuSyncHook();
		resetThreadSlotsIfInactive();

		if (g_razorCpuDebugInfo.stopFrameCountdown != 0) {
			if (--g_razorCpuDebugInfo.stopFrameCountdown == 0) {
				_razorCpuStopAfterFramesHook();
				resetThreadSlotsIfInactive();
			}
		}
	}

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_USER_TRACE) == 0) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return 0;
	}

	if (g_userMarkerCount >= g_userMarkerMaxPackets) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		SceUID threadId = sceKernelGetThreadId();
		SceRazorCpuUserMarkerTracePacket *pkt =
			&g_userMarkerBufs[g_userMarkerBufIdx][g_userMarkerCount];

		pkt->header = 0x10000U;
		pkt->threadId = (SceUInt32)threadId;
		pkt->stackLevel = 0;
		pkt->color = 0;
		pkt->timestamp = scePerfGetTimebaseValue();
		g_userMarkerCount++;
		ret = 0;
	}

	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	writePmcntenset(savedCntEn);
	return ret;
}

int sceRazorCpuPushMarkerWithHud(const char *szLabel, SceUInt32 color, SceUInt32 flags)
{
	SceUInt32 savedCntEn;
	SceUID threadId;
	RazorCpuThreadSlot *slot;
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (!g_razorCpuInitialized)
		return 0;

	savedCntEn = readPmcntenset();
	writePmcntenclr(savedCntEn);

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	threadId = sceKernelGetThreadId();
	slot = findOrAllocThreadSlot(threadId);
	if (slot == NULL) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return 0;
	}

	if (slot->stackLevel >= RAZOR_MAX_MARKER_DEPTH) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return SCE_ERROR_ERRNO_EINVAL;
	}

	ret = 0;
	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0)
		ret = writePushMarkerPacket(szLabel, color, 1);

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_USER_TRACE) != 0 && flags != 0) {
		if (g_userMarkerCount >= g_userMarkerMaxPackets) {
			slot->stackLevel++;
			sceKernelUnlockLwMutex(&g_userTraceLock, 1);
			writePmcntenset(savedCntEn);
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		{
			SceRazorCpuUserMarkerTracePacket *pkt =
				&g_userMarkerBufs[g_userMarkerBufIdx][g_userMarkerCount];

			slot->hudMask |= (SceUInt32)(1U << slot->stackLevel);
			pkt->header = 0x20000U;
			pkt->threadId = (SceUInt32)threadId;
			pkt->stackLevel = slot->stackLevel;
			pkt->color = color;
			pkt->timestamp = scePerfGetTimebaseValue();
			slot->stackLevel++;
			g_userMarkerCount++;
			sceKernelUnlockLwMutex(&g_userTraceLock, 1);
			writePmcntenset(savedCntEn);
			return ret;
		}
	}

	slot->stackLevel++;
	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	writePmcntenset(savedCntEn);
	return ret;
}

int sceRazorCpuPushMarker(const char *szLabel)
{
	SceUInt32 savedCntEn;
	SceUID threadId;
	RazorCpuThreadSlot *slot;
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (!g_razorCpuInitialized)
		return 0;

	savedCntEn = readPmcntenset();
	writePmcntenclr(savedCntEn);

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	threadId = sceKernelGetThreadId();
	slot = findOrAllocThreadSlot(threadId);
	if (slot == NULL) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return 0;
	}

	if (slot->stackLevel >= RAZOR_MAX_MARKER_DEPTH) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return SCE_PERF_ERROR_TOO_MANY_PUSHES;
	}

	ret = 0;
	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0)
		ret = writePushMarkerPacket(szLabel, 0, 0);

	slot->stackLevel++;
	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	writePmcntenset(savedCntEn);
	return ret;
}

int sceRazorCpuPopMarker(void)
{
	SceUInt32 savedCntEn;
	SceUID threadId;
	RazorCpuThreadSlot *slot;
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (!g_razorCpuInitialized)
		return 0;

	savedCntEn = readPmcntenset();
	writePmcntenclr(savedCntEn);

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	threadId = sceKernelGetThreadId();
	slot = findOrAllocThreadSlot(threadId);
	if (slot == NULL || slot->stackLevel == 0) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return 0;
	}

	slot->stackLevel--;

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0) {
		SceUID popTid = sceKernelGetThreadId();
		RazorCpuThreadSlot *popSlot = findOrAllocThreadSlot(popTid);

		if (popSlot != NULL) {
			volatile SceUInt32 *paReg = &g_paIoBase[9];
			SceUInt32 i;
			SceUInt32 value;

			*paReg = 0x30000U | ((g_pmonEventInfo.fields.numCounters + 4U) * 4U);
			*paReg = (SceUInt32)popTid;
			*paReg = readPmccntr();
			*paReg = popSlot->stackLevel;
			*paReg = g_pmonEventInfo.fields.numCounters;

			for (i = 0; i < g_pmonEventInfo.fields.numCounters; i++) {
				scePerfArmPmonGetCounterValue(0, i, &value);
				*paReg = value;
			}

			__asm__ volatile("dmb" ::: "memory");
			resetThreadSlotsIfInactive();
		}
	}

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_USER_TRACE) == 0) {
		sceKernelUnlockLwMutex(&g_userTraceLock, 1);
		writePmcntenset(savedCntEn);
		return 0;
	}

	if (g_userMarkerCount >= g_userMarkerMaxPackets) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		SceUInt32 level = slot->stackLevel;

		ret = 0;
		if (((slot->hudMask >> level) & 1ULL) != 0) {
			SceRazorCpuUserMarkerTracePacket *pkt =
				&g_userMarkerBufs[g_userMarkerBufIdx][g_userMarkerCount];

			pkt->header = 0x30000U;
			pkt->threadId = (SceUInt32)threadId;
			pkt->stackLevel = level;
			pkt->color = 0;
			pkt->timestamp = scePerfGetTimebaseValue();
			slot->hudMask &= ~(1ULL << slot->stackLevel);
			g_userMarkerCount++;
		}
	}

	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	writePmcntenset(savedCntEn);
	return ret;
}

int sceRazorCpuStartUserMarkerTrace(void *pBufferBase, SceUInt32 bufferSize)
{
	SceUInt32 maxPackets;

	if (!g_razorCpuInitialized || g_razorCpuDebugInfo.captureFlags != 0 ||
	    pBufferBase == NULL || ((SceUIntPtr)pBufferBase & 7U) != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	maxPackets = bufferSize / (sizeof(SceRazorCpuUserMarkerTracePacket) * 2U);
	g_userMarkerMaxPackets = maxPackets;
	if (maxPackets < 64U)
		return SCE_ERROR_ERRNO_ENOMEM;

	g_userMarkerBufs[0] = (SceRazorCpuUserMarkerTracePacket *)pBufferBase;
	g_userMarkerBufs[1] = g_userMarkerBufs[0] + maxPackets;

	if (g_razorCpuDebugInfo.captureFlags == 0) {
		int i;

		g_numTrackedThreads = 0;
		for (i = 0; i < RAZOR_MAX_TRACKED_THREADS; i++) {
			g_threadSlots[i].threadId = 0;
			g_threadSlots[i].stackLevel = 0;
		}
		g_razorCpuDebugInfo.captureFlags = RAZOR_CAPTURE_FLAG_USER_TRACE;
	}

	return 0;
}

int sceRazorCpuStopUserMarkerTrace(void)
{
	if (!g_razorCpuInitialized || g_razorCpuDebugInfo.captureFlags == 0)
		return SCE_ERROR_ERRNO_EINVAL;

	g_userMarkerMaxPackets = 0;
	g_razorCpuDebugInfo.captureFlags = 0;
	g_userMarkerBufs[0] = NULL;
	g_userMarkerBufs[1] = NULL;
	return 0;
}

int sceRazorCpuStartCapture(void)
{
	int ret;

	if (!g_razorCpuInitialized)
		return SCE_ERROR_ERRNO_EINVAL;

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0)
		return SCE_PERF_ERROR_ALREADY_STARTED;

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	_razorCpuStartCaptureHook();
	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0)
		ret = 0;
	else
		ret = SCE_PERF_ERROR_CANNOT_START;

	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	return ret;
}

int sceRazorCpuStopCapture(void)
{
	int ret;

	if (!g_razorCpuInitialized)
		return SCE_ERROR_ERRNO_EINVAL;

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) == 0)
		return SCE_PERF_ERROR_ALREADY_STOPPED;

	ret = lockUserTrace();
	if (ret < 0)
		return ret;

	_razorCpuStopCaptureHook();
	resetThreadSlotsIfInactive();

	if ((g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE) != 0)
		ret = SCE_PERF_ERROR_CANNOT_STOP;
	else
		ret = 0;

	sceKernelUnlockLwMutex(&g_userTraceLock, 1);
	return ret;
}

SceUInt32 sceRazorCpuIsCapturing(void)
{
	return g_razorCpuDebugInfo.captureFlags & RAZOR_CAPTURE_FLAG_ACTIVE;
}

int _sceRazorCpuInit(void)
{
	return 0;
}

int razorCpuInit(SceUInt32 numCounters)
{
	int ret;

	if (g_razorCpuInitialized != 0 || numCounters > SCE_PERF_ARM_PMON_PMCNT_NUM)
		return 1;

	g_pmonEventInfo.fields.numCounters = (SceUInt16)numCounters;
	g_razorCpuDebugInfo.captureFlags = 0;
	ret = sceKernelCreateLwMutex(&g_userTraceLock, "SceRazorCpuUsertraceLock", 0, 0, NULL);
	if (ret >= 0) {
		int i;

		g_numTrackedThreads = 0;
		for (i = 0; i < RAZOR_MAX_TRACKED_THREADS; i++) {
			g_threadSlots[i].threadId = 0;
			g_threadSlots[i].stackLevel = 0;
		}
		g_razorCpuInitialized = 1;
		return 0;
	}
	return ret;
}

int _sceRazorCpuWriteFiberUltPkt(SceUInt32 kind, SceUInt32 event, SceUInt32 arg,
				 const void *object, const void *data)
{
	SceUInt32 hasName = arg & 1U;
	SceUInt32 words = 0;
	SceUInt32 sizeField = 0x10000U;
	volatile SceUInt32 *paReg = &g_paIoBase[15];

	if (hasName != 0 && data != NULL) {
		const char *p = (const char *)data;
		int len;

		words = RAZOR_MAX_LABEL_WORDS;
		sizeField = (RAZOR_MAX_LABEL_WORDS + 1U) << 16;
		for (len = 0; len < RAZOR_MAX_LABEL_LEN; len++) {
			if (*p++ == '\0') {
				words = (SceUInt32)(len + 4) >> 2;
				sizeField = (words + 1U) << 16;
				break;
			}
		}
	}

	*paReg = (kind << 24) | (event << 8) | hasName | sizeField;
	*paReg = (SceUInt32)object;

	if (hasName != 0 && data != NULL)
		writeStringWordsToPa(paReg, (const char *)data, words);

	__asm__ volatile("dmb" ::: "memory");
	return 0;
}

int _sceCpuRazorPushFiberUserMarker(const char *szLabel, SceUInt32 *markerStack,
				    const void *fiber, SceUInt32 color, SceUInt32 flags)
{
	SceUInt32 level;
	volatile SceUInt32 *paReg;

	(void)flags;

	if (markerStack == NULL)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	level = *markerStack;
	if (level >= RAZOR_MAX_MARKER_DEPTH)
		return SCE_PERF_ERROR_TOO_MANY_PUSHES;

	paReg = &g_paIoBase[15];
	if (szLabel != NULL) {
		const char *p = szLabel;
		SceUInt32 words = RAZOR_MAX_LABEL_WORDS;
		SceUInt32 hdr = ((RAZOR_MAX_LABEL_WORDS + 3U) << 16) | 0x04008107U;
		int len;

		for (len = 0; len < RAZOR_MAX_LABEL_LEN; len++) {
			if (*p++ == '\0') {
				words = (SceUInt32)(len + 4) >> 2;
				hdr = ((words + 3U) << 16) | 0x04008107U;
				break;
			}
		}

		*paReg = hdr;
		*paReg = (SceUInt32)fiber;
		*paReg = level;
		*paReg = color;
		writeStringWordsToPa(paReg, szLabel, words);
	} else {
		*paReg = 0x04038106U;
		*paReg = (SceUInt32)fiber;
		*paReg = level;
		*paReg = color;
	}

	__asm__ volatile("dmb" ::: "memory");
	(*markerStack)++;
	return 0;
}

int _sceCpuRazorPopFiberUserMarker(SceUInt32 *markerStack, const void *fiber)
{
	SceUInt32 level;
	volatile SceUInt32 *paReg;

	if (markerStack == NULL)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	if (*markerStack == 0)
		return SCE_PERF_ERROR_POP_WITHOUT_PUSH;

	level = *markerStack - 1U;
	*markerStack = level;

	paReg = &g_paIoBase[15];
	*paReg = 0x04024202U;
	*paReg = (SceUInt32)fiber;
	*paReg = level;

	__asm__ volatile("dmb" ::: "memory");
	return 0;
}
