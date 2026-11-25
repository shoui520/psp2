/*
 * HUD data source classes: ring series, GPU buffers, GPU frametime,
 * CPU activity monitor, CPU user markers, and GPU timeline.
 */
#include "razor_hud_internal.h"

typedef struct HudDataSrcVtable {
	void *(*dtor)(void *self);
	void *(*deletingDtor)(void *self);
	void (*update)(void *self);
	float (*getWindowScale)(void *self);
} HudDataSrcVtable;

typedef struct HudSeriesRing {
	float *samples;
	uint32_t head;
	uint32_t count;
} HudSeriesRing;

typedef struct HudSeriesDataSrc {
	const HudDataSrcVtable *vtable; /* 0 */
	uint8_t dirty;                  /* 4 */
	uint8_t dirtyPad[3];
	float curMin;                   /* 8 */
	float baseMin;                  /* 12 */
	float curMax;                   /* 16 */
	float baseMax;                  /* 20 */
	float step;                     /* 24 */
	uint32_t autoWindow;            /* 28 */
	HudSeriesRing *rings;           /* 32 */
	uint32_t *colors;               /* 36 */
	uint32_t seriesCount;           /* 40 */
	uint32_t capacity;              /* 44 */
	uint8_t autoMin;                /* 48 */
	uint8_t autoMax;                /* 49 */
	uint8_t seriesTailPad[2];
} HudSeriesDataSrc;

typedef struct HudRingStat {
	uint32_t capacity;
	uint32_t used;
	uint32_t overflows;
	uint32_t updates;
	float peakPct;
} HudRingStat;

typedef struct HudGpuBufDataSrc {
	HudSeriesDataSrc base;          /* 0..51 */
	HudRingStat rings[5];           /* 52..151 */
	void *gpuTimeline;              /* 152 */
} HudGpuBufDataSrc;

typedef struct HudGpuFrameDataSrc {
	HudSeriesDataSrc base;          /* 0..51 */
	int32_t lastFrameSeq;           /* 52 */
	void *gpuTimeline;              /* 56 */
} HudGpuFrameDataSrc;

typedef struct HudCpuActDataSrc {
	const HudDataSrcVtable *vtable; /* 0 */
	uint8_t dirty;                  /* 4 */
	uint8_t dirtyPad[3];
	HudSeriesDataSrc coreSeries[4]; /* 8..215 */
	HudSeriesDataSrc totalSeries;   /* 216..267 */
	HudSeriesDataSrc fpsSeries;     /* 268..319 */
	uint64_t startClock;            /* 320 */
	uint32_t elapsedSec;            /* 328 */
	uint32_t frameCounter;          /* 332 */
} HudCpuActDataSrc;

typedef struct HudTimelineEvent {
	uint32_t flags;
	float startMs;
	float endMs;
	uint32_t color;
} HudTimelineEvent;

typedef struct HudTimelineTrack {
	HudTimelineEvent events[4096];
	uint32_t count;
} HudTimelineTrack;

typedef struct HudTimelineMarker {
	float timeMs;
	uint32_t color;
} HudTimelineMarker;

typedef struct HudTimelineBase {
	const HudDataSrcVtable *vtable; /* 0 */
	uint8_t dirty;                  /* 4 */
	uint8_t dirtyPad[3];
	HudTimelineTrack tracks[12];    /* 8..786487 */
	uint32_t trackCount;            /* 786488 */
	HudTimelineMarker markers[128]; /* 786492..787515 */
	uint32_t markerCount;           /* 787516 */
	float windowStart;              /* 787520 */
	float windowEnd;                /* 787524 */
	float minSpan;                  /* 787528 */
} HudTimelineBase;

typedef struct HudTimingInfo {
	uint64_t paTime;                /* 0 */
	uint32_t tbFreq;                /* 8 */
	uint32_t tbFreqHi;              /* 12 */
	uint64_t gpuBaseUs;             /* 16 */
	uint64_t cpuBaseUs;             /* 24 */
	float gpuWinStart;              /* 32 */
	float gpuWinEnd;                /* 36 */
	float cpuWinStart;              /* 40 */
	float cpuWinEnd;                /* 44 */
	float cpuScale;                 /* 48 */
	float gpuScale;                 /* 52 */
	uint32_t sampleInterval;        /* 56 */
	uint32_t frameCounter;          /* 60 */
	uint32_t displayFrames;         /* 64 */
	uint32_t useCpuBase;            /* 68 */
} HudTimingInfo;

typedef struct HudCpuMarkersDataSrc {
	HudTimelineBase base;           /* 0..787531 */
	uint32_t warnCountdown;         /* 787532 */
	uint64_t frameTimes[128];       /* 787536..788559 */
	HudTimingInfo *timing;          /* 788560 */
	uint32_t maxPackets;            /* 788564 */
	uint32_t frameTimeCount;        /* 788568 */
	uint32_t depth[12];             /* 788572..788619 */
	uint32_t colorStack[12][10];    /* 788620..789099 */
	void *activeEv[12];             /* 789100..789147 */
	uint32_t *threadIds;            /* 789148 */
	uint32_t threadCount;           /* 789152 */
	uint32_t markersReserved;        /* 789156 */
	uint8_t mismatchWarn;           /* 789160 */
	uint8_t bufTooSmallPrinted;     /* 789161 */
	uint8_t markersTailPad[6];
} HudCpuMarkersDataSrc;

typedef struct HudGpuKickRecord {
	uint32_t type;
	float ms;
	uint32_t frame;
	uint32_t core;
} HudGpuKickRecord;

typedef struct HudGpuTimelineDataSrc {
	HudTimelineBase base;           /* 0..787531 */
	uint32_t sceneColorMode;        /* 787532 */
	uint32_t vertexTrackBase;       /* 787536 */
	uint32_t fragTrackBase;         /* 787540 */
	uint32_t fwTrackBase;           /* 787544 */
	uint32_t trackPad;             /* 787548 */
	uint64_t frameStarts[256];      /* 787552..789599 */
	uint32_t framePad[4];          /* 789600..789615 */
	uint32_t numCores;              /* 789616 */
	uint32_t numFw;                 /* 789620 */
	uint32_t frameSeq;              /* 789624 */
	uint32_t gpuFreq;               /* 789628 */
	uint64_t paTimeUs;              /* 789632 */
	uint64_t prevPaTimeUs;          /* 789640 */
	uint64_t curPaTimeUs;           /* 789648 */
	uint32_t frameStartCount;       /* 789656 */
	void *liveBufs[2];              /* 789660..789667 */
	uint32_t liveBufIdx;            /* 789668 */
	SceRazorGpuLiveResultInfo liveRes; /* 789672..789711 */
	uint32_t lastVertexFrame[4];    /* 789712..789727 */
	uint32_t lastFragFrame[4];      /* 789728..789743 */
	float lastFrameMs;              /* 789744 */
	uint32_t paramBufUsed;          /* 789748 */
	int32_t processId;              /* 789752 */
	HudGpuKickRecord kicks[16384];  /* 789756..1051899 */
	uint32_t kickCount;             /* 1051900 */
	uint8_t logEnabled;             /* 1051904 */
	uint8_t logFlagPad[3];
	HudTimingInfo *timing;          /* 1051908 */
} HudGpuTimelineDataSrc;

