/*
 * HUD manager lifecycle, GXM shader patcher, 2D primitive/font batch renderer,
 * and Line/Bar/Timeline overlay widgets.
 */
#include "razor_hud_internal.h"
#include "hud_assets.h"

typedef struct HudDrawContext {
	SceGxmContext *context;
	SceGxmVertexProgram *vertexProgram;
	SceGxmFragmentProgram *colorFragProgram;
	SceGxmFragmentProgram *texFragProgram;
	const SceGxmProgramParameter *wvpParam;
	const float *wvpMatrix;
} HudDrawContext;

typedef struct HudWidgetVtable {
	void *(*dtor)(void *self);
	void *(*deletingDtor)(void *self);
	void (*render)(void *self, void *drawCtx);
	void (*build)(void *self);
} HudWidgetVtable;

typedef struct HudVertex {
	float x;
	float y;
	uint32_t color;
	uint16_t u;
	uint16_t v;
} HudVertex;

typedef struct HudDrawCmd {
	uint32_t primType;
	uint32_t indexStart;
	uint32_t indexCount;
	uint32_t textureIdx;
} HudDrawCmd;

typedef struct HudWidgetBase {
	const HudWidgetVtable *vtable; /* 0 */
	void *dataSource;              /* 4 */
	uint8_t enabled;               /* 8 */
	uint8_t enabledPad[7];
	uint32_t textColor;            /* 16 */
	float fontWidth;               /* 20 */
	float fontHeight;              /* 24 */
	HudVertex *vb[2];              /* 28..35 */
	uint16_t *ib[2];               /* 36..43 */
	uint32_t bufIdx;               /* 44 */
	uint32_t vertCount;            /* 48 */
	HudDrawCmd cmds[1000];         /* 52..16051 */
	uint32_t cmdCount;             /* 16052 */
	float x;                       /* 16056 */
	float y;                       /* 16060 */
	float w;                       /* 16064 */
	float h;                       /* 16068 */
	float titleFontSize;           /* 16072 */
	char title[32];                /* 16076..16107 */
	uint32_t titleStyle;           /* 16108 */
	uint32_t titleBgColor;         /* 16112 */
	uint32_t titleFgColor;         /* 16116 */
} HudWidgetBase;

typedef struct HudBarGraphWidget {
	HudWidgetBase base;            /* 0..16119 */
	uint32_t mode;                 /* 16120 */
} HudBarGraphWidget;

typedef struct HudTimelineWidget {
	HudWidgetBase base;            /* 0..16119 */
	uint32_t rowLabels[30];        /* 16120..16239 */
	uint32_t timelinePad;             /* 16240 */
	uint32_t autoHeight;           /* 16244 */
	float rowHeight;               /* 16248 */
} HudTimelineWidget;

static const HudWidgetVtable g_widgetBaseVtable = {
	hudWidgetBaseDtor,
	hudWidgetBaseDeletingDtor,
	hudWidgetBaseRender,
	hudWidgetDrawFrame,
};

static const HudWidgetVtable g_lineGraphVtable = {
	hudLineGraphDtor,
	hudLineGraphDeletingDtor,
	hudWidgetBaseRender,
	hudLineGraphBuild,
};

static const HudWidgetVtable g_barGraphVtable = {
	hudBarGraphDtor,
	hudBarGraphDeletingDtor,
	hudWidgetBaseRender,
	hudBarGraphBuild,
};

static const HudWidgetVtable g_timelineWidgetVtable = {
	hudTimelineWidgetDtor,
	hudTimelineWidgetDeletingDtor,
	hudWidgetBaseRender,
	hudTimelineWidgetBuild,
};

static inline __attribute__((always_inline)) uint32_t hudFloatBits(float f)
{
	union { float f; uint32_t u; } v;
	v.f = f;
	return v.u;
}

static inline __attribute__((always_inline)) uint16_t hudFloatToHalf(uint32_t fbits)
{
	uint32_t exp = (fbits >> 23) & 0xFFU;
	uint32_t sign = (fbits >> 16) & 0x8000U;
	uint32_t mant = fbits & 0x7FFFFFU;
	int e;
	uint32_t mask, rem, half;

	if (exp == 255)
		return (uint16_t)(sign | (mant >> 13) | 0x7E00U);
	if (exp == 0 && mant == 0)
		return (uint16_t)sign;

	e = (int)exp - 127;
	mant |= 0x800000U;
	if (e < -14)
		mask = (e < -25) ? 0xFFFFFFU : (0xFFFFFFU >> (exp - 102));
	else
		mask = 0x1FFFU;

	rem = mask & mant;
	if (rem != 0) {
		half = (mask + 1U) >> 1;
		if (rem == half)
			half = mant & (half * 2U);
		mant += half;
		if (mant >= 0x1000000U) {
			e = (int)exp - 126;
			mant >>= 1;
		}
	}
	if (e > 15)
		return (uint16_t)(sign | 0x7C00U);
	if (e < -24)
		return (uint16_t)sign;
	if (e < -14) {
		mant >>= (-14 - e);
		return (uint16_t)((mant >> 13) | sign);
	}
	return (uint16_t)((((uint32_t)(e + 14) << 10) + (mant >> 13)) | sign);
}

static inline __attribute__((always_inline)) void hudWidgetEnsureCmd(HudWidgetBase *w, uint32_t primType, uint32_t texIdx)
{
	if (w->cmdCount == 0 ||
	    w->cmds[w->cmdCount - 1U].primType != primType ||
	    w->cmds[w->cmdCount - 1U].textureIdx != texIdx) {
		HudDrawCmd *c = &w->cmds[w->cmdCount++];
		c->primType = primType;
		c->indexStart = w->vertCount;
		c->indexCount = 0;
		c->textureIdx = texIdx;
	}
}

static inline __attribute__((always_inline)) void hudWidgetEmitVert(HudWidgetBase *w, float x, float y, uint32_t color, uint16_t u, uint16_t v)
{
	uint32_t idx = w->vertCount;
	if (idx <= 0x3A97U) {
		HudVertex *vx;
		w->ib[w->bufIdx][idx] = (uint16_t)idx;
		vx = &w->vb[w->bufIdx][idx];
		vx->x = x;
		vx->y = y;
		vx->color = color;
		vx->u = u;
		vx->v = v;
		w->vertCount = idx + 1U;
		if (w->cmdCount != 0)
			w->cmds[w->cmdCount - 1U].indexCount++;
	}
}

void *hudPatcherHostAlloc(void *userData, uint32_t size)
{
	(void)userData;
	return hudPoolAlloc(0U, size, 4U);
}

void hudPatcherHostFree(void *userData, void *mem)
{
	(void)userData;
	hudPoolFree(0U, mem);
}

void *hudMgrCtor(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	SceGxmContextParams ctxParams;

	m->paTime = 0;
	m->tbFreq = sceKernelPaGetTimebaseFrequency();
	m->tbFreqHi = 0;
	m->gpuBaseUs = 0;
	m->cpuBaseUs = 0;
	m->gpuWinStart = 0.0f;
	m->gpuWinEnd = 0.0f;
	m->cpuWinStart = 0.0f;
	m->cpuWinEnd = 0.0f;
	m->cpuScale = 0.0f;
	m->gpuScale = 0.0f;
	m->frameCounter = 19U;
	m->orthoRight = 1.0f;
	m->orthoTop = 1.0f;
	m->orthoBottom = 0.0f;
	m->orthoLeft = 0.0f;
	m->displayFrames = 1U;
	m->useCpuBase = 0;
	m->flags = 0;
	m->gxmHostMem = NULL;
	m->gxmContext = NULL;
	m->patcher = NULL;
	m->fpColorId = NULL;
	m->fpTexId = NULL;
	m->msaaMode = 0;
	m->gpuFrameSrc = NULL;
	m->cpuActSrc = NULL;
	m->gpuBufSrc = NULL;
	m->cpuMarkersSrc = NULL;
	m->gpuTimelineSrc = NULL;
	m->sampleInterval = 20U;
	sceClibMemset(m->traceCounters, 0, sizeof(m->traceCounters));
	m->traceRate = 0;
	m->traceMask = ~(uint64_t)0;
	m->traceNeedStart = 1;
	m->traceBufSize = 0;
	m->traceStopFrame = 0;
	m->traceTriggered = 0;
	m->fpTex = NULL;
	m->fpColor = NULL;
	m->vp = NULL;
	m->rt = NULL;
	m->txtLogView = NULL;
	m->csvLogView = NULL;
	m->xmlLogView = NULL;
	m->gpuFrameWidget = NULL;
	m->cpuUtilWidget = NULL;
	m->cpuFpsWidget = NULL;
	m->gpuBufWidget = NULL;
	m->gpuTimelineWidget = NULL;
	m->cpuMarkersWidget = NULL;
	m->wvpParam = NULL;
	m->vpId = NULL;
	m->swapCount = 0;
	sceClibMemset(m->views, 0, sizeof(m->views));
	m->viewCount = 0;
	sceClibMemset(m->dataSources, 0, sizeof(m->dataSources));
	m->dataSourceCount = 0;
	sceClibMemset(m->notify, 0, sizeof(m->notify));
	m->traceCountLen = 3U;
	m->activityPackets = 4096U;
	m->userMarkerPackets = 16384U;
	m->displayEnabled = 1;
	m->gxmVerMinor = 0;
	m->perfMode = 1;
	m->txtFilename[0] = '\0';
	m->traceFilename[0] = '\0';
	m->userMarkerBuf = NULL;
	m->activityBuf = NULL;
	m->perfLoaded = 0;
	m->widgetsBuilt = 0;
	m->paramBufSize = 0;
	m->gxmVerMajor = 0;
	m->cpuCoreWidgets[0] = NULL;
	m->cpuCoreWidgets[1] = NULL;
	m->cpuCoreWidgets[2] = NULL;
	m->cpuCoreWidgets[3] = NULL;

	m->gxmHostMem = hudPoolAlloc(0U, 2048U, 4U);
	sceClibMemset(&ctxParams, 0, sizeof(ctxParams));
	ctxParams.hostMem = m->gxmHostMem;
	ctxParams.hostMemSize = 2048U;
	ctxParams.vdmRingBufferMem = hudGetVdmRingMem();
	ctxParams.vdmRingBufferMemSize = hudGetVdmRingSize();
	ctxParams.vertexRingBufferMem = hudGetVertexRingMem();
	ctxParams.vertexRingBufferMemSize = hudGetVertexRingSize();
	ctxParams.fragmentRingBufferMem = hudGetFragmentRingMem();
	ctxParams.fragmentRingBufferMemSize = hudGetFragmentRingSize();
	ctxParams.fragmentUsseRingBufferMem = hudGetFragmentUsseRingMem();
	ctxParams.fragmentUsseRingBufferMemSize = hudGetFragmentUsseRingSize();
	ctxParams.fragmentUsseRingBufferOffset = hudGetFragmentUsseRingOffset();

	m->rtWidth = 960U;
	m->rtHeight = 544U;

	m->paMgr = hudNew(1060U);
	hudPaCtor(m->paMgr, m);
	m->liveSession = hudNew(8U);
	hudLiveCtor(m->liveSession, m->paMgr);

	m->clipXMin = 0;
	m->clipYMin = 0;
	m->clipXMax = 0;
	m->clipYMax = 0;
	sceGxmCreateContext(&ctxParams, &m->gxmContext);
	sceClibMemset(m->hostConfigPath, 0, sizeof(m->hostConfigPath));
	m->savedConfig = NULL;
	if (m->gxmContext != NULL) {
		uint32_t *nbase = (uint32_t *)SceGxmInternal_338E3186();
		m->notify[0].address = nbase;
		m->notify[0].value = 0;
		m->notify[1].address = nbase + 1;
		m->notify[1].value = 0;
	}
	return m;
}

void *hudMgrDtor(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	if (m->liveSession != NULL) {
		hudLiveDtor(m->liveSession);
		hudDelete(m->liveSession);
		m->liveSession = NULL;
	}
	if (m->paMgr != NULL) {
		hudPaDtor(m->paMgr);
		hudDelete(m->paMgr);
		m->paMgr = NULL;
	}
	hudDelete(m->savedConfig);
	m->savedConfig = NULL;
	sceGxmFinish(m->gxmContext);
	sceGxmDestroyContext(m->gxmContext);
	hudPoolFree(0U, m->gxmHostMem);
	return m;
}

uint64_t hudMgrRefreshTime(void)
{
	return hudGetProcessTimeAndStore();
}

void hudMgrSetFlags(void *mgrPtr, uint32_t flags)
{
	((HudManager *)mgrPtr)->flags = flags;
}

uint32_t hudMgrGetFlags(void *mgrPtr)
{
	return ((HudManager *)mgrPtr)->flags;
}

int hudMgrStartCpuPerf(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	int ret;

	if (m->perfMode != 4)
		return HUD_ERR_EINVAL;
	if (m->activityBuf == NULL) {
		m->activityBuf = hudPoolAlloc(0U, m->activityPackets * 40U, 8U);
		if (m->activityBuf == NULL)
			return HUD_ERR_ENOMEM;
		ret = sceRazorCpuStartActivityMonitor(m->activityBuf, m->activityPackets * 40U);
		if (ret < 0)
			return ret;
	}
	if (m->userMarkerBuf == NULL && m->perfMode == 4) {
		m->userMarkerBuf = hudPoolAlloc(0U, m->userMarkerPackets * 24U, 8U);
		if (m->userMarkerBuf != NULL) {
			if (g_userMarkerBufCustom != 0) {
				sceRazorCpuStartUserMarkerTrace(g_userMarkerBufPtr, g_userMarkerBufSize);
				m->userMarkerPackets = g_userMarkerBufSize / 24U;
			} else {
				sceRazorCpuStartUserMarkerTrace(m->userMarkerBuf, m->userMarkerPackets * 24U);
			}
		}
	}
	return 0;
}

int hudMgrSetUserMarkerBuffer(void *buf, uint32_t size)
{
	if (g_userMarkerBufCustom != 0 || buf == NULL || ((uintptr_t)buf & 7U) != 0)
		return HUD_ERR_EINVAL;
	if (size < 0xC00U)
		return HUD_ERR_ENOMEM;
	g_userMarkerBufPtr = buf;
	g_userMarkerBufSize = size;
	g_userMarkerBufCustom = 1;
	return 0;
}

void hudMgrLayoutWidgets(void *mgrPtr, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax)
{
	HudManager *m = (HudManager *)mgrPtr;
	float x = (float)xMin;
	float y = (float)yMin;
	float w = (float)(xMax - xMin);
	float h = (float)(yMax - yMin);
	int i;

	if ((m->flags & 0x50U) == 0)
		return;
	if (m->clipXMin == xMin && m->clipYMin == yMin && m->clipXMax == xMax && m->clipYMax == yMax)
		return;

	if (m->gpuFrameWidget != NULL)
		hudWidgetSetRect(m->gpuFrameWidget, x + w * 0.02f, y + h * 0.04f, w * 0.30f, h * 0.22f);
	if (m->cpuUtilWidget != NULL)
		hudWidgetSetRect(m->cpuUtilWidget, x + w * 0.35f, y + h * 0.04f, w * 0.30f, h * 0.22f);
	if (m->cpuFpsWidget != NULL)
		hudWidgetSetRect(m->cpuFpsWidget, x + w * 0.68f, y + h * 0.04f, w * 0.30f, h * 0.22f);
	for (i = 0; i < 4; i++) {
		if (m->cpuCoreWidgets[i] != NULL)
			hudWidgetSetRect(m->cpuCoreWidgets[i], x + w * (0.02f + 0.24f * (float)i), y + h * 0.30f, w * 0.22f, h * 0.16f);
	}
	if (m->gpuBufWidget != NULL)
		hudWidgetSetRect(m->gpuBufWidget, x + w * 0.02f, y + h * 0.30f, w * 0.30f, h * 0.22f);
	if (m->gpuTimelineWidget != NULL) {
		float tlH = h * 0.18f;
		if (m->gpuTimelineSrc != NULL) {
			uint32_t tracks = hudTimelineSrcGetTrackCount(m->gpuTimelineSrc);
			if (tracks != 0)
				tlH = (float)tracks * 14.0f + 16.0f;
		}
		hudWidgetSetRect(m->gpuTimelineWidget, x + w * 0.02f, y + h * 0.72f, w * 0.96f, tlH);
	}
	if (m->cpuMarkersWidget != NULL)
		hudWidgetSetRect(m->cpuMarkersWidget, x + w * 0.02f, y + h * 0.50f, w * 0.96f, h * 0.20f);

	m->clipXMin = xMin;
	m->clipYMin = yMin;
	m->clipXMax = xMax;
	m->clipYMax = yMax;
}