static const uint32_t g_sceneColors[16] = {
	0xFF6060FFU, 0xFF60FF60U, 0xFF60FFFFU, 0xFFFF6060U,
	0xFFFF60FFU, 0xFFFFFF60U, 0xFFFFFFFFU, 0xFF606060U,
	0xFF20208FU, 0xFF208F20U, 0xFF208F8FU, 0xFF8F2020U,
	0xFF8F208FU, 0xFF8F8F20U, 0xFF8F8F8FU, 0xFF404040U,
};

static const HudDataSrcVtable g_dataSrcBaseVtable = {
	hudDataSrcBaseDtor,
	hudDataSrcBaseDeletingDtor,
	hudDataSrcBaseReset,
	NULL,
};

static const HudDataSrcVtable g_seriesSrcVtable = {
	hudSeriesSrcDtor,
	hudSeriesSrcDeletingDtor,
	hudDataSrcBaseReset,
	NULL,
};

static const HudDataSrcVtable g_gpuBufSrcVtable = {
	hudGpuBufSrcDtor,
	hudGpuBufSrcDeletingDtor,
	hudGpuBufSrcUpdate,
	NULL,
};

static const HudDataSrcVtable g_gpuFrameSrcVtable = {
	hudGpuFrameSrcDtor,
	hudGpuFrameSrcDeletingDtor,
	hudGpuFrameSrcUpdate,
	NULL,
};

static const HudDataSrcVtable g_cpuActSrcVtable = {
	hudCpuActSrcDtor,
	hudCpuActSrcDeletingDtor,
	hudCpuActSrcUpdate,
	NULL,
};

static const HudDataSrcVtable g_timelineSrcVtable = {
	hudTimelineSrcDtor,
	hudTimelineSrcDeletingDtor,
	hudTimelineSrcReset,
	hudTimelineGetWindowScale,
};

static const HudDataSrcVtable g_cpuMarkersSrcVtable = {
	hudCpuMarkersSrcDtor,
	hudCpuMarkersSrcDeletingDtor,
	(void (*)(void *))hudCpuMarkersSrcUpdate,
	hudCpuMarkersGetWindowScale,
};

static const HudDataSrcVtable g_gpuTimelineSrcVtable = {
	hudGpuTimelineSrcDtor,
	hudGpuTimelineSrcDeletingDtor,
	hudGpuTimelineSrcUpdate,
	hudGpuTimelineGetWindowScale,
};

/* Base data source */
void *hudDataSrcBaseDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_dataSrcBaseVtable;
	return objPtr;
}

void hudDataSrcBaseReset(void *objPtr)
{
	*((uint8_t *)objPtr + 4) = 0;
}

void *hudDataSrcBaseDeletingDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_dataSrcBaseVtable;
	hudDelete(objPtr);
	return objPtr;
}

void *hudDataSrcBaseCtor(void *objPtr)
{
	*((uint8_t *)objPtr + 4) = 0;
	*(const HudDataSrcVtable **)objPtr = &g_dataSrcBaseVtable;
	return objPtr;
}

/* GPU Buffers data source */
void *hudGpuBufSrcDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_gpuBufSrcVtable;
	hudSeriesSrcDtor(objPtr);
	return objPtr;
}

void *hudGpuBufSrcDeletingDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_gpuBufSrcVtable;
	hudSeriesSrcDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void hudGpuBufSrcUpdate(void *objPtr)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	int i;

	if (s->gpuTimeline != NULL) {
		HudGpuTimelineDataSrc *tl = (HudGpuTimelineDataSrc *)s->gpuTimeline;
		if ((tl->timing->frameCounter % tl->timing->sampleInterval) != 0)
			return;
		{
			uint32_t used = hudGpuTimelineSrcGetParamBufferUsed(tl);
			float pct;
			s->rings[4].used = used;
			s->rings[4].updates = 1U;
			pct = ((float)used * 100.0f) / (float)s->rings[4].capacity;
			if (pct > 100.0f)
				pct = 100.0f;
			if (s->rings[4].peakPct < pct)
				s->rings[4].peakPct = pct;
		}
	}
	for (i = 0; i < 5; i++) {
		float val = s->rings[i].peakPct;
		hudSeriesSrcPush(&s->base, (uint32_t)i, &val);
		s->rings[i].peakPct = 0.0f;
	}
}

void *hudGpuBufSrcCtor(void *objPtr)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	hudSeriesSrcCtor(&s->base);
	s->base.vtable = &g_gpuBufSrcVtable;
	hudSeriesSrcSetSeriesCount(&s->base, 5U);
	hudSeriesSrcSetColor(&s->base, 0U, 0xFF00FFFFU);
	hudSeriesSrcSetColor(&s->base, 1U, 0xFF40BFFFU);
	hudSeriesSrcSetColor(&s->base, 2U, 0xFF8080FFU);
	hudSeriesSrcSetColor(&s->base, 3U, 0xFFFF40FFU);
	hudSeriesSrcSetColor(&s->base, 4U, 0xFFFFFFFFU);
	sceClibMemset(s->rings, 0, sizeof(s->rings));
	return s;
}

void hudGpuBufSrcOnOverflow(void *objPtr, int a1, int a2, int a3, uint8_t a4)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	if (a1 != 0) s->rings[0].overflows++;
	if (a2 != 0) s->rings[1].overflows++;
	if (a3 != 0) s->rings[2].overflows++;
	if (a4 != 0) s->rings[3].overflows++;
}

void hudGpuBufSrcOnInit(void *objPtr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	int i;
	s->rings[0].capacity = a1;
	s->rings[1].capacity = a2;
	s->rings[2].capacity = a3;
	s->rings[3].capacity = a4;
	for (i = 0; i < 4; i++) {
		s->rings[i].used = 0;
		s->rings[i].overflows = 0;
		s->rings[i].updates = 0;
	}
}

void hudGpuBufSrcOnUpdate(void *objPtr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	uint32_t deltas[4];
	int i;

	deltas[0] = a1;
	deltas[1] = a2;
	deltas[2] = a3;
	deltas[3] = a4;
	for (i = 0; i < 4; i++) {
		float pct;
		s->rings[i].used += deltas[i];
		s->rings[i].updates++;
		pct = ((float)s->rings[i].used * 100.0f) / (float)s->rings[i].capacity;
		if (pct > s->rings[i].peakPct)
			s->rings[i].peakPct = pct;
	}
}

void hudGpuBufSrcSetParamBufSize(void *objPtr, uint32_t val)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	s->rings[4].capacity = val;
}

void hudGpuBufSrcSetGpuTimeline(void *objPtr, void *tl)
{
	HudGpuBufDataSrc *s = (HudGpuBufDataSrc *)objPtr;
	s->gpuTimeline = tl;
}

/* GPU Frametime data source */
void *hudGpuFrameSrcDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_gpuFrameSrcVtable;
	hudSeriesSrcDtor(objPtr);
	return objPtr;
}

void *hudGpuFrameSrcDeletingDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_gpuFrameSrcVtable;
	hudSeriesSrcDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void hudGpuFrameSrcUpdate(void *objPtr)
{
	HudGpuFrameDataSrc *s = (HudGpuFrameDataSrc *)objPtr;
	int32_t seq;
	float v;

	if (s->gpuTimeline == NULL)
		return;
	seq = (int32_t)hudGpuTimelineSrcGetFrameSeq(s->gpuTimeline);
	if (s->lastFrameSeq == seq)
		return;
	v = hudGpuTimelineSrcSumVertexMs(s->gpuTimeline);
	hudSeriesSrcPush(&s->base, 0U, &v);
	v = hudGpuTimelineSrcSumFragmentMs(s->gpuTimeline);
	hudSeriesSrcPush(&s->base, 1U, &v);
	v = hudGpuTimelineSrcGetFrameMs(s->gpuTimeline);
	hudSeriesSrcPush(&s->base, 2U, &v);
	s->lastFrameSeq = seq;
}

void *hudGpuFrameSrcCtor(void *objPtr)
{
	HudGpuFrameDataSrc *s = (HudGpuFrameDataSrc *)objPtr;
	hudSeriesSrcCtor(&s->base);
	s->base.vtable = &g_gpuFrameSrcVtable;
	s->lastFrameSeq = -1;
	s->gpuTimeline = NULL;
	hudSeriesSrcSetSeriesCount(&s->base, 3U);
	hudSeriesSrcSetColor(&s->base, 0U, 0xF08080FFU);
	hudSeriesSrcSetColor(&s->base, 1U, 0xF0FF8080U);
	hudSeriesSrcSetColor(&s->base, 2U, 0xF0FFFFFFU);
	return s;
}

void hudGpuFrameSrcSetGpuTimeline(void *objPtr, void *tl)
{
	HudGpuFrameDataSrc *s = (HudGpuFrameDataSrc *)objPtr;
	s->gpuTimeline = tl;
}

/* Generic ring-buffer series data source */
static inline __attribute__((always_inline)) void hudFreeSeriesRings(HudSeriesDataSrc *s)
{
	if (s->rings != NULL) {
		uint32_t i;
		for (i = s->seriesCount; i > 0; i--) {
			if (s->rings[i - 1U].samples != NULL)
				hudDeleteArray(s->rings[i - 1U].samples);
		}
		hudDeleteArray(s->rings);
		s->rings = NULL;
	}
}

void *hudSeriesSrcDtor(void *objPtr)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	s->vtable = &g_seriesSrcVtable;
	hudFreeSeriesRings(s);
	if (s->colors != NULL) {
		hudDeleteArray(s->colors);
		s->colors = NULL;
	}
	hudDataSrcBaseDtor(s);
	return s;
}

void *hudSeriesSrcDeletingDtor(void *objPtr)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	hudSeriesSrcDtor(s);
	hudDelete(s);
	return s;
}

void *hudSeriesSrcCtor(void *objPtr)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	hudDataSrcBaseCtor(s);
	s->vtable = &g_seriesSrcVtable;
	s->autoMax = 1;
	s->autoWindow = 500U;
	s->curMin = 0.0f;
	s->baseMin = 0.0f;
	s->step = 0.0f;
	s->curMax = 1.0f;
	s->baseMax = 1.0f;
	s->autoMin = 0;
	s->capacity = 0;
	s->rings = NULL;
	s->colors = NULL;
	s->seriesCount = 0;
	return s;
}

void hudSeriesSrcPush(void *objPtr, uint32_t series, const float *val)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	HudSeriesRing *r = &s->rings[series];
	uint32_t i, j;

	if (r->count >= s->capacity) {
		r->samples[r->head] = *val;
		r->head = (r->head + 1U) % s->capacity;
	} else {
		r->samples[(r->head + r->count) % s->capacity] = *val;
		r->count++;
	}

	if (!s->autoMin && !s->autoMax)
		return;
	if (s->autoMin)
		s->curMin = s->baseMin;
	if (s->autoMax)
		s->curMax = s->baseMax;

	for (i = 0; i < s->seriesCount; i++) {
		HudSeriesRing *ri = &s->rings[i];
		uint32_t start = (ri->count > s->autoWindow) ? (ri->count - s->autoWindow) : 0U;
		for (j = start; j < ri->count; j++) {
			float v = ri->samples[(ri->head + j) % s->capacity];
			if (s->autoMin && v < s->curMin) {
				if (s->step <= 0.0f) {
					s->curMin = v;
				} else {
					while (v < s->curMin)
						s->curMin -= s->step;
				}
			}
			if (s->autoMax && v > s->curMax) {
				if (s->step <= 0.0f) {
					s->curMax = v;
				} else {
					float inc = s->step * 0.25f;
					while (v > s->curMax)
						s->curMax += inc;
				}
			}
		}
	}
}

float *hudSeriesSrcGetSample(void *objPtr, uint32_t series, uint32_t idx)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	HudSeriesRing *r = &s->rings[series];
	return &r->samples[(r->head + idx) % s->capacity];
}

uint32_t hudSeriesSrcGetCount(void *objPtr, uint32_t series)
{
	return ((HudSeriesDataSrc *)objPtr)->rings[series].count;
}

uint32_t hudSeriesSrcGetSeriesCount(void *objPtr)
{
	return ((HudSeriesDataSrc *)objPtr)->seriesCount;
}

uint32_t hudSeriesSrcGetCapacity(void *objPtr)
{
	return ((HudSeriesDataSrc *)objPtr)->capacity;
}

void hudSeriesSrcSetColor(void *objPtr, uint32_t series, uint32_t color)
{
	((HudSeriesDataSrc *)objPtr)->colors[series] = color;
}

uint32_t hudSeriesSrcGetColor(void *objPtr, uint32_t series)
{
	return ((HudSeriesDataSrc *)objPtr)->colors[series];
}

void hudSeriesSrcSetRange(void *objPtr, float minVal, float maxVal)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	s->curMin = minVal;
	s->baseMin = minVal;
	s->curMax = maxVal;
	s->baseMax = maxVal;
}

float *hudSeriesSrcGetMinPtr(void *objPtr)
{
	return &((HudSeriesDataSrc *)objPtr)->curMin;
}

float *hudSeriesSrcGetMaxPtr(void *objPtr)
{
	return &((HudSeriesDataSrc *)objPtr)->curMax;
}

void hudSeriesSrcSetAutoScale(void *objPtr, uint8_t autoMin, uint8_t autoMax, uint32_t window)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	s->autoMin = autoMin;
	s->autoMax = autoMax;
	s->autoWindow = window;
}

void hudSeriesSrcSetCapacity(void *objPtr, uint32_t cap)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	uint32_t i;

	hudFreeSeriesRings(s);
	s->capacity = cap;
	s->rings = (HudSeriesRing *)hudNewArray(s->seriesCount * sizeof(HudSeriesRing));
	for (i = 0; i < s->seriesCount; i++) {
		s->rings[i].samples = (float *)hudNewArray(cap * sizeof(float));
		s->rings[i].head = 0;
		s->rings[i].count = 0;
	}
}

void hudSeriesSrcSetSeriesCount(void *objPtr, uint32_t count)
{
	HudSeriesDataSrc *s = (HudSeriesDataSrc *)objPtr;
	uint32_t i;

	hudFreeSeriesRings(s);
	if (s->colors != NULL) {
		hudDeleteArray(s->colors);
		s->colors = NULL;
	}
	s->seriesCount = count;
	s->rings = (HudSeriesRing *)hudNewArray(count * sizeof(HudSeriesRing));
	for (i = 0; i < count; i++) {
		s->rings[i].samples = (float *)hudNewArray(s->capacity * sizeof(float));
		s->rings[i].head = 0;
		s->rings[i].count = 0;
	}
	s->colors = (uint32_t *)hudNewArray(count * sizeof(uint32_t));
	for (i = 0; i < count; i++)
		s->colors[i] = 0xFFFFFFFFU;
}