static inline __attribute__((always_inline)) void hudDestroyObj(void **slot)
{
	if (*slot != NULL) {
		const HudWidgetVtable *vt = *(const HudWidgetVtable **)(*slot);
		vt->deletingDtor(*slot);
		*slot = NULL;
	}
}

void hudMgrDestroyWidgets(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	int i;

	m->widgetsBuilt = 0;
	m->viewCount = 0;
	m->dataSourceCount = 0;
	hudDestroyObj(&m->xmlLogView);
	hudDestroyObj(&m->csvLogView);
	hudDestroyObj(&m->txtLogView);
	hudDestroyObj(&m->gpuBufWidget);
	hudDestroyObj(&m->cpuUtilWidget);
	hudDestroyObj(&m->cpuFpsWidget);
	hudDestroyObj(&m->gpuFrameWidget);
	hudDestroyObj(&m->gpuTimelineWidget);
	hudDestroyObj(&m->cpuMarkersWidget);
	hudDestroyObj(&m->gpuBufSrc);
	hudDestroyObj(&m->gpuFrameSrc);
	hudDestroyObj(&m->gpuTimelineSrc);
	hudDestroyObj(&m->cpuActSrc);
	for (i = 0; i < 4; i++)
		hudDestroyObj(&m->cpuCoreWidgets[i]);
	m->clipXMin = 0;
	m->clipYMin = 0;
	m->clipXMax = 0;
	m->clipYMax = 0;
}

void hudMgrRebuildWidgets(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	uint32_t counters[16];
	int i;

	hudMgrDestroyWidgets(m);
	if ((m->flags & 1U) == 0)
		return;
	m->widgetsBuilt = 1;

	if (m->perfLoaded == 0 && (uint32_t)(m->perfMode - 2) <= 2U) {
		int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_PERF);
		if (ret != 0)
			sceClibPrintf("[SceRazorHud]:Failed to start PERF module (0x%08x)\n", ret);
		else
			m->perfLoaded = 1;
	}

	if (m->perfMode == 4) {
		hudMgrStartCpuPerf(m);
		if (m->perfMode == 4 && (m->flags & 0x10U) != 0) {
			m->useCpuBase = 0;
			if (m->gpuTimelineSrc == NULL) {
				m->gpuTimelineSrc = hudNew(1051912U);
				hudGpuTimelineSrcCtor(m->gpuTimelineSrc, &m->paTime);
			}
			hudGpuTimelineSrcSetCores(m->gpuTimelineSrc, m->hudCores, m->hudFirmware);
			hudGpuTimelineSrcSetColorMode(m->gpuTimelineSrc, m->sceneColorMode);
			m->dataSources[m->dataSourceCount++] = m->gpuTimelineSrc;
		}
	}

	if (m->perfMode == 3 || m->perfMode == 4) {
		if (m->liveSession != NULL)
			hudLiveGetCounters(m->liveSession, counters);
		hudGpuSetPerfCounters(counters);
	} else {
		hudGpuSetPerfCounters(m->traceCounters);
	}

	if (m->perfMode == 2 && m->paMgr != NULL) {
		hudPaSetBuffer(m->paMgr, 0, m->traceBufSize);
		hudPaSetConfig(m->paMgr, m->traceCounters,
			(uint32_t)m->traceMask, (uint32_t)(m->traceMask >> 32), m->traceRate);
		hudPaSetTraceFlags(m->paMgr, 0, 1, 1);
		hudPaSetup(m->paMgr);
	}

	if ((m->flags & 0x40U) != 0) {
		if ((m->flags & 0x10U) == 0)
			m->useCpuBase = 1;
		if (m->cpuActSrc == NULL) {
			m->cpuActSrc = hudNew(336U);
			hudCpuActSrcCtor(m->cpuActSrc);
		}
		if (m->cpuMarkersSrc == NULL) {
			m->cpuMarkersSrc = hudNew(789168U);
			hudCpuMarkersSrcCtor(m->cpuMarkersSrc, &m->paTime, m->userMarkerPackets);
		}
		hudCpuActSrcSetCapacity(m->cpuActSrc, 120U);
		m->dataSources[m->dataSourceCount++] = m->cpuActSrc;
		m->dataSources[m->dataSourceCount++] = m->cpuMarkersSrc;
	}

	if ((m->flags & 0x10U) != 0) {
		if (m->gpuFrameSrc == NULL) {
			m->gpuFrameSrc = hudNew(60U);
			hudGpuFrameSrcCtor(m->gpuFrameSrc);
		}
		if (m->gpuBufSrc == NULL) {
			m->gpuBufSrc = hudNew(156U);
			hudGpuBufSrcCtor(m->gpuBufSrc);
		}
		hudSeriesSrcSetCapacity(m->gpuFrameSrc, 120U);
		hudSeriesSrcSetRange(m->gpuFrameSrc, 0.0f, 33.333332f);
		hudSeriesSrcSetAutoScale(m->gpuFrameSrc, 0, 1, 120U);
		hudSeriesSrcSetStep(m->gpuFrameSrc, 16.666666f);
		hudGpuFrameSrcSetGpuTimeline(m->gpuFrameSrc, m->gpuTimelineSrc);
		m->dataSources[m->dataSourceCount++] = m->gpuFrameSrc;

		hudSeriesSrcSetCapacity(m->gpuBufSrc, 120U);
		hudSeriesSrcSetRange(m->gpuBufSrc, 0.0f, 100.0f);
		hudSeriesSrcSetAutoScale(m->gpuBufSrc, 0, 0, 20U);
		hudSeriesSrcSetStep(m->gpuBufSrc, 25.0f);
		hudGpuBufSrcSetParamBufSize(m->gpuBufSrc, m->paramBufSize);
		hudGpuBufSrcSetGpuTimeline(m->gpuBufSrc, m->gpuTimelineSrc);
		m->dataSources[m->dataSourceCount++] = m->gpuBufSrc;
	}

	if ((m->flags & 0x40U) != 0) {
		if ((m->flags & 0x0EU) == 0) {
			if (m->cpuUtilWidget == NULL) {
				m->cpuUtilWidget = hudNew(16124U);
				hudBarGraphCtor(m->cpuUtilWidget, 0U);
			}
			if (m->cpuFpsWidget == NULL) {
				m->cpuFpsWidget = hudNew(16124U);
				hudBarGraphCtor(m->cpuFpsWidget, 1U);
			}
			if (m->cpuMarkersWidget == NULL) {
				m->cpuMarkersWidget = hudNew(16252U);
				hudTimelineWidgetCtor(m->cpuMarkersWidget);
			}
		}
		if (m->cpuMarkersWidget != NULL) {
			hudViewSetDataSource(m->cpuMarkersWidget, m->cpuMarkersSrc);
			hudWidgetSetTitle(m->cpuMarkersWidget, "CPU User Markers");
			hudWidgetSetTitleStyle(m->cpuMarkersWidget, 2U, 12.0f);
			hudTimelineWidgetSetAutoHeight(m->cpuMarkersWidget, 1U);
			hudTimelineWidgetSetRowHeight(m->cpuMarkersWidget, 12.0f);
			m->views[m->viewCount++] = m->cpuMarkersWidget;
		}
		if (m->cpuUtilWidget != NULL) {
			hudViewSetDataSource(m->cpuUtilWidget, hudCpuActSrcGetTotalSeries(m->cpuActSrc));
			hudWidgetSetTitle(m->cpuUtilWidget, "CPU Utilization");
			hudWidgetSetTitleStyle(m->cpuUtilWidget, 1U, 12.0f);
			m->views[m->viewCount++] = m->cpuUtilWidget;
		}
		if (m->cpuFpsWidget != NULL) {
			hudViewSetDataSource(m->cpuFpsWidget, hudCpuActSrcGetFpsSeries(m->cpuActSrc));
			hudWidgetSetTitle(m->cpuFpsWidget, "CPU Framerate");
			hudWidgetSetTitleStyle(m->cpuFpsWidget, 1U, 12.0f);
			m->views[m->viewCount++] = m->cpuFpsWidget;
		}
		for (i = 0; i < 4; i++) {
			if (m->cpuCoreWidgets[i] == NULL) {
				m->cpuCoreWidgets[i] = hudNew(16124U);
				hudBarGraphCtor(m->cpuCoreWidgets[i], 0U);
			}
			hudViewSetDataSource(m->cpuCoreWidgets[i], hudCpuActSrcGetCoreSeries(m->cpuActSrc, (uint32_t)i));
			hudWidgetSetTitle(m->cpuCoreWidgets[i], "CPU Core Utilization");
			hudWidgetSetTitleStyle(m->cpuCoreWidgets[i], (i == 0) ? 1U : 0U, 12.0f);
			m->views[m->viewCount++] = m->cpuCoreWidgets[i];
		}
	}

	if ((m->flags & 0x10U) != 0) {
		if ((m->flags & 0x0EU) == 0) {
			if (m->gpuFrameWidget == NULL) {
				m->gpuFrameWidget = hudNew(16120U);
				hudLineGraphCtor(m->gpuFrameWidget);
			}
			if (m->gpuBufWidget == NULL) {
				m->gpuBufWidget = hudNew(16120U);
				hudLineGraphCtor(m->gpuBufWidget);
			}
			if (m->gpuTimelineWidget == NULL) {
				m->gpuTimelineWidget = hudNew(16252U);
				hudTimelineWidgetCtor(m->gpuTimelineWidget);
			}
		}
		if (m->gpuFrameWidget != NULL) {
			hudViewSetDataSource(m->gpuFrameWidget, m->gpuFrameSrc);
			hudWidgetSetTitle(m->gpuFrameWidget, "GPU Frametime");
			hudWidgetSetTitleStyle(m->gpuFrameWidget, 1U, 12.0f);
			m->views[m->viewCount++] = m->gpuFrameWidget;
		}
		if (m->gpuBufWidget != NULL) {
			hudViewSetDataSource(m->gpuBufWidget, m->gpuBufSrc);
			hudWidgetSetTitle(m->gpuBufWidget, "GPU Buffers");
			hudWidgetSetTitleStyle(m->gpuBufWidget, 1U, 12.0f);
			m->views[m->viewCount++] = m->gpuBufWidget;
		}
		if (m->gpuTimelineWidget != NULL) {
			hudViewSetDataSource(m->gpuTimelineWidget, m->gpuTimelineSrc);
			hudWidgetSetTitle(m->gpuTimelineWidget, "GPU Jobs");
			hudWidgetSetTitleStyle(m->gpuTimelineWidget, 2U, 12.0f);
			m->views[m->viewCount++] = m->gpuTimelineWidget;
		}
		if ((m->flags & 2U) != 0) {
			if (m->txtLogView == NULL) {
				m->txtLogView = hudNew(131096U);
				hudTxtLogCtor(m->txtLogView);
			}
			hudViewSetDataSource(m->txtLogView, m->gpuTimelineSrc);
			m->views[m->viewCount++] = m->txtLogView;
		}
		if ((m->flags & 4U) != 0) {
			if (m->csvLogView == NULL) {
				m->csvLogView = hudNew(131096U);
				hudCsvLogCtor(m->csvLogView);
			}
			hudViewSetDataSource(m->csvLogView, m->gpuTimelineSrc);
			m->views[m->viewCount++] = m->csvLogView;
		}
		if ((m->flags & 8U) != 0) {
			if (m->xmlLogView == NULL) {
				m->xmlLogView = hudNew(131096U);
				hudXmlLogCtor(m->xmlLogView);
			}
			hudViewSetDataSource(m->xmlLogView, m->gpuTimelineSrc);
			m->views[m->viewCount++] = m->xmlLogView;
		}
		{
			uint8_t useFile = (m->flags & 0x20U) != 0 ? 1 : 0;
			if (m->txtLogView != NULL) {
				hudLogSetFilename(m->txtLogView, m->txtFilename);
				hudLogSetUseFile(m->txtLogView, useFile);
			}
			if (m->csvLogView != NULL) {
				hudLogSetFilename(m->csvLogView, m->csvFilename);
				hudLogSetUseFile(m->csvLogView, useFile);
			}
			if (m->xmlLogView != NULL) {
				hudLogSetFilename(m->xmlLogView, m->xmlFilename);
				hudLogSetUseFile(m->xmlLogView, useFile);
			}
		}
	}

	hudMgrLayoutWidgets(m, 0U, 0U, m->rtWidth - 1U, m->rtHeight - 1U);
}

void hudMgrDestroyShaders(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	if (m->fpColor != NULL) {
		sceGxmShaderPatcherReleaseFragmentProgram(m->patcher, m->fpColor);
		m->fpColor = NULL;
	}
	if (m->fpTex != NULL) {
		sceGxmShaderPatcherReleaseFragmentProgram(m->patcher, m->fpTex);
		m->fpTex = NULL;
	}
	if (m->vp != NULL) {
		sceGxmShaderPatcherReleaseVertexProgram(m->patcher, m->vp);
		m->vp = NULL;
	}
	if (m->fpColorId != NULL) {
		sceGxmShaderPatcherUnregisterProgram(m->patcher, m->fpColorId);
		m->fpColorId = NULL;
	}
	if (m->fpTexId != NULL) {
		sceGxmShaderPatcherUnregisterProgram(m->patcher, m->fpTexId);
		m->fpTexId = NULL;
	}
	if (m->vpId != NULL) {
		sceGxmShaderPatcherUnregisterProgram(m->patcher, m->vpId);
		m->vpId = NULL;
	}
	if (m->patcher != NULL) {
		sceGxmShaderPatcherDestroy(m->patcher);
		m->patcher = NULL;
	}
}