void hudSeriesSrcSetStep(void *objPtr, float step)
{
	((HudSeriesDataSrc *)objPtr)->step = step;
}

float hudSeriesSrcGetStep(void *objPtr)
{
	return ((HudSeriesDataSrc *)objPtr)->step;
}

/* CPU Activity Monitor data source */
typedef struct HudCpuActSample {
	uint64_t userNs;
	uint64_t sysNs;
	uint64_t idleNs;
	uint64_t timestamp;
	uint32_t core;
	uint32_t reserved;
} HudCpuActSample;

void hudCpuActSrcUpdate(void *objPtr)
{
	HudCpuActDataSrc *s = (HudCpuActDataSrc *)objPtr;
	void *buf = NULL;
	int count, i;
	float val;

	s->frameCounter++;
	if (s->startClock != 0) {
		uint64_t now = 0;
		sceKernelGetProcessTime(&now);
		if ((now - s->startClock) / 1000000ULL < (uint64_t)s->elapsedSec)
			return;
		s->elapsedSec++;
	} else {
		sceKernelGetProcessTime(&s->startClock);
		s->elapsedSec = 1U;
	}

	val = (float)s->frameCounter;
	hudSeriesSrcPush(&s->fpsSeries, 0U, &val);
	val = 60.0f;
	hudSeriesSrcPush(&s->fpsSeries, 1U, &val);
	s->frameCounter = 0;

	count = sceRazorCpuGetActivityMonitorTraceBuffer(&buf);
	if (count > 0) {
		const HudCpuActSample *samples = (const HudCpuActSample *)buf;
		double totIdle = 0.0, totUser = 0.0, totSys = 0.0;
		i = 0;
		while (i < count) {
			uint64_t ts = samples[i].timestamp;
			while (i < count && samples[i].timestamp == ts) {
				double idle = hudU64ToDouble(samples[i].idleNs) / 1000000000.0;
				double user = hudU64ToDouble(samples[i].userNs) / 1000000000.0;
				double sys  = hudU64ToDouble(samples[i].sysNs)  / 1000000000.0;
				double sum  = idle + user + sys;
				uint32_t c  = samples[i].core;
				val = (float)(idle / sum);
				hudSeriesSrcPush(&s->coreSeries[c], 0U, &val);
				val = (float)(user / sum);
				hudSeriesSrcPush(&s->coreSeries[c], 1U, &val);
				val = (float)(sys / sum);
				hudSeriesSrcPush(&s->coreSeries[c], 2U, &val);
				val = 1.0f;
				hudSeriesSrcPush(&s->coreSeries[c], 3U, &val);
				totIdle += idle;
				totUser += user;
				totSys  += sys;
				i++;
			}
			{
				double sum = totIdle + totUser + totSys;
				val = (float)(totIdle / sum);
				hudSeriesSrcPush(&s->totalSeries, 0U, &val);
				val = (float)(totUser / sum);
				hudSeriesSrcPush(&s->totalSeries, 1U, &val);
				val = (float)(totSys / sum);
				hudSeriesSrcPush(&s->totalSeries, 2U, &val);
				val = 1.0f;
				hudSeriesSrcPush(&s->totalSeries, 3U, &val);
			}
		}
	}
}

void *hudCpuActSrcCtor(void *objPtr)
{
	HudCpuActDataSrc *s = (HudCpuActDataSrc *)objPtr;
	int i;

	hudDataSrcBaseCtor(s);
	s->vtable = &g_cpuActSrcVtable;
	for (i = 0; i < 4; i++)
		hudSeriesSrcCtor(&s->coreSeries[i]);
	hudSeriesSrcCtor(&s->totalSeries);
	hudSeriesSrcCtor(&s->fpsSeries);

	for (i = 0; i < 5; i++) {
		HudSeriesDataSrc *ser = (i < 4) ? &s->coreSeries[i] : &s->totalSeries;
		hudSeriesSrcSetSeriesCount(ser, 4U);
		hudSeriesSrcSetColor(ser, 0U, 0xF080FF80U);
		hudSeriesSrcSetColor(ser, 1U, 0xF080FFFFU);
		hudSeriesSrcSetColor(ser, 2U, 0U);
		hudSeriesSrcSetColor(ser, 3U, 0U);
		hudSeriesSrcSetRange(ser, 0.0f, 1.0f);
		hudSeriesSrcSetAutoScale(ser, 0, 0, 0U);
		hudSeriesSrcSetStep(ser, 0.25f);
	}
	hudSeriesSrcSetStep(&s->totalSeries, 0.25f);

	hudSeriesSrcSetSeriesCount(&s->fpsSeries, 2U);
	hudSeriesSrcSetColor(&s->fpsSeries, 0U, 0xF0FF8080U);
	hudSeriesSrcSetColor(&s->fpsSeries, 1U, 0U);
	hudSeriesSrcSetRange(&s->fpsSeries, 0.0f, 60.0f);
	hudSeriesSrcSetAutoScale(&s->fpsSeries, 0, 1, hudSeriesSrcGetCapacity(&s->fpsSeries));
	hudSeriesSrcSetStep(&s->fpsSeries, 15.0f);

	s->frameCounter = 0;
	s->elapsedSec = 0;
	s->startClock = 0;
	return s;
}

void hudCpuActSrcSetCapacity(void *objPtr, uint32_t cap)
{
	HudCpuActDataSrc *s = (HudCpuActDataSrc *)objPtr;
	int i;
	for (i = 0; i < 4; i++)
		hudSeriesSrcSetCapacity(&s->coreSeries[i], cap);
	hudSeriesSrcSetCapacity(&s->totalSeries, cap);
	hudSeriesSrcSetCapacity(&s->fpsSeries, cap);
	hudSeriesSrcSetAutoScale(&s->fpsSeries, 0, 1, cap);
}

void *hudCpuActSrcGetCoreSeries(void *objPtr, uint32_t core)
{
	HudCpuActDataSrc *s = (HudCpuActDataSrc *)objPtr;
	if (core > 3U)
		return NULL;
	return &s->coreSeries[core];
}

void *hudCpuActSrcGetTotalSeries(void *objPtr)
{
	return &((HudCpuActDataSrc *)objPtr)->totalSeries;
}

void *hudCpuActSrcGetFpsSeries(void *objPtr)
{
	return &((HudCpuActDataSrc *)objPtr)->fpsSeries;
}

void *hudCpuActSrcDtor(void *objPtr)
{
	HudCpuActDataSrc *s = (HudCpuActDataSrc *)objPtr;
	int i;
	s->vtable = &g_cpuActSrcVtable;
	hudSeriesSrcDtor(&s->fpsSeries);
	hudSeriesSrcDtor(&s->totalSeries);
	for (i = 3; i >= 0; i--)
		hudSeriesSrcDtor(&s->coreSeries[i]);
	hudDataSrcBaseDtor(s);
	return s;
}

void *hudCpuActSrcDeletingDtor(void *objPtr)
{
	hudCpuActSrcDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

/* CPU User Markers data source */
void *hudCpuMarkersSrcDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_cpuMarkersSrcVtable;
	hudTimelineSrcDtor(objPtr);
	return objPtr;
}

void *hudCpuMarkersSrcDeletingDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_cpuMarkersSrcVtable;
	hudTimelineSrcDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudCpuMarkersSrcCtor(void *objPtr, void *timingPtr, uint32_t maxPackets)
{
	HudCpuMarkersDataSrc *s = (HudCpuMarkersDataSrc *)objPtr;
	HudTimingInfo *timing = (HudTimingInfo *)timingPtr;

	hudTimelineSrcCtor(&s->base);
	s->base.vtable = &g_cpuMarkersSrcVtable;
	s->maxPackets = maxPackets;
	s->timing = timing;
	sceClibMemset(s->depth, 0, sizeof(s->depth));
	sceClibMemset(s->activeEv, 0, sizeof(s->activeEv));
	s->threadIds = (uint32_t *)hudNewArray(48U);
	s->threadCount = 0;
	s->frameTimeCount = 0;
	s->bufTooSmallPrinted = 0;
	s->warnCountdown = 20U;
	timing->cpuScale = 1.0f;
	hudTimelineSrcSetTrackCount(&s->base, 1U);
	hudTimelineSrcSetMinSpan(&s->base, 0.05f);
	return s;
}

void hudSortU32(uint32_t *arr, uint32_t lo, uint32_t hi)
{
	uint32_t mid = (lo + hi) >> 1;
	uint32_t pivot = arr[mid];
	uint32_t i, store;

	arr[mid] = arr[hi];
	arr[hi] = pivot;
	if (lo >= hi) {
		uint32_t t = arr[hi];
		arr[hi] = arr[lo];
		arr[lo] = t;
		return;
	}
	store = lo;
	for (i = lo; i < hi; i++) {
		if (arr[i] <= pivot) {
			uint32_t t = arr[store];
			arr[store] = arr[i];
			arr[i] = t;
			store++;
		}
	}
	{
		uint32_t t = arr[hi];
		arr[hi] = arr[store];
		arr[store] = t;
	}
	if (lo < store)
		hudSortU32(arr, lo, store - 1U);
	if (hi > store)
		hudSortU32(arr, store + 1U, hi);
}

typedef struct HudUserMarkerPkt {
	uint32_t header;
	uint32_t threadId;
	uint32_t depth;
	uint32_t color;
	uint64_t timestamp;
} HudUserMarkerPkt;