int hudMgrCreateShaders(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	SceGxmShaderPatcherParams params;
	SceGxmVertexAttribute attrs[3];
	SceGxmVertexStream stream;
	SceGxmBlendInfo blend;
	const SceGxmProgram *vpProg;

	hudMgrDestroyShaders(m);
	sceClibMemset(&params, 0, sizeof(params));
	params.hostAllocCallback = hudPatcherHostAlloc;
	params.hostFreeCallback = hudPatcherHostFree;
	params.bufferMem = hudGetPatcherBufMem();
	params.bufferMemSize = hudGetPatcherBufSize();
	params.vertexUsseMem = hudGetPatcherVertexUsseMem();
	params.vertexUsseMemSize = hudGetPatcherVertexUsseSize();
	params.vertexUsseOffset = hudGetPatcherVertexUsseOffset();
	params.fragmentUsseMem = hudGetPatcherFragmentUsseMem();
	params.fragmentUsseMemSize = hudGetPatcherFragmentUsseSize();
	params.fragmentUsseOffset = hudGetPatcherFragmentUsseOffset();
	sceGxmShaderPatcherCreate(&params, &m->patcher);

	sceGxmShaderPatcherRegisterProgram(m->patcher, (const SceGxmProgram *)g_hudVertexGxp, &m->vpId);
	sceGxmShaderPatcherRegisterProgram(m->patcher, (const SceGxmProgram *)g_hudColorFragGxp, &m->fpColorId);
	sceGxmShaderPatcherRegisterProgram(m->patcher, (const SceGxmProgram *)g_hudTexFragGxp, &m->fpTexId);

	vpProg = sceGxmShaderPatcherGetProgramFromId(m->vpId);
	attrs[0].streamIndex = 0;
	attrs[0].offset = 0;
	attrs[0].format = 9;
	attrs[0].componentCount = 2;
	attrs[0].regIndex = (uint16_t)hudFindParamResourceIndex(vpProg, "aPosition");

	attrs[1].streamIndex = 0;
	attrs[1].offset = 8;
	attrs[1].format = 4;
	attrs[1].componentCount = 4;
	attrs[1].regIndex = (uint16_t)hudFindParamResourceIndex(vpProg, "aColor");

	attrs[2].streamIndex = 0;
	attrs[2].offset = 12;
	attrs[2].format = 8;
	attrs[2].componentCount = 2;
	attrs[2].regIndex = (uint16_t)hudFindParamResourceIndex(vpProg, "aTexCoord");

	stream.stride = 16;
	stream.indexSource = 0;
	m->vp = NULL;
	sceGxmShaderPatcherCreateVertexProgram(m->patcher, m->vpId, attrs, 3U, &stream, 1U, &m->vp);

	hudMemFontSingleton();
	blend.colorMask = 0xFU;
	blend.colorFunc = 1;
	blend.alphaFunc = 1;
	blend.colorSrc = 4;
	blend.colorDst = 5;
	blend.alphaSrc = 4;
	blend.alphaDst = 5;

	m->fpColor = NULL;
	sceGxmShaderPatcherCreateFragmentProgram(m->patcher, m->fpColorId, 0U, m->msaaMode, &blend, vpProg, &m->fpColor);
	m->fpTex = NULL;
	return sceGxmShaderPatcherCreateFragmentProgram(m->patcher, m->fpTexId, 0U, m->msaaMode, &blend, vpProg, &m->fpTex);
}

void hudMgrStopAll(void *mgrPtr, int unloadPerf)
{
	HudManager *m = (HudManager *)mgrPtr;
	int uid;

	if (m->liveSession != NULL)
		hudLiveStop(m->liveSession);
	if (m->paMgr != NULL)
		hudPaTeardown(m->paMgr);
	if (m->perfMode == 4) {
		if (m->userMarkerBuf != NULL) {
			sceRazorCpuStopUserMarkerTrace();
			hudPoolFree(0U, m->userMarkerBuf);
			m->userMarkerBuf = NULL;
		}
		if (m->perfMode == 4 && m->activityBuf != NULL) {
			sceRazorCpuStopActivityMonitor();
			hudPoolFree(0U, m->activityBuf);
			m->activityBuf = NULL;
		}
	}
	hudMgrDestroyWidgets(m);
	hudMemFontTerm(hudMemFontSingleton());
	hudMgrDestroyShaders(m);
	if (m->rt != NULL) {
		sceGxmRenderTargetGetDriverMemBlock(m->rt, &uid);
		sceGxmDestroyRenderTarget(m->rt);
		m->rt = NULL;
		sceKernelFreeMemBlock(uid);
	}
	if (m->perfLoaded != 0 && unloadPerf != 0) {
		int ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_PERF);
		if (ret != 0)
			sceClibPrintf("[SceRazorHud]:Failed to stop PERF module (0x%08x)\n", ret);
		m->perfLoaded = 0;
	}
}

int hudMgrCreateRenderTarget(void *mgrPtr)
{
	HudManager *m = (HudManager *)mgrPtr;
	SceGxmRenderTargetParams rtParams;
	uint32_t rtSize = 0;
	int uid;

	hudMgrDestroyShaders(m);
	if (m->rt != NULL) {
		sceGxmRenderTargetGetDriverMemBlock(m->rt, &uid);
		sceGxmDestroyRenderTarget(m->rt);
		m->rt = NULL;
		sceKernelFreeMemBlock(uid);
	}

	m->orthoLeft = 0.0f;
	m->orthoTop = 0.0f;
	m->orthoRight = (float)(m->rtWidth - 1U);
	m->orthoBottom = (float)(m->rtHeight - 1U);

	sceClibMemset(&rtParams, 0, sizeof(rtParams));
	rtParams.flags = 8U;
	rtParams.width = (uint16_t)m->rtWidth;
	rtParams.height = (uint16_t)m->rtHeight;
	rtParams.scenesPerFrame = 1U;
	rtParams.multisampleMode = (uint16_t)m->msaaMode;
	rtParams.multisampleLocations = 0;
	rtParams.driverMemBlock = -1;
	sceGxmGetRenderTargetMemSizeInternal(&rtParams, &rtSize);
	rtParams.driverMemBlock = sceKernelAllocMemBlock("RazorHud", 0x0C208060U, rtSize, NULL);
	sceGxmCreateRenderTargetInternal(&rtParams, &m->rt);
	return hudMgrCreateShaders(m);
}

void hudMgrOnSwap(void *mgrPtr, const SceGxmColorSurface *colorSurface, SceGxmSyncObject *fragmentSyncObject)
{
	HudManager *m = (HudManager *)mgrPtr;
	uint32_t counters[16];
	uint32_t bufIdx;
	int beganScene = 0;
	float wvp[16];
	HudDrawContext drawCtx;
	uint32_t i;

	if (m->gxmContext == NULL || (m->flags & 1U) == 0)
		return;

	m->swapCount++;
	if (m->perfMode == 3 || m->perfMode == 4) {
		if (m->liveSession != NULL)
			hudLiveGetCounters(m->liveSession, counters);
		hudGpuSetPerfCounters(counters);
	} else {
		hudGpuSetPerfCounters(m->traceCounters);
	}

	if (m->perfMode == 2 && m->paMgr != NULL && hudPaIsSetup(m->paMgr) != 0) {
		if (hudPaIsRunning(m->paMgr) != 0) {
			hudPaInsertBookmark(m->paMgr, HUD_TRACE_BOOKMARK_FRAME);
			if (m->traceTriggered != 0 || m->swapCount == m->traceStopFrame) {
				uint32_t freq;
				m->traceTriggered = 0;
				hudPaStop(m->paMgr);
				freq = hudGpuGetClockFreq();
				hudPaSetFilename(m->paMgr, m->traceFilename, m->traceCountLen);
				hudPaWriteNextPmon(m->paMgr, freq);
				hudHostToolNotifyDone(0);
				m->traceNeedStart = 1;
				if (m->savedConfig != NULL) {
					hudConfigApplyToManager(m->savedConfig, m);
					hudDelete(m->savedConfig);
					m->savedConfig = NULL;
				}
			}
		} else if (m->traceNeedStart != 0) {
			hudPaSetConfig(m->paMgr, m->traceCounters,
				(uint32_t)m->traceMask, (uint32_t)(m->traceMask >> 32), m->traceRate);
			hudHostToolSetStatus(0);
			m->traceNeedStart = 0;
			hudPaSetup(m->paMgr);
			hudPaStart(m->paMgr);
			sceClibPrintf("[SceRazorHud]: Starting GPU Trace...\n");
		}
	}

	if (m->gpuTimelineSrc != NULL)
		hudGpuTimelineNull1();

	if (m->rt == NULL) {
		if ((m->flags & 0x50U) != 0) {
			hudMemFontInit(hudMemFontSingleton());
			hudMgrCreateRenderTarget(m);
			m->wvpParam = hudFindProgramParam(m->vpId, "wvp");
		}
		if (m->widgetsBuilt == 0)
			hudMgrRebuildWidgets(m);
	}

	bufIdx = m->swapCount & 1U;
	if (m->gpuBufWidget != NULL || m->gpuFrameWidget != NULL || m->gpuTimelineWidget != NULL ||
	    m->cpuMarkersWidget != NULL || m->cpuUtilWidget != NULL || m->cpuFpsWidget != NULL) {
		SceGxmColorSurface surf;
		SceGxmValidRegion vr;
		uint32_t xMin, yMin, xMax, yMax;

		if (colorSurface == NULL)
			return;
		sceGxmNotificationWait(&m->notify[bufIdx]);
		m->notify[bufIdx].value++;
		surf = *colorSurface;
		if (m->msaaMode != 0)
			surf.pbeSidebandWord |= 1U;
		else
			surf.pbeSidebandWord &= ~1U;
		surf.pbeEmitWords[0] &= ~0x02000000U;
		surf.outputRegisterSize = 0;
		sceGxmColorSurfaceSetGammaMode(&surf, 0U);
		sceGxmSetCullMode(m->gxmContext, 0U);
		sceGxmSetFrontDepthFunc(m->gxmContext, 7U);
		sceGxmSetBackDepthFunc(m->gxmContext, 7U);
		sceGxmColorSurfaceGetClip(colorSurface, &xMin, &yMin, &xMax, &yMax);
		vr.xMax = (xMax < m->rtWidth - 1U) ? xMax : (m->rtWidth - 1U);
		vr.yMax = (yMax < m->rtHeight - 1U) ? yMax : (m->rtHeight - 1U);
		sceGxmBeginScene(m->gxmContext, 0U, m->rt, &vr, NULL, fragmentSyncObject, &surf, NULL);
		beganScene = 1;
	}

	sceClibMemset(wvp, 0, sizeof(wvp));
	wvp[0]  = 2.0f / (m->orthoRight - m->orthoLeft);
	wvp[5]  = 2.0f / (m->orthoTop - m->orthoBottom);
	wvp[10] = -0.01f;
	wvp[12] = -((m->orthoLeft + m->orthoRight) / (m->orthoRight - m->orthoLeft));
	wvp[13] = -((m->orthoBottom + m->orthoTop) / (m->orthoTop - m->orthoBottom));
	wvp[14] = -0.0f;
	wvp[15] = 1.0f;

	drawCtx.context = m->gxmContext;
	drawCtx.vertexProgram = m->vp;
	drawCtx.colorFragProgram = m->fpColor;
	drawCtx.texFragProgram = m->fpTex;
	drawCtx.wvpParam = m->wvpParam;
	drawCtx.wvpMatrix = wvp;

	if (m->cpuMarkersWidget != NULL && m->gpuTimelineWidget != NULL) {
		HudWidgetBase *gtl = (HudWidgetBase *)m->gpuTimelineWidget;
		hudWidgetSetTop(m->cpuMarkersWidget, gtl->y - gtl->h);
	}

	for (i = 0; i < m->viewCount; i++) {
		const HudWidgetVtable *vt = *(const HudWidgetVtable **)m->views[i];
		hudViewSetEnabled(m->views[i], m->displayEnabled);
		vt->render(m->views[i], &drawCtx);
	}

	if (beganScene)
		sceGxmEndScene(m->gxmContext, &m->notify[bufIdx], NULL);

	m->paTime = sceKernelPaGetTimebaseValue() / (uint64_t)sceKernelPaGetTimebaseFrequency();
	m->frameCounter++;
	for (i = 0; i < m->dataSourceCount; i++) {
		void (**vt)(void *) = *(void (***)(void *))m->dataSources[i];
		vt[2](m->dataSources[i]);
	}
}

/* Base 2D Widget */
void *hudWidgetBaseDtor(void *objPtr)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	w->vtable = &g_widgetBaseVtable;
	hudPoolFree(1U, w->vb[0]);
	hudPoolFree(1U, w->ib[0]);
	hudPoolFree(1U, w->vb[1]);
	hudPoolFree(1U, w->ib[1]);
	hudViewBaseDtor(w);
	return w;
}

void hudWidgetBaseRender(void *objPtr, void *drawCtxPtr)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	HudDrawContext *ctx = (HudDrawContext *)drawCtxPtr;
	void *font;
	void *ubuf = NULL;
	uint32_t curTex = 2U;
	uint32_t i;

	if (w->enabled == 0)
		return;
	w->bufIdx = (w->bufIdx + 1U) & 1U;
	font = hudMemFontSingleton();
	if (font == NULL)
		return;

	w->vertCount = 0;
	w->cmdCount = 0;
	sceGxmSetVertexProgram(ctx->context, ctx->vertexProgram);
	sceGxmSetFragmentProgram(ctx->context, ctx->colorFragProgram);
	sceGxmSetVertexStream(ctx->context, 0U, w->vb[w->bufIdx]);
	sceGxmReserveVertexDefaultUniformBuffer(ctx->context, &ubuf);
	sceGxmSetUniformDataF(ubuf, ctx->wvpParam, 0U, 16U, ctx->wvpMatrix);

	w->vtable->build(w);

	for (i = 0; i < w->cmdCount; i++) {
		HudDrawCmd *c = &w->cmds[i];
		if (c->textureIdx != curTex) {
			curTex = c->textureIdx;
			if (curTex == 2U) {
				sceGxmSetFragmentProgram(ctx->context, ctx->colorFragProgram);
			} else {
				const SceGxmTexture *tex = (const SceGxmTexture *)hudMemFontGetTexture(font, (int)curTex);
				sceGxmSetFragmentTexture(ctx->context, 0U, tex);
				sceGxmSetFragmentProgram(ctx->context, ctx->texFragProgram);
			}
		}
		sceGxmSetFrontPolygonMode(ctx->context, (c->primType == 0x04000000U) ? 0x00000100U : 0U);
		if (c->indexCount != 0)
			sceGxmDraw(ctx->context, c->primType, 0U, &w->ib[w->bufIdx][c->indexStart], c->indexCount);
	}
}

void *hudWidgetBaseDeletingDtor(void *objPtr)
{
	hudWidgetBaseDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudWidgetBaseCtor(void *objPtr)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	int i;

	hudViewBaseCtor(w);
	w->vtable = &g_widgetBaseVtable;
	w->bufIdx = 0;
	w->vertCount = 0;
	w->cmdCount = 0;
	sceClibMemset(w->cmds, 0, sizeof(w->cmds));
	w->textColor = 0xFFFFFFFFU;
	w->fontWidth = 1.0f;
	w->fontHeight = 1.0f;
	w->x = 0.0f;
	w->y = 0.0f;
	w->w = 1.0f;
	w->h = 1.0f;
	w->titleFontSize = 1.0f;
	for (i = 0; i < 2; i++) {
		w->vb[i] = (HudVertex *)hudPoolAlloc(1U, 240000U, 4U);
		w->ib[i] = (uint16_t *)hudPoolAlloc(1U, 30000U, 4U);
	}
	w->title[0] = '\0';
	w->titleStyle = 0;
	w->titleBgColor = 0x40010000U;
	w->titleFgColor = 0xFFFFFFFFU;
	return w;
}

void hudWidgetSetTop(void *objPtr, float y)
{
	((HudWidgetBase *)objPtr)->y = y;
}

void hudWidgetSetRect(void *objPtr, float x, float y, float w, float h)
{
	HudWidgetBase *wdg = (HudWidgetBase *)objPtr;
	wdg->x = x;
	wdg->y = y;
	wdg->w = w;
	wdg->h = h;
}

char *hudWidgetSetTitle(void *objPtr, const char *title)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	char *ret = sceClibStrncpy(w->title, title, 31U);
	w->title[31] = '\0';
	return ret;
}

void hudWidgetSetTitleStyle(void *objPtr, uint32_t style, float fontSize)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	w->titleStyle = style;
	w->titleFontSize = fontSize;
}