int hudCpuMarkersSrcCollect(void *objPtr)
{
	HudCpuMarkersDataSrc *s = (HudCpuMarkersDataSrc *)objPtr;
	HudTimingInfo *tm = s->timing;
	void *buf = NULL;
	int count, i;
	uint64_t minTs = ~(uint64_t)0, maxTs = 0, firstFrameTs = 0, lastFrameTs = 0;
	uint32_t frameMarkers = 0;
	float winEndLimit;

	if (s->frameTimeCount < 128U) {
		s->frameTimes[s->frameTimeCount++] = tm->paTime;
	}
	if ((tm->frameCounter % tm->sampleInterval) != 0)
		return 0;

	hudTimelineSrcClearTracks(&s->base);
	hudTimelineSrcClearMarkers(&s->base);
	count = sceRazorCpuGetUserMarkerTraceBuffer(&buf);
	if (count > 0) {
		const HudUserMarkerPkt *pkts = (const HudUserMarkerPkt *)buf;
		void *filter = hudThreadFilterGetInstance();
		s->threadCount = 0;
		for (i = 0; i < count; i++) {
			uint32_t tid = pkts[i].threadId;
			uint32_t j;
			int found = 0;
			if (!hudThreadFilterIsEnabled(filter, (int)tid))
				continue;
			for (j = 0; j < s->threadCount; j++) {
				if (s->threadIds[j] == tid) {
					found = 1;
					break;
				}
			}
			if (!found) {
				if (s->threadCount == 12U)
					break;
				s->threadIds[s->threadCount++] = tid;
			}
		}
		if (s->threadCount != 0)
			hudSortU32(s->threadIds, 0U, s->threadCount - 1U);
		hudTimelineSrcSetTrackCount(&s->base, s->threadCount);
		s->mismatchWarn = 0;
		if (s->threadCount != 0)
			hudMemsetWrap(s->activeEv, 0, sizeof(s->activeEv));

		tm->cpuBaseUs = (uint64_t)((int64_t)pkts[0].timestamp / (int64_t)tm->tbFreq);

		for (i = 0; i < count; i++) {
			uint64_t ts = pkts[i].timestamp;
			uint32_t type = pkts[i].header & 0x30000U;
			int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
			float ms;

			if (ts < minTs) minTs = ts;
			if (ts > maxTs) maxTs = ts;

			if (type == 0x10000U) {
				if (frameMarkers == 1U) {
					firstFrameTs = ts;
					lastFrameTs = ts;
				} else if (frameMarkers == tm->displayFrames + 1U) {
					lastFrameTs = ts;
				}
				frameMarkers++;
				ms = hudI64ToFloat((int64_t)(ts / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
				hudTimelineSrcAddMarker(&s->base, ms, 0xF0FFFFFFU);
			} else if (type == 0x20000U || type == 0x30000U) {
				uint32_t tIdx;
				for (tIdx = 0; tIdx < s->threadCount; tIdx++) {
					if (s->threadIds[tIdx] == pkts[i].threadId)
						break;
				}
				if (tIdx >= s->threadCount || tIdx >= 12U)
					continue;
				ms = hudI64ToFloat((int64_t)(ts / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
				if (type == 0x20000U) {
					uint32_t d = s->depth[tIdx];
					uint32_t newDepth = pkts[i].depth + 1U;
					if (d > 9U)
						continue;
					if (newDepth == d + 1U && s->activeEv[tIdx] != NULL) {
						hudTimelineSrcEndEvent(&s->base, tIdx, ms, s->colorStack[tIdx][d - 1U]);
						s->activeEv[tIdx] = NULL;
					} else if (d != 0) {
						s->mismatchWarn = 1;
					}
					s->colorStack[tIdx][d] = pkts[i].color;
					s->depth[tIdx] = newDepth;
					s->activeEv[tIdx] = hudTimelineSrcBeginEvent(&s->base, tIdx, ms, pkts[i].color);
				} else {
					uint32_t d = s->depth[tIdx];
					if (d == 0)
						continue;
					if (pkts[i].depth + 1U == d && s->activeEv[tIdx] != NULL) {
						hudTimelineSrcEndEvent(&s->base, tIdx, ms, pkts[i].color);
						s->activeEv[tIdx] = NULL;
					} else {
						s->mismatchWarn = 1;
					}
					s->depth[tIdx] = pkts[i].depth;
					if (pkts[i].depth != 0)
						s->activeEv[tIdx] = hudTimelineSrcBeginEvent(&s->base, tIdx, ms, s->colorStack[tIdx][pkts[i].depth - 1U]);
				}
			}
		}

		if (firstFrameTs < lastFrameTs) {
			int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
			tm->cpuWinStart = hudI64ToFloat((int64_t)(firstFrameTs / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
			tm->cpuWinEnd   = hudI64ToFloat((int64_t)(lastFrameTs  / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
		} else if (minTs < maxTs) {
			int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
			tm->cpuWinStart = hudI64ToFloat((int64_t)(minTs / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
			tm->cpuWinEnd   = hudI64ToFloat((int64_t)(maxTs / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
		}
	}

	s->base.dirty = 1;
	if (s->frameTimeCount != 0) {
		uint32_t k;
		int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
		for (k = 0; k < s->frameTimeCount; k++) {
			float ms = hudI64ToFloat((int64_t)(s->frameTimes[k] / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
			hudTimelineSrcAddMarker(&s->base, ms, 0x80808080U);
		}
		if (s->frameTimeCount > 1U) {
			s->frameTimes[0] = s->frameTimes[s->frameTimeCount - 1U];
			s->frameTimeCount = 1U;
		}
	}

	winEndLimit = (tm->useCpuBase != 0) ? (tm->cpuWinEnd + tm->cpuScale) : (tm->gpuWinEnd + tm->gpuScale);
	hudTimelineSrcSetTimeWindow(&s->base,
		(tm->useCpuBase != 0) ? tm->cpuWinStart : tm->gpuWinStart,
		(tm->useCpuBase != 0) ? tm->cpuWinEnd   : tm->gpuWinEnd);

	{
		int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
		float maxMs = hudI64ToFloat((int64_t)(maxTs / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
		if (winEndLimit > maxMs && (int)(s->maxPackets >> 1) == count && s->bufTooSmallPrinted == 0) {
			if (--s->warnCountdown == 0) {
				float minMs = hudI64ToFloat((int64_t)(minTs / (uint64_t)tm->tbFreq) - baseUs) / 1000.0f;
				uint32_t req = 2U * (uint32_t)((((float)(s->maxPackets >> 1) * 1.1f) * (winEndLimit - minMs)) / (maxMs - minMs));
				sceClibPrintf("[SceRazorHud]: The user marker trace buffer size is too small for display on HUD. The graph may be misleading.\n");
				sceClibPrintf("[SceRazorHud]: Try calling sceRazorCpuHudSetUserMarkerTraceBuffer() with a larger buffer size after \n");
				sceClibPrintf("[SceRazorHud]: loading the HUD module (SCE_SYSMODULE_RAZOR_HUD).\n");
				sceClibPrintf("[SceRazorHud]: [%d] packets currently allocated\n", s->maxPackets);
				sceClibPrintf("[SceRazorHud]: [%d] packets required (approximately)\n", req);
				s->bufTooSmallPrinted = 1;
			}
		}
	}
	return 0;
}

int hudCpuMarkersSrcUpdate(void *objPtr)
{
	hudTimelineSrcReset(objPtr);
	return hudCpuMarkersSrcCollect(objPtr);
}

/* Base Timeline data source */
void *hudTimelineSrcDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_timelineSrcVtable;
	hudDataSrcBaseDtor(objPtr);
	return objPtr;
}

void *hudTimelineSrcDeletingDtor(void *objPtr)
{
	*(const HudDataSrcVtable **)objPtr = &g_timelineSrcVtable;
	hudDataSrcBaseDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudTimelineSrcCtor(void *objPtr)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	int i, j;

	hudDataSrcBaseCtor(t);
	t->vtable = &g_timelineSrcVtable;
	for (i = 0; i < 12; i++) {
		for (j = 0; j < 4096; j++) {
			t->tracks[i].events[j].flags = 0;
			t->tracks[i].events[j].startMs = 0.0f;
			t->tracks[i].events[j].endMs = 0.0f;
			t->tracks[i].events[j].color = 0;
		}
		t->tracks[i].count = 0;
	}
	for (i = 0; i < 128; i++) {
		t->markers[i].timeMs = 0.0f;
		t->markers[i].color = 0;
	}
	t->trackCount = 0;
	t->markerCount = 0;
	t->windowStart = 0.0f;
	t->windowEnd = 1.0f;
	t->minSpan = 0.0f;
	return t;
}

void hudTimelineSrcSetMinSpan(void *objPtr, float val)
{
	((HudTimelineBase *)objPtr)->minSpan = val;
}

void hudTimelineSrcSetTrackCount(void *objPtr, uint32_t count)
{
	((HudTimelineBase *)objPtr)->trackCount = count;
}

uint32_t hudTimelineSrcGetTrackCount(void *objPtr)
{
	return ((HudTimelineBase *)objPtr)->trackCount;
}

uint32_t hudTimelineSrcGetMarkerCount(void *objPtr)
{
	return ((HudTimelineBase *)objPtr)->markerCount;
}

uint32_t hudTimelineSrcGetEventCount(void *objPtr, uint32_t track)
{
	return ((HudTimelineBase *)objPtr)->tracks[track].count;
}

void *hudTimelineSrcGetMarker(void *objPtr, uint32_t idx)
{
	return &((HudTimelineBase *)objPtr)->markers[idx];
}

void *hudTimelineSrcGetEvent(void *objPtr, uint32_t track, uint32_t idx)
{
	return &((HudTimelineBase *)objPtr)->tracks[track].events[idx];
}

void *hudTimelineSrcAddMarker(void *objPtr, float timeMs, uint32_t color)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	HudTimelineMarker *m = &t->markers[t->markerCount++];
	m->timeMs = timeMs;
	m->color = color;
	return m;
}

void *hudTimelineSrcBeginEvent(void *objPtr, uint32_t track, float timeMs, uint32_t color)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	HudTimelineTrack *tr;
	HudTimelineEvent *ev;

	if (track >= t->trackCount)
		return NULL;
	tr = &t->tracks[track];
	if (tr->count >= 4096U)
		return NULL;
	if (tr->count != 0) {
		ev = &tr->events[tr->count - 1U];
		if (ev->color == color && (timeMs - ev->endMs) < t->minSpan) {
			ev->flags = 1U;
			ev->endMs = timeMs;
			return ev;
		}
	}
	ev = &tr->events[tr->count++];
	ev->flags = 1U;
	ev->startMs = timeMs;
	ev->endMs = timeMs;
	ev->color = color;
	return ev;
}

__attribute__((optimize("no-partial-inlining")))
void hudTimelineSrcEndEvent(void *objPtr, uint32_t track, float timeMs, uint32_t color)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	HudTimelineTrack *tr;
	HudTimelineEvent *ev;

	if (track >= t->trackCount)
		return;
	tr = &t->tracks[track];
	if (tr->count == 0) {
		ev = &tr->events[0];
		tr->count = 1U;
		ev->flags = 0;
		ev->startMs = 0.0f;
		ev->endMs = 0.0f;
		ev->color = color;
	}
	ev = &tr->events[tr->count - 1U];
	ev->flags |= 2U;
	ev->endMs = timeMs;
	if (timeMs < ev->startMs) {
		tr->count--;
		return;
	}
	if (tr->count > 1U) {
		HudTimelineEvent *prev = &tr->events[tr->count - 2U];
		if ((timeMs - prev->startMs) < t->minSpan) {
			if ((timeMs - ev->startMs) > (prev->endMs - prev->startMs))
				prev->color = ev->color;
			tr->count--;
		}
	}
}

void hudTimelineSrcClearMarkers(void *objPtr)
{
	((HudTimelineBase *)objPtr)->markerCount = 0;
}

void hudTimelineSrcClearTracks(void *objPtr)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	uint32_t i;
	for (i = 0; i < t->trackCount && i < 12U; i++)
		t->tracks[i].count = 0;
}

void hudTimelineSrcGetTimeWindow(void *objPtr, float *outStart, float *outEnd)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	*outStart = t->windowStart;
	*outEnd = t->windowEnd;
}

void hudTimelineSrcSetTimeWindow(void *objPtr, float start, float end)
{
	HudTimelineBase *t = (HudTimelineBase *)objPtr;
	t->windowStart = start;
	t->windowEnd = end;
}

void hudTimelineSrcReset(void *objPtr)
{
	hudDataSrcBaseReset(objPtr);
}

/* GPU Timeline data source */
void *hudGpuTimelineSrcDtor(void *objPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	s->base.vtable = &g_gpuTimelineSrcVtable;
	hudPoolFree(0U, s->liveBufs[0]);
	hudPoolFree(0U, s->liveBufs[1]);
	hudTimelineSrcDtor(&s->base);
	return s;
}

void *hudGpuTimelineSrcDeletingDtor(void *objPtr)
{
	hudGpuTimelineSrcDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudGpuTimelineSrcCtor(void *objPtr, void *timingPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	HudTimingInfo *tm = (HudTimingInfo *)timingPtr;
	int ret;

	hudTimelineSrcCtor(&s->base);
	s->base.vtable = &g_gpuTimelineSrcVtable;
	s->timing = tm;
	s->vertexTrackBase = 0;
	s->fragTrackBase = 1U;
	s->fwTrackBase = 2U;
	s->numCores = 1U;
	s->numFw = 1U;
	hudTimelineSrcSetTrackCount(&s->base, 3U);
	s->prevPaTimeUs = 0;
	s->curPaTimeUs = 0;
	tm->gpuScale = 1.0f;
	s->frameStartCount = 0;
	s->paTimeUs = 0;
	s->lastFrameMs = 0.0f;
	s->logEnabled = 0;
	s->kickCount = 0;
	s->sceneColorMode = 0;
	s->paramBufUsed = 0;
	s->processId = sceKernelGetProcessId();
	sceClibMemset(s->lastVertexFrame, 0, sizeof(s->lastVertexFrame));
	sceClibMemset(s->lastFragFrame, 0, sizeof(s->lastFragFrame));
	s->gpuFreq = hudGpuGetClockFreq();
	s->liveBufIdx = 0;
	s->liveBufs[0] = hudPoolAlloc(0U, 0x100000U, 8U);
	s->liveBufs[1] = hudPoolAlloc(0U, 0x100000U, 8U);
	sceClibMemset(&s->liveRes, 0, sizeof(s->liveRes));

	hudLiveStopInternal(1);
	ret = hudLiveSetMetricsGroupInternal(1, 0U);
	if (ret != 0) {
		hudGxmFinishInternal();
		sceClibPrintf("[SceRazorHud]:Could not setup GPU Live Metrics for HUD (0x%08x-%x)\n", ret, 1);
	} else {
		hudLiveStartInternal(1);
		hudGxmFinishInternal();
		hudLiveSetBufferInternal(1, s->liveBufs[s->liveBufIdx], 0x100000U, NULL);
	}
	return s;
}

void hudGpuTimelineNull1(void) {}
void hudGpuTimelineNull2(void) {}
void hudGpuTimelineNull4(void) {}
void hudGpuTimelineNull3(void) {}

void hudGpuTimelineSrcSetCores(void *objPtr, uint32_t cores, uint32_t fw)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	if (cores != 1U && cores != 4U)
		cores = 1U;
	if (fw > 1U && fw != 4U)
		fw = 1U;
	s->numCores = cores;
	s->numFw = fw;
	s->vertexTrackBase = 0;
	s->fragTrackBase = cores;
	s->fwTrackBase = cores * 2U;
	hudTimelineSrcSetTrackCount(&s->base, cores * 2U + fw);
}

void hudGpuTimelineSrcSetColorMode(void *objPtr, uint32_t mode)
{
	((HudGpuTimelineDataSrc *)objPtr)->sceneColorMode = mode;
}

void hudGpuTimelineSrcSetLogEnabled(void *objPtr, uint8_t enabled)
{
	((HudGpuTimelineDataSrc *)objPtr)->logEnabled = enabled;
}

uint32_t hudGpuTimelineSrcGetKickCount(void *objPtr)
{
	return ((HudGpuTimelineDataSrc *)objPtr)->kickCount;
}

void *hudGpuTimelineSrcGetKick(void *objPtr, uint32_t idx)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	if (idx >= s->kickCount)
		return NULL;
	return &s->kicks[idx];
}

void hudGpuTimelineSrcProcess(void *objPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	HudTimingInfo *tm = s->timing;
	const uint8_t *p = (const uint8_t *)s->liveRes.resultData;
	uint32_t i;
	int firstJob = 1;

	hudTimelineSrcClearTracks(&s->base);
	s->frameSeq++;
	if (p != NULL) {
		s->kickCount = 0;
		for (i = 0; i < s->liveRes.entryCount; i++) {
			const SceRazorGpuLiveEntryHeader *hdr = (const SceRazorGpuLiveEntryHeader *)p;
			if (hdr->entryType == 1) {
				const SceRazorGpuLiveEntryParameterBuffer *pb = (const SceRazorGpuLiveEntryParameterBuffer *)p;
				s->paramBufUsed = pb->peakUsage;
			} else if (hdr->entryType == 0) {
				const SceRazorGpuLiveEntryJob *job = (const SceRazorGpuLiveEntryJob *)p;
				int64_t baseUs;
				if (firstJob) {
					tm->gpuBaseUs = job->startTime;
					firstJob = 0;
				}
				baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
				if (job->type == 0) {
					if ((uint32_t)job->core < s->numFw) {
						uint32_t track = s->fwTrackBase + (uint32_t)job->core;
						float st = hudI64ToFloat((int64_t)job->startTime - baseUs) / 1000.0f;
						float en = hudI64ToFloat((int64_t)job->endTime - baseUs) / 1000.0f;
						hudTimelineSrcBeginEvent(&s->base, track, st, 0xFF808FA0U);
						hudTimelineSrcEndEvent(&s->base, track, en, 0xFF808FA0U);
					}
				} else if (job->type == 1 || job->type == 7) {
					if ((uint32_t)job->core < s->numCores) {
						int otherPid = ((int32_t)job->processId != s->processId);
						int newFrame = (!otherPid && job->frameNumber != s->lastVertexFrame[job->core]);
						uint32_t color;
						float st, en;
						if (newFrame)
							s->lastVertexFrame[job->core] = job->frameNumber;
						if (s->sceneColorMode == 1U)
							color = otherPid ? 0xFFA0A0A0U : g_sceneColors[job->sceneIndex & 0xFU];
						else if ((job->flags & 1U) != 0)
							color = 0xFF2020FFU;
						else if (newFrame)
							color = 0xFFD0D0FFU;
						else
							color = otherPid ? 0xFFA0A0A0U : 0xFF8080FFU;
						st = hudI64ToFloat((int64_t)job->startTime - baseUs) / 1000.0f;
						en = hudI64ToFloat((int64_t)job->endTime - baseUs) / 1000.0f;
						hudTimelineSrcBeginEvent(&s->base, s->vertexTrackBase + (uint32_t)job->core, st, color);
						hudTimelineSrcEndEvent(&s->base, s->vertexTrackBase + (uint32_t)job->core, en, color);
						if (s->logEnabled && (job->frameNumber % 10U) == 0 && s->kickCount < 16384U) {
							HudGpuKickRecord *kr = &s->kicks[s->kickCount++];
							s->base.dirty = 1;
							kr->type = 0;
							kr->ms = en - st;
							kr->frame = job->frameNumber;
							kr->core = (uint32_t)job->core;
						}
					}
				} else if (job->type == 2 || job->type == 8) {
					if ((uint32_t)job->core < s->numCores) {
						int otherPid = ((int32_t)job->processId != s->processId);
						int newFrame = (!otherPid && job->frameNumber != s->lastFragFrame[job->core]);
						uint32_t color;
						float st, en;
						if (newFrame) {
							if (job->core == 0 && s->frameStartCount < 256U)
								s->frameStarts[s->frameStartCount++] = job->startTime;
							s->lastFragFrame[job->core] = job->frameNumber;
						}
						if (s->sceneColorMode == 1U)
							color = otherPid ? 0xFFA0A0A0U : g_sceneColors[job->sceneIndex & 0xFU];
						else if ((job->flags & 1U) != 0)
							color = 0xFF2020FFU;
						else if (newFrame)
							color = 0xFFFFD0D0U;
						else
							color = otherPid ? 0xFFA0A0A0U : 0xFFFF8080U;
						st = hudI64ToFloat((int64_t)job->startTime - baseUs) / 1000.0f;
						en = hudI64ToFloat((int64_t)job->endTime - baseUs) / 1000.0f;
						hudTimelineSrcBeginEvent(&s->base, s->fragTrackBase + (uint32_t)job->core, st, color);
						hudTimelineSrcEndEvent(&s->base, s->fragTrackBase + (uint32_t)job->core, en, color);
						if (s->logEnabled && (job->frameNumber % 10U) == 0 && s->kickCount < 16384U) {
							HudGpuKickRecord *kr = &s->kicks[s->kickCount++];
							s->base.dirty = 1;
							kr->type = 1U;
							kr->ms = en - st;
							kr->frame = job->frameNumber;
							kr->core = (uint32_t)job->core;
						}
					}
				}
			}
			p += hdr->entrySize;
		}
	}

	hudTimelineSrcClearMarkers(&s->base);
	{
		int64_t baseUs = (tm->useCpuBase != 0) ? (int64_t)tm->cpuBaseUs : (int64_t)tm->gpuBaseUs;
		for (i = 0; i < s->frameStartCount; i++) {
			float ms = hudI64ToFloat((int64_t)s->frameStarts[i] - baseUs) / 1000.0f;
			hudTimelineSrcAddMarker(&s->base, ms, 0xF0FFFFFFU);
		}
		if (s->frameStartCount > tm->displayFrames + 3U) {
			tm->gpuWinStart = hudI64ToFloat((int64_t)s->frameStarts[2] - baseUs) / 1000.0f;
			tm->gpuWinEnd   = hudI64ToFloat((int64_t)s->frameStarts[tm->displayFrames + 2U] - baseUs) / 1000.0f;
		} else {
			tm->gpuWinStart = hudI64ToFloat((int64_t)s->prevPaTimeUs - baseUs) / 1000.0f;
			tm->gpuWinEnd   = hudI64ToFloat((int64_t)s->curPaTimeUs  - baseUs) / 1000.0f;
		}
		if (s->frameStartCount > 1U) {
			float f0 = hudI64ToFloat((int64_t)s->frameStarts[0] - baseUs) / 1000.0f;
			float f1 = hudI64ToFloat((int64_t)s->frameStarts[1] - baseUs) / 1000.0f;
			s->lastFrameMs = f1 - f0;
		} else {
			s->lastFrameMs = 0.0f;
		}
		hudTimelineSrcSetTimeWindow(&s->base, tm->gpuWinStart, tm->gpuWinEnd);
	}
}

uint32_t hudGpuTimelineSrcGetFrameSeq(void *objPtr)
{
	return ((HudGpuTimelineDataSrc *)objPtr)->frameSeq;
}

float hudGpuTimelineSrcSumVertexMs(void *objPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	uint32_t count = hudTimelineSrcGetEventCount(&s->base, s->vertexTrackBase);
	uint32_t i;
	float total = 0.0f;
	for (i = 0; i < count; i++) {
		HudTimelineEvent *ev = (HudTimelineEvent *)hudTimelineSrcGetEvent(&s->base, s->vertexTrackBase, i);
		total += (ev->endMs - ev->startMs);
	}
	return total;
}

float hudGpuTimelineSrcSumFragmentMs(void *objPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	uint32_t count = hudTimelineSrcGetEventCount(&s->base, s->fragTrackBase);
	uint32_t i;
	float total = 0.0f;
	for (i = 0; i < count; i++) {
		HudTimelineEvent *ev = (HudTimelineEvent *)hudTimelineSrcGetEvent(&s->base, s->fragTrackBase, i);
		total += (ev->endMs - ev->startMs);
	}
	return total;
}

float hudGpuTimelineSrcGetFrameMs(void *objPtr)
{
	return ((HudGpuTimelineDataSrc *)objPtr)->lastFrameMs;
}

uint32_t hudGpuTimelineSrcGetParamBufferUsed(void *objPtr)
{
	return ((HudGpuTimelineDataSrc *)objPtr)->paramBufUsed;
}

void hudGpuTimelineSrcUpdate(void *objPtr)
{
	HudGpuTimelineDataSrc *s = (HudGpuTimelineDataSrc *)objPtr;
	HudTimingInfo *tm = s->timing;

	hudTimelineSrcReset(&s->base);
	if ((tm->frameCounter % tm->sampleInterval) != 0)
		return;

	s->prevPaTimeUs = s->curPaTimeUs;
	s->curPaTimeUs = (uint64_t)((int64_t)tm->paTime / (int64_t)tm->tbFreq);
	s->base.dirty = 1;
	if (hudLiveSetBufferInternal(1, s->liveBufs[s->liveBufIdx], 0x100000U, &s->liveRes) != 0)
		s->liveRes.resultData = NULL;
	if (s->liveRes.overflowCount != 0)
		s->liveRes.entryCount = 0;
	s->liveBufIdx = (s->liveBufIdx + 1U) % 2U;
	s->gpuFreq = hudGpuGetClockFreq();
	hudTimelineSrcSetMinSpan(&s->base, 0.05f);
	hudGpuTimelineSrcProcess(s);
	if (s->frameStartCount > 1U) {
		s->frameStarts[0] = s->frameStarts[s->frameStartCount - 1U];
		s->frameStartCount = 1U;
	}
}

float hudCpuMarkersGetWindowScale(void *objPtr)
{
	HudTimingInfo *tm = ((HudCpuMarkersDataSrc *)objPtr)->timing;
	return (tm->useCpuBase != 0) ? tm->cpuScale : tm->gpuScale;
}

float hudTimelineGetWindowScale(void *objPtr)
{
	(void)objPtr;
	return 0.0f;
}

float hudGpuTimelineGetWindowScale(void *objPtr)
{
	HudTimingInfo *tm = ((HudGpuTimelineDataSrc *)objPtr)->timing;
	return (tm->useCpuBase != 0) ? tm->cpuScale : tm->gpuScale;
}