void hudWidgetAddRect(void *objPtr, float posXA, float posYA, float posXB, float posYB, uint32_t color)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	uint32_t dark;
	float rx1 = (posXA + 2.0f > posXB) ? (posXA + 2.0f) : posXB;

	hudWidgetEnsureCmd(w, 0x0C000000U, 2U);
	dark = (color & 0xFF000000U) |
	       (((color & 0xFFU) >> 1) & 0xFFU) |
	       ((((color >> 8) & 0xFFU) >> 1) << 8) |
	       ((((color >> 16) & 0xFFU) >> 1) << 16);

	hudWidgetEmitVert(w, posXA,  posYA, color, 0, 0);
	hudWidgetEmitVert(w, posXA,  posYA, color, 0, 0);
	hudWidgetEmitVert(w, posXA,  posYB, dark,  0, 0);
	hudWidgetEmitVert(w, rx1, posYA, color, 0, 0);
	hudWidgetEmitVert(w, rx1, posYB, dark,  0, 0);
	hudWidgetEmitVert(w, rx1, posYB, dark,  0, 0);
}

void hudWidgetAddLine(void *objPtr, float posXA, float posYA, float posXB, float posYB, uint32_t color)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	hudWidgetEnsureCmd(w, 0x04000000U, 2U);
	hudWidgetEmitVert(w, posXA, posYA, color, 0, 0);
	hudWidgetEmitVert(w, posXB, posYB, color, 0, 0);
}

int hudWidgetAddText(void *objPtr, float x, float y, const char *fmt, ...)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	char buf[1024];
	va_list ap;
	uint32_t len, i;

	if (fmt == NULL)
		return 0;
	va_start(ap, fmt);
	sceClibVsnprintf(buf, sizeof(buf) - 1U, fmt, ap);
	va_end(ap);
	buf[1023] = '\0';

	len = sceClibStrnlen(buf, sizeof(buf));
	hudWidgetEnsureCmd(w, 0x0C000000U, 0U);
	for (i = 0; i < len; i++) {
		int ch = (int)(int8_t)(buf[i] - 32);
		float texUA = (float)(ch % 16) * 0.0625f;
		float texVA = (float)(ch / 16) * 0.125f;
		float texUB = texUA + 0.0625f;
		float texVB = texVA + 0.125f;
		uint16_t hu0 = hudFloatToHalf(hudFloatBits(texUA));
		uint16_t hv0 = hudFloatToHalf(hudFloatBits(texVA));
		uint16_t hu1 = hudFloatToHalf(hudFloatBits(texUB));
		uint16_t hv1 = hudFloatToHalf(hudFloatBits(texVB));
		float cw = w->fontWidth;
		float chh = w->fontHeight;
		uint32_t col = w->textColor;

		hudWidgetEmitVert(w, x,      y,       col, hu0, hv0);
		hudWidgetEmitVert(w, x,      y,       col, hu0, hv0);
		hudWidgetEmitVert(w, x + cw, y,       col, hu1, hv0);
		hudWidgetEmitVert(w, x,      y + chh, col, hu0, hv1);
		hudWidgetEmitVert(w, x + cw, y + chh, col, hu1, hv1);
		hudWidgetEmitVert(w, x + cw, y + chh, col, hu1, hv1);
		x += cw * 0.5f;
	}
	return 0;
}

void hudWidgetDrawFrame(void *objPtr)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	uint32_t len;
	float fsz, baseX, posYA = 0.0f, posYB = 0.0f, textY = 0.0f;
	uint32_t topCol = 0, botCol = 0;

	hudWidgetAddRect(w, w->x, w->y, w->x + w->w, w->y + w->h, 0x40010000U);
	len = sceClibStrnlen(w->title, sizeof(w->title));
	fsz = w->titleFontSize;
	baseX = w->x;

	if (w->titleStyle == 0)
		return;
	if (w->titleStyle == 1) {
		uint32_t c = w->titleBgColor;
		topCol = (c & 0xFF000000U) | (((c & 0xFFU) >> 1) & 0xFFU) |
			((((c >> 8) & 0xFFU) >> 1) << 8) | ((((c >> 16) & 0xFFU) >> 1) << 16);
		botCol = c;
		posYA = w->y;
		posYB = w->y - fsz;
		textY = posYB;
	} else if (w->titleStyle == 2) {
		uint32_t c = w->titleBgColor;
		topCol = c;
		botCol = (c & 0xFF000000U) | (((c & 0xFFU) >> 1) & 0xFFU) |
			((((c >> 8) & 0xFFU) >> 1) << 8) | ((((c >> 16) & 0xFFU) >> 1) << 16);
		posYA = w->y + w->h;
		posYB = posYA + fsz;
		textY = posYA;
	}

	hudWidgetEnsureCmd(w, 0x0C000000U, 2U);
	{
		float xStart = baseX + fsz;
		float xMid = xStart + (fsz * (float)len) * 0.64f;
		float xEnd = xMid + fsz;
		hudWidgetEmitVert(w, xStart, posYB, topCol, 0, 0);
		hudWidgetEmitVert(w, xStart, posYB, topCol, 0, 0);
		hudWidgetEmitVert(w, baseX,  posYA, botCol, 0, 0);
		hudWidgetEmitVert(w, xMid,   posYB, topCol, 0, 0);
		hudWidgetEmitVert(w, xEnd,   posYA, botCol, 0, 0);
		hudWidgetEmitVert(w, xEnd,   posYA, botCol, 0, 0);
	}
	w->fontWidth = fsz;
	w->fontHeight = fsz;
	w->textColor = w->titleFgColor;
	hudWidgetAddText(w, baseX + fsz, textY, "%s", w->title);
}

/* Line Graph Widget */
void *hudLineGraphDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_lineGraphVtable;
	hudWidgetBaseDtor(objPtr);
	return objPtr;
}

void *hudLineGraphDeletingDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_lineGraphVtable;
	hudWidgetBaseDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudLineGraphCtor(void *objPtr)
{
	hudWidgetBaseCtor(objPtr);
	*(const HudWidgetVtable **)objPtr = &g_lineGraphVtable;
	return objPtr;
}

void hudLineGraphBuild(void *objPtr)
{
	HudWidgetBase *w = (HudWidgetBase *)objPtr;
	void *src = w->dataSource;
	float padX, padY, gx0, gy0, gx1, gy1, minV, maxV, stepV, dx;
	uint32_t nSeries, s, i;

	hudWidgetDrawFrame(w);
	if (src == NULL)
		return;

	padX = 3.0f;
	padY = 3.0f;
	gx0 = w->x + padX;
	gx1 = w->x + w->w - padX;
	gy0 = w->y + padY;
	gy1 = w->y + w->h - padY;

	hudWidgetAddRect(w, gx0, gy0, gx1, gy1, 0x80707070U);
	hudWidgetAddLine(w, gx0, gy1, gx1, gy1, 0x81000000U);

	maxV = *hudSeriesSrcGetMaxPtr(src);
	minV = *hudSeriesSrcGetMinPtr(src);
	stepV = hudSeriesSrcGetStep(src);
	if (stepV > 0.0f) {
		float lim = maxV + stepV * 0.25f;
		float v;
		for (v = 0.0f; v < lim; v += stepV) {
			float y = gy1 - ((v - minV) / (maxV - minV)) * (gy1 - gy0);
			hudWidgetAddLine(w, gx0, y, gx1, y, 0x81000000U);
		}
	}

	dx = (gx1 - gx0) / (float)hudSeriesSrcGetCapacity(src);
	nSeries = hudSeriesSrcGetSeriesCount(src);
	for (s = 0; s < nSeries; s++) {
		uint32_t col = hudSeriesSrcGetColor(src, s);
		uint32_t cnt = hudSeriesSrcGetCount(src, s);
		if (cnt > 1U) {
			float prevV = *hudSeriesSrcGetSample(src, s, 0U);
			float x = gx0 + dx;
			for (i = 1U; i < cnt; i++) {
				float curV = *hudSeriesSrcGetSample(src, s, i);
				float posYA = gy1 - ((prevV - minV) / (maxV - minV)) * (gy1 - gy0);
				float posYB = gy1 - ((curV  - minV) / (maxV - minV)) * (gy1 - gy0);
				hudWidgetAddLine(w, x - dx, posYA, x, posYB, col);
				prevV = curV;
				x += dx;
			}
		}
	}
}

/* Bar / Stacked Graph Widget */
void *hudBarGraphDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_barGraphVtable;
	hudWidgetBaseDtor(objPtr);
	return objPtr;
}

void *hudBarGraphDeletingDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_barGraphVtable;
	hudWidgetBaseDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudBarGraphCtor(void *objPtr, uint32_t mode)
{
	HudBarGraphWidget *w = (HudBarGraphWidget *)objPtr;
	hudWidgetBaseCtor(&w->base);
	w->base.vtable = &g_barGraphVtable;
	w->mode = mode;
	return w;
}

void hudBarGraphBuild(void *objPtr)
{
	HudBarGraphWidget *bw = (HudBarGraphWidget *)objPtr;
	HudWidgetBase *w = &bw->base;
	void *src = w->dataSource;
	float gx0, gy0, gx1, gy1, minV, maxV, stepV, dx, xStart;
	uint32_t nSeries, cap, cnt0, i, s;

	hudWidgetDrawFrame(w);
	if (src == NULL)
		return;

	gx0 = w->x + 3.0f;
	gx1 = w->x + w->w - 3.0f;
	gy0 = w->y + 3.0f;
	gy1 = w->y + w->h - 3.0f;

	hudWidgetAddRect(w, gx0, gy0, gx1, gy1, 0x80707070U);
	hudWidgetAddLine(w, gx0, gy1, gx1, gy1, 0x81000000U);

	maxV = *hudSeriesSrcGetMaxPtr(src);
	minV = *hudSeriesSrcGetMinPtr(src);
	stepV = hudSeriesSrcGetStep(src);
	if (stepV > 0.0f) {
		float lim = maxV + stepV * 0.25f;
		float v;
		for (v = 0.0f; v < lim; v += stepV) {
			float y = gy1 - ((v - minV) / (maxV - minV)) * (gy1 - gy0);
			hudWidgetAddLine(w, gx0, y, gx1, y, 0x81000000U);
		}
	}

	cap = hudSeriesSrcGetCapacity(src);
	dx = (gx1 - gx0) / (float)cap;
	nSeries = hudSeriesSrcGetSeriesCount(src);
	cnt0 = hudSeriesSrcGetCount(src, 0U);
	xStart = gx0 + (float)(cap - cnt0) * dx;

	for (i = 1U; i < cnt0; i++) {
		float x = xStart + (float)i * dx;
		float baseY = gy1;
		for (s = 0; s + 1U < nSeries; s++) {
			uint32_t col = hudSeriesSrcGetColor(src, s);
			if (hudSeriesSrcGetCount(src, s) == 0)
				continue;
			{
				float curV = *hudSeriesSrcGetSample(src, s, i);
				if (bw->mode != 0) {
					float prevV = *hudSeriesSrcGetSample(src, s, i - 1U);
					float posYA = gy1 - ((prevV - minV) / (maxV - minV)) * (gy1 - gy0);
					float posYB = gy1 - ((curV  - minV) / (maxV - minV)) * (gy1 - gy0);
					hudWidgetAddLine(w, x - dx, posYA, x, posYB, col);
				} else {
					float nextY = baseY + ((gy0 - gy1) * curV) / (maxV - minV);
					hudWidgetAddRect(w, x - dx, nextY, x, baseY, col);
					baseY = nextY;
				}
			}
		}
	}
}

/* Timeline Widget */
void *hudTimelineWidgetDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_timelineWidgetVtable;
	hudWidgetBaseDtor(objPtr);
	return objPtr;
}

void *hudTimelineWidgetDeletingDtor(void *objPtr)
{
	*(const HudWidgetVtable **)objPtr = &g_timelineWidgetVtable;
	hudWidgetBaseDtor(objPtr);
	hudDelete(objPtr);
	return objPtr;
}

void *hudTimelineWidgetCtor(void *objPtr)
{
	HudTimelineWidget *tw = (HudTimelineWidget *)objPtr;
	hudWidgetBaseCtor(&tw->base);
	tw->base.vtable = &g_timelineWidgetVtable;
	tw->autoHeight = 0;
	hudMemsetWrap(tw->rowLabels, 0, sizeof(tw->rowLabels));
	tw->rowHeight = 0.0f;
	return tw;
}

void hudTimelineWidgetSetAutoHeight(void *objPtr, uint32_t mode)
{
	((HudTimelineWidget *)objPtr)->autoHeight = mode;
}

void hudTimelineWidgetSetRowHeight(void *objPtr, float h)
{
	((HudTimelineWidget *)objPtr)->rowHeight = h;
}

void hudTimelineWidgetBuild(void *objPtr)
{
	HudTimelineWidget *tw = (HudTimelineWidget *)objPtr;
	HudWidgetBase *w = &tw->base;
	void *src = w->dataSource;
	uint32_t tracks, i, j, mCount;
	float gx0, gx1, gyTop, rowH, rowGap, winStart = 0.0f, winEnd = 0.0f;
	float spanMs, pxPerMs, maxX, yCur;

	hudWidgetDrawFrame(w);
	if (src == NULL)
		return;
	tracks = hudTimelineSrcGetTrackCount(src);
	if (tracks == 0)
		return;

	if (tw->autoHeight == 1U)
		w->h = (float)tracks * tw->rowHeight;

	gx0 = w->x + 3.0f;
	gx1 = w->x + w->w - 3.0f;
	rowGap = 3.0f;
	gyTop = w->y + rowGap;
	rowH = (w->h - (float)(tracks + 1U) * rowGap) / (float)tracks;

	yCur = gyTop;
	for (i = 0; i < tracks; i++) {
		hudWidgetAddRect(w, gx0, yCur, gx1, yCur + rowH, 0xA0202020U);
		yCur += rowH + rowGap;
	}

	hudTimelineSrcGetTimeWindow(src, &winStart, &winEnd);
	spanMs = (winEnd - winStart) * 0.06f;
	if (spanMs > 100.0f)
		spanMs = 100.0f;
	pxPerMs = ((gx1 - gx0) * 0.06f) / spanMs;
	maxX = gx0 + (winEnd - winStart) * pxPerMs;

	if ((winEnd - winStart) / 16.666666f < 100.0f) {
		float gx;
		for (gx = gx0 + winStart; gx <= gx0 + winEnd + 16.666666f; gx += 16.666666f)
			hudWidgetAddLine(w, gx, w->y, gx, w->y + w->h, 0x40FFFFFFU);
	}

	mCount = hudTimelineSrcGetMarkerCount(src);
	for (i = 0; i < mCount; i++) {
		struct { float timeMs; uint32_t color; } *mk = hudTimelineSrcGetMarker(src, i);
		float mx = gx0 + (mk->timeMs - winStart) * pxPerMs;
		if (mx >= gx0 && mx <= maxX)
			hudWidgetAddLine(w, mx, w->y, mx, w->y + w->h, mk->color);
	}

	yCur = gyTop;
	for (i = 0; i < tracks; i++) {
		uint32_t evCnt = hudTimelineSrcGetEventCount(src, i);
		for (j = 0; j < evCnt; j++) {
			struct { uint32_t flags; float startMs; float endMs; uint32_t color; } *ev =
				hudTimelineSrcGetEvent(src, i, j);
			float ex0 = gx0 + (ev->startMs - winStart) * pxPerMs;
			float ex1 = gx0 + (ev->endMs - winStart) * pxPerMs;
			if (ex1 >= gx0 && ex0 <= maxX)
				hudWidgetAddRect(w, ex0, yCur, ex1, yCur + rowH, ev->color);
		}
		yCur += rowH + rowGap;
	}
}
