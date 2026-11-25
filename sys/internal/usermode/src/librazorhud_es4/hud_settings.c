/*
 * hud_settings.ini parser, configuration storage, and HUD manager settings accessors.
 */
#include "razor_hud_internal.h"
#include "hud_assets.h"

typedef struct HudConfig {
	uint32_t flags;             /* 0 */
	uint32_t perfMode;          /* 4 */
	uint32_t hudCores;          /* 8 */
	uint32_t hudFirmware;       /* 12 */
	uint32_t displayFrames;     /* 16 */
	uint32_t sceneColorMode;    /* 20 */
	char baseFilename[1024];    /* 24 */
	char txtFilename[1024];     /* 1048 */
	char csvFilename[1024];     /* 2072 */
	char xmlFilename[1024];     /* 3096 */
	char traceFilename[1024];   /* 4120 */
	uint32_t rtWidth;           /* 5144 */
	uint32_t rtHeight;          /* 5148 */
	uint64_t traceMask;         /* 5152 */
	uint32_t traceRate;         /* 5160 */
	uint32_t traceBufSize;      /* 5164 */
	uint32_t traceStopFrame;    /* 5168 */
	uint32_t traceCountLen;     /* 5172 */
	uint32_t traceGroups[8];    /* 5176 */
	uint32_t traceCounters[8];  /* 5208 */
	uint32_t liveMetricsGroup;  /* 5240 */
	uint32_t liveReserved;      /* 5244 */
	uint8_t liveEnabled;        /* 5248 */
	uint8_t cfgTailPad[7];
} HudConfig;

typedef struct HudConfigKeyEntry {
	uint32_t id;
	const char *name;
} HudConfigKeyEntry;

static const HudConfigKeyEntry g_hudConfigKeys[29] = {
	{ 0, "INVALID" },
	{ 1, "ENABLE" },
	{ 2, "TTY" },
	{ 3, "CSVTTY" },
	{ 4, "XMLTTY" },
	{ 5, "FILE" },
	{ 6, "FILENAME" },
	{ 7, "RENDERTARGET_WIDTH" },
	{ 8, "RENDERTARGET_HEIGHT" },
	{ 9, "HUD" },
	{ 10, "GPU_HUD" },
	{ 11, "CPU_HUD" },
	{ 12, "HUD_CORES" },
	{ 13, "HUD_FIRMWARE" },
	{ 14, "HUD_DISPLAY_FRAMES" },
	{ 15, "HUD_SCENE_COLOR_MODE" },
	{ 16, "TRACE" },
	{ 17, "TRACE_RATE" },
	{ 18, "TRACE_RATE_CUSTOM" },
	{ 19, "TRACE_SIZE" },
	{ 20, "TRACE_SIZE_CUSTOM" },
	{ 21, "TRACE_METRICS_GROUP" },
	{ 22, "TRACE_STOP" },
	{ 23, "TRACE_GROUPS" },
	{ 24, "TRACE_COUNTERS" },
	{ 25, "TRACE_FILENAME" },
	{ 26, "TRACE_FILENAME_COUNT_LEN" },
	{ 27, "LIVE" },
	{ 28, "LIVE_METRICS_GROUP" },
};

int hudMgrGetPerfMode(void *mgr)
{
	return ((HudManager *)mgr)->perfMode;
}

int hudMgrSetPerfMode(void *mgr, int mode)
{
	if ((uint32_t)(mode - 1) > 3U)
		return HUD_ERR_INVALID_PARAM;
	((HudManager *)mgr)->perfMode = mode;
	return 0;
}

void hudMgrSetParamBufSize(void *mgr, uint32_t val)
{
	((HudManager *)mgr)->paramBufSize = val;
}

void hudMgrSetGxmVersions(void *mgr, uint32_t texVA, uint32_t texVB)
{
	((HudManager *)mgr)->gxmVerMajor = texVA;
	((HudManager *)mgr)->gxmVerMinor = texVB;
}

void hudMgrGetGxmVersions(void *mgr, uint32_t *texVA, uint32_t *texVB)
{
	if (texVA != NULL)
		*texVA = ((HudManager *)mgr)->gxmVerMajor;
	if (texVB != NULL)
		*texVB = ((HudManager *)mgr)->gxmVerMinor;
}

void hudMgrSetTraceConfig(void *mgr, const void *counters, uint64_t mask, uint32_t rate, uint32_t bufSize, uint32_t stopFrame)
{
	HudManager *hm = (HudManager *)mgr;
	sceClibMemcpy_safe(hm->traceCounters, counters, 0x40U);
	hm->traceMask = mask;
	hm->traceRate = rate;
	hm->traceBufSize = bufSize;
	hm->traceStopFrame = stopFrame;
	hm->swapCount = 0;
}

void hudMgrGetTraceConfig(void *mgr, void *counters, uint64_t *mask, uint32_t *rate, uint32_t *bufSize, uint32_t *stopFrame)
{
	HudManager *hm = (HudManager *)mgr;
	sceClibMemcpy_safe(counters, hm->traceCounters, 0x40U);
	*mask = hm->traceMask;
	*rate = hm->traceRate;
	*bufSize = hm->traceBufSize;
	*stopFrame = hm->traceStopFrame;
}

char *hudMgrSetTxtFilename(void *mgr, const char *name)
{
	char *dst = ((HudManager *)mgr)->txtFilename;
	char *ret = sceClibStrncpy(dst, name, 255U);
	dst[255] = '\0';
	return ret;
}

char *hudMgrGetTxtFilename(void *mgr, char *dst, int size)
{
	char *ret = sceClibStrncpy(dst, ((HudManager *)mgr)->txtFilename, (uint32_t)(size - 1));
	dst[size - 1] = '\0';
	return ret;
}

char *hudMgrSetCsvFilename(void *mgr, const char *name)
{
	char *dst = ((HudManager *)mgr)->csvFilename;
	char *ret = sceClibStrncpy(dst, name, 255U);
	dst[255] = '\0';
	return ret;
}

char *hudMgrGetCsvFilename(void *mgr, char *dst, int size)
{
	char *ret = sceClibStrncpy(dst, ((HudManager *)mgr)->csvFilename, (uint32_t)(size - 1));
	dst[size - 1] = '\0';
	return ret;
}

char *hudMgrSetXmlFilename(void *mgr, const char *name)
{
	char *dst = ((HudManager *)mgr)->xmlFilename;
	char *ret = sceClibStrncpy(dst, name, 255U);
	dst[255] = '\0';
	return ret;
}

char *hudMgrGetXmlFilename(void *mgr, char *dst, int size)
{
	char *ret = sceClibStrncpy(dst, ((HudManager *)mgr)->xmlFilename, (uint32_t)(size - 1));
	dst[size - 1] = '\0';
	return ret;
}

void hudMgrSetRenderTargetSize(void *mgr, uint32_t w, uint32_t h)
{
	if (w != 0)
		((HudManager *)mgr)->rtWidth = w;
	if (h != 0)
		((HudManager *)mgr)->rtHeight = h;
}

void hudMgrGetRenderTargetSize(void *mgr, uint32_t *w, uint32_t *h)
{
	*w = ((HudManager *)mgr)->rtWidth;
	*h = ((HudManager *)mgr)->rtHeight;
}

void hudMgrSetCoreCounts(void *mgr, uint32_t cores, uint32_t firmware)
{
	((HudManager *)mgr)->hudCores = cores;
	((HudManager *)mgr)->hudFirmware = firmware;
}

void hudMgrGetCoreCounts(void *mgr, uint32_t *cores, uint32_t *firmware)
{
	*cores = ((HudManager *)mgr)->hudCores;
	*firmware = ((HudManager *)mgr)->hudFirmware;
}

int hudMgrPollHostTool(void *mgr)
{
	HudManager *hm = (HudManager *)mgr;
	char *pathBuf = hm->hostConfigPath;

	if (hudHostToolPollStart(pathBuf, 512U) != 0) {
		void *saved = hm->savedConfig;
		if (saved != NULL)
			hudDelete(saved);
		saved = hudNew(sizeof(HudConfig));
		hudConfigCtor(saved);
		hm->savedConfig = saved;
		hudConfigSaveFromManager(saved, mgr);
		sceClibPrintf("[SceRazorHud]:Starting GPU trace requested from Razor host tool.\n");
		hudReloadConfigPath(pathBuf);
		hm->traceNeedStart = 1;
		hm->traceTriggered = 0;
	}
	if (hudHostToolPollTrigger() != 0) {
		sceClibPrintf("[SceRazorHud]:GPU trace triggered from Razor host tool.\n");
		hm->traceTriggered = 1;
	}
	if (hudHostToolPollCancel() != 0) {
		void *saved = hm->savedConfig;
		sceClibPrintf("[SceRazorHud]:GPU trace cancelled by Razor host tool.\n");
		if (saved != NULL) {
			hudConfigApplyToManager(saved, mgr);
			hudDelete(hm->savedConfig);
			hm->savedConfig = NULL;
		}
		hm->traceNeedStart = 1;
		hm->traceTriggered = 0;
		hudHostToolNotifyDone(0);
		return 1;
	}
	return 0;
}

void hudMgrOnRingOverflow(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD)
{
	void *bufSrc = ((HudManager *)mgr)->gpuBufSrc;
	void *gpuTl = ((HudManager *)mgr)->gpuTimelineSrc;
	if (bufSrc != NULL)
		hudGpuBufSrcOnOverflow(bufSrc, ringArgA, ringArgB, ringArgC, (uint8_t)ringArgD);
	if (gpuTl != NULL)
		hudGpuTimelineNull4();
}

void hudMgrOnRingInit(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD)
{
	void *bufSrc = ((HudManager *)mgr)->gpuBufSrc;
	void *gpuTl = ((HudManager *)mgr)->gpuTimelineSrc;
	if (bufSrc != NULL)
		hudGpuBufSrcOnInit(bufSrc, (uint32_t)ringArgA, (uint32_t)ringArgB, (uint32_t)ringArgC, (uint32_t)ringArgD);
	if (gpuTl != NULL)
		hudGpuTimelineNull2();
}

void hudMgrOnRingUpdate(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD)
{
	void *bufSrc = ((HudManager *)mgr)->gpuBufSrc;
	void *gpuTl = ((HudManager *)mgr)->gpuTimelineSrc;
	if (bufSrc != NULL)
		hudGpuBufSrcOnUpdate(bufSrc, (uint32_t)ringArgA, (uint32_t)ringArgB, (uint32_t)ringArgC, (uint32_t)ringArgD);
	if (gpuTl != NULL)
		hudGpuTimelineNull3();
}

int hudMgrTraceTrigger(void *mgr)
{
	HudManager *hm = (HudManager *)mgr;
	if (hm->perfMode != 2)
		return HUD_ERR_WRONG_MODE;
	hm->traceStopFrame = 0;
	hm->traceTriggered = 1;
	return 0;
}

int hudMgrTraceRingBufferSize(void *mgr)
{
	HudManager *hm = (HudManager *)mgr;
	uint32_t info[2];
	void *pa;

	if (hm->perfMode != 2)
		return HUD_ERR_WRONG_MODE;
	pa = hm->paMgr;
	if (pa == NULL)
		return HUD_ERR_INTERNAL;
	hudPaGetBufferInfo(pa, info);
	return (int)info[1];
}

int hudMgrTraceRingBufferUsed(void *mgr)
{
	HudManager *hm = (HudManager *)mgr;
	void *pa;

	if (hm->perfMode != 2)
		return HUD_ERR_WRONG_MODE;
	pa = hm->paMgr;
	if (pa == NULL)
		return HUD_ERR_INTERNAL;
	return hudPaGetTraceUsed(pa);
}

char *hudConfigSetBaseFilename(char *cfgPtr, const char *base)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;
	uint32_t len;

	sceClibStrncpy(cfg->baseFilename, base, 1023U);
	sceClibMemset(cfg->txtFilename, 0, sizeof(cfg->txtFilename));
	sceClibMemset(cfg->csvFilename, 0, sizeof(cfg->csvFilename));
	sceClibMemset(cfg->xmlFilename, 0, sizeof(cfg->xmlFilename));

	sceClibStrncpy(cfg->txtFilename, base, 1023U);
	len = sceClibStrnlen(cfg->txtFilename, sizeof(cfg->txtFilename));
	sceClibStrncat(cfg->txtFilename, ".txt", 1023U - len);

	sceClibStrncpy(cfg->csvFilename, base, 1023U);
	len = sceClibStrnlen(cfg->csvFilename, sizeof(cfg->csvFilename));
	sceClibStrncat(cfg->csvFilename, ".csv", 1023U - len);

	sceClibStrncpy(cfg->xmlFilename, base, 1023U);
	len = sceClibStrnlen(cfg->xmlFilename, sizeof(cfg->xmlFilename));
	return sceClibStrncat(cfg->xmlFilename, ".xml", 1023U - len);
}

void *hudConfigCtor(void *cfgPtr)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;

	cfg->flags = 0x51U;
	cfg->perfMode = 1U;
	hudConfigSetBaseFilename((char *)cfg, "perflog");
	cfg->rtWidth = 0;
	cfg->rtHeight = 0;
	cfg->traceMask = ~(uint64_t)0;
	cfg->traceRate = 999U;
	cfg->traceBufSize = 0x2000000U;
	cfg->traceStopFrame = 0;
	sceClibStrncpy(cfg->traceFilename, "", 1023U);
	cfg->traceCountLen = 0;
	cfg->hudCores = 1U;
	cfg->hudFirmware = 1U;
	cfg->displayFrames = 1U;
	cfg->sceneColorMode = 0;
	sceClibMemcpy_safe(cfg->traceGroups, &g_gpuTraceMetricsTable[0][2], 0x20U);
	sceClibMemcpy_safe(cfg->traceCounters, &g_gpuTraceMetricsTable[0][10], 0x20U);
	cfg->liveMetricsGroup = 0;
	cfg->liveEnabled = 0;
	return cfg;
}

void *hudConfigDisableAll(void *cfgPtr)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;
	cfg->flags = 0;
	cfg->perfMode = 1U;
	return cfg;
}

int hudConfigLookupMetricsGroup(uint32_t group, void *dst)
{
	if (dst == NULL || group > 53U)
		return HUD_ERR_INVALID_PARAM;
	sceClibMemcpy_safe(dst, &g_gpuTraceMetricsTable[group][2], 0x20U);
	uint32_t *dstWords = (uint32_t *)dst;
	sceClibMemcpy_safe(&dstWords[8], &g_gpuTraceMetricsTable[group][10], 0x20U);
	return 0;
}

void hudConfigSaveFromManager(void *cfgPtr, void *mgr)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;

	cfg->flags = hudMgrGetFlags(mgr);
	hudMgrGetTraceConfig(mgr, cfg->traceGroups, &cfg->traceMask, &cfg->traceRate,
		&cfg->traceBufSize, &cfg->traceStopFrame);
	cfg->liveMetricsGroup = (uint32_t)hudMgrLiveGetMetricsGroup(mgr);
	cfg->liveEnabled = (uint8_t)hudMgrLiveIsRunning(mgr);
	hudMgrGetTxtFilename(mgr, cfg->txtFilename, 1024);
	hudMgrGetCsvFilename(mgr, cfg->csvFilename, 1024);
	hudMgrGetXmlFilename(mgr, cfg->xmlFilename, 1024);
	cfg->displayFrames = (uint32_t)hudMgrGetDisplayFrameCount(mgr);
	cfg->sceneColorMode = (uint32_t)hudMgrGetSceneColorMode(mgr);
	hudMgrGetRenderTargetSize(mgr, &cfg->rtWidth, &cfg->rtHeight);
	hudMgrGetCoreCounts(mgr, &cfg->hudCores, &cfg->hudFirmware);
	cfg->perfMode = (uint32_t)hudMgrGetPerfMode(mgr);
	hudMgrTraceGetFilename(mgr, cfg->traceFilename, 1024, &cfg->traceCountLen);
}

int hudConfigApplyToManager(void *cfgPtr, void *mgr)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;
	int ret;

	if ((cfg->flags & 0x0EU) != 0)
		cfg->flags = (cfg->flags & ~0x50U) | 0x10U;
	if (cfg->perfMode == 1U && (cfg->flags & 0x50U) != 0)
		cfg->perfMode = 4U;

	hudMgrStopAll(mgr, 0);
	hudMgrRefreshTime();
	hudMgrSetFlags(mgr, cfg->flags);
	hudMgrSetTxtFilename(mgr, cfg->txtFilename);
	hudMgrSetCsvFilename(mgr, cfg->csvFilename);
	hudMgrSetXmlFilename(mgr, cfg->xmlFilename);
	hudMgrSetDisplayFrameCount(mgr, (int)cfg->displayFrames);
	hudMgrSetSceneColorMode(mgr, (int)cfg->sceneColorMode);
	hudMgrSetRenderTargetSize(mgr, cfg->rtWidth, cfg->rtHeight);
	hudMgrSetCoreCounts(mgr, cfg->hudCores, cfg->hudFirmware);
	hudMgrSetPerfMode(mgr, (int)cfg->perfMode);
	hudMgrTraceSetFilename(mgr, cfg->traceFilename, cfg->traceCountLen);
	hudMgrSetTraceConfig(mgr, cfg->traceGroups, cfg->traceMask, cfg->traceRate,
		cfg->traceBufSize, cfg->traceStopFrame);
	ret = hudMgrLiveSetMetricsGroup(mgr, 0, cfg->liveMetricsGroup);
	if (cfg->liveEnabled != 0)
		return hudMgrLiveStart(mgr, 0);
	return ret;
}

void hudConfigApplyLine(void *cfgPtr, uint32_t lineNo, const char *key, const char *val, const char *valUpper)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;
	int64_t num;
	int i;

	if (key[0] == '\0' || val[0] == '\0')
		return;

	num = sceClibStrtoll(val, NULL, 0);
	for (i = 0; i < 29; i++) {
		if (sceClibStrncmp(key, g_hudConfigKeys[i].name, 64U) == 0)
			break;
	}

	switch (i) {
	case 1:
		cfg->flags = (num != 0) ? (cfg->flags | 1U) : (cfg->flags & ~1U);
		return;
	case 2:
		cfg->flags = (num != 0) ? (cfg->flags | 2U) : (cfg->flags & ~2U);
		return;
	case 3:
		cfg->flags = (num != 0) ? (cfg->flags | 4U) : (cfg->flags & ~4U);
		return;
	case 4:
		cfg->flags = (num != 0) ? (cfg->flags | 8U) : (cfg->flags & ~8U);
		return;
	case 5:
		cfg->flags = (num != 0) ? (cfg->flags | 0x20U) : (cfg->flags & ~0x20U);
		return;
	case 6:
		hudConfigSetBaseFilename((char *)cfg, val);
		return;
	case 7:
		cfg->rtWidth = (uint32_t)num;
		return;
	case 8:
		cfg->rtHeight = (uint32_t)num;
		return;
	case 9:
		if (num != 0) {
			cfg->flags |= 0x50U;
			cfg->perfMode = 4U;
		} else {
			cfg->flags &= ~0x50U;
		}
		return;
	case 10:
		if (num != 0) {
			cfg->flags |= 0x10U;
			cfg->perfMode = 4U;
		} else {
			cfg->flags &= ~0x10U;
		}
		return;
	case 11:
		if (num != 0) {
			cfg->flags |= 0x40U;
			cfg->perfMode = 4U;
		} else {
			cfg->flags &= ~0x40U;
		}
		return;
	case 12:
		cfg->hudCores = ((uint32_t)num == 1U || (uint32_t)num == 4U) ? (uint32_t)num : 1U;
		return;
	case 13:
		cfg->hudFirmware = ((uint32_t)num <= 1U || (uint32_t)num == 4U) ? (uint32_t)num : 1U;
		return;
	case 14:
		cfg->displayFrames = (uint32_t)num;
		return;
	case 15:
		cfg->sceneColorMode = (uint32_t)num;
		return;
	case 16:
		if (num != 0) {
			cfg->perfMode = 2U;
			cfg->flags &= ~0x50U;
		} else if (cfg->perfMode == 2U) {
			cfg->perfMode = 1U;
		}
		return;
	case 17:
		if (sceClibStrncmp(valUpper, "HIGH", 10U) == 0)
			cfg->traceRate = 333U;
		else if (sceClibStrncmp(valUpper, "MEDIUM", 10U) == 0)
			cfg->traceRate = 999U;
		else if (sceClibStrncmp(valUpper, "LOW", 10U) == 0)
			cfg->traceRate = 3330U;
		else
			sceClibPrintf("[SceRazorHud]:Invalid value at line %u in hud_settings.ini (%s)\n", lineNo, val);
		return;
	case 18:
		cfg->traceRate = (uint32_t)num;
		return;
	case 19:
		if (sceClibStrncmp(valUpper, "HIGH", 10U) == 0)
			cfg->traceBufSize = 0x6000000U;
		else if (sceClibStrncmp(valUpper, "MEDIUM", 10U) == 0)
			cfg->traceBufSize = 0x2000000U;
		else if (sceClibStrncmp(valUpper, "LOW", 10U) == 0)
			cfg->traceBufSize = 0x800000U;
		else
			sceClibPrintf("[SceRazorHud]:Invalid value at line %u in hud_settings.ini (%s)\n", lineNo, val);
		return;
	case 20:
		cfg->traceBufSize = ((uint32_t)num) << 20;
		return;
	case 21:
		if ((uint32_t)num <= 53U) {
			sceClibMemcpy_safe(cfg->traceGroups, &g_gpuTraceMetricsTable[(uint32_t)num][2], 0x20U);
			sceClibMemcpy_safe(cfg->traceCounters, &g_gpuTraceMetricsTable[(uint32_t)num][10], 0x20U);
		}
		return;
	case 22:
		cfg->traceStopFrame = (uint32_t)num;
		return;
	case 23: {
		uint64_t u = (uint64_t)num;
		cfg->traceGroups[0] = (uint32_t)((u >> 56) & 0xFFU);
		cfg->traceGroups[1] = (uint32_t)((u >> 48) & 0xFFU);
		cfg->traceGroups[2] = (uint32_t)((u >> 40) & 0xFFU);
		cfg->traceGroups[3] = (uint32_t)((u >> 32) & 0xFFU);
		cfg->traceGroups[4] = (uint32_t)((u >> 24) & 0xFFU);
		cfg->traceGroups[5] = (uint32_t)((u >> 16) & 0xFFU);
		cfg->traceGroups[6] = (uint32_t)((u >> 8) & 0xFFU);
		cfg->traceGroups[7] = (uint32_t)(u & 0xFFU);
		return;
	}
	case 24: {
		uint64_t u = (uint64_t)num;
		cfg->traceCounters[0] = (uint32_t)((u >> 56) & 0xFFU);
		cfg->traceCounters[1] = (uint32_t)((u >> 48) & 0xFFU);
		cfg->traceCounters[2] = (uint32_t)((u >> 40) & 0xFFU);
		cfg->traceCounters[3] = (uint32_t)((u >> 32) & 0xFFU);
		cfg->traceCounters[4] = (uint32_t)((u >> 24) & 0xFFU);
		cfg->traceCounters[5] = (uint32_t)((u >> 16) & 0xFFU);
		cfg->traceCounters[6] = (uint32_t)((u >> 8) & 0xFFU);
		cfg->traceCounters[7] = (uint32_t)(u & 0xFFU);
		return;
	}
	case 25:
		sceClibStrncpy(cfg->traceFilename, val, 1023U);
		return;
	case 26:
		cfg->traceCountLen = (uint32_t)num;
		return;
	case 27:
		if (num != 0) {
			cfg->perfMode = 3U;
			cfg->flags &= ~0x10U;
		} else if (cfg->perfMode == 3U) {
			cfg->perfMode = 1U;
		}
		return;
	case 28:
		if ((uint32_t)num <= 3U)
			cfg->liveMetricsGroup = (uint32_t)num;
		return;
	default:
		sceClibPrintf("[SceRazorHud]:Unknown parameter at line %u in hud_settings.ini\n", lineNo);
		return;
	}
}

int hudConfigLoadFile(void *cfgPtr, const char *path)
{
	HudConfig *cfg = (HudConfig *)cfgPtr;
	int fd, nread;
	int64_t fsize;
	char *buf, *p, *keyBuf, *valBuf, *valUpper;
	uint32_t lineNo;

	fd = sceIoOpen(path, 1, 0);
	if (fd < 0)
		return 0;
	fsize = sceIoLseek(fd, 0, 2);
	if (fsize < 1) {
		sceIoClose(fd);
		return 1;
	}
	sceIoLseek(fd, 0, 0);
	buf = (char *)hudNewArray((uint32_t)fsize + 1U);
	nread = sceIoRead(fd, buf, (uint32_t)fsize);
	sceIoClose(fd);
	buf[fsize] = '\0';
	if (nread != (int)fsize) {
		sceClibPrintf("[SceRazorHud]:Error reading file \"%s\".\n", path);
		if (buf != NULL)
			hudDeleteArray(buf);
		return 1;
	}

	sceClibPrintf("[SceRazorHud]:Found Razor config file \"%s\".\n", path);
	keyBuf = (char *)hudNewArray(256U);
	valUpper = (char *)hudNewArray(256U);
	valBuf = (char *)hudNewArray(256U);
	p = buf;
	lineNo = 1U;

	while (*p != '\0') {
		char *kEnd, *vEnd, *c;
		while (*p != '\0' && (sceClibLookCtypeTable((unsigned char)*p) & 8U) != 0) {
			if (*p == '\n')
				lineNo++;
			p++;
		}
		if (*p == '\0')
			break;
		if (*p == '#') {
			while (*p != '\0' && *p != '\n')
				p++;
		}
		if (*p == '\n') {
			lineNo++;
			p++;
			continue;
		}
		kEnd = keyBuf;
		while (*p != '\0' && *p != '=' && *p != '\n')
			*kEnd++ = *p++;
		while (kEnd > keyBuf && (sceClibLookCtypeTable((unsigned char)kEnd[-1]) & 8U) != 0)
			kEnd--;
		while (*p == '=')
			p++;
		while (*p != '\0' && *p != '\n' && (sceClibLookCtypeTable((unsigned char)*p) & 8U) != 0)
			p++;
		vEnd = valBuf;
		while (*p != '\0' && *p != '\n')
			*vEnd++ = *p++;
		for (c = vEnd - 1; c >= valBuf; c--) {
			if (*c == '#') {
				vEnd = c;
				break;
			}
		}
		while (vEnd > valBuf && (sceClibLookCtypeTable((unsigned char)vEnd[-1]) & 8U) != 0)
			vEnd--;
		*kEnd = '\0';
		*vEnd = '\0';
		for (c = keyBuf; *c != '\0'; c++)
			*c = (char)sceClibToupper((unsigned char)*c);
		{
			char *u = valUpper;
			for (c = valBuf; *c != '\0'; c++)
				*u++ = (char)sceClibToupper((unsigned char)*c);
			*u = '\0';
		}
		hudConfigApplyLine(cfg, lineNo, keyBuf, valBuf, valUpper);
	}

	if ((cfg->flags & 1U) == 0)
		cfg->perfMode = 1U;
	if (valUpper != NULL)
		hudDeleteArray(valUpper);
	if (valBuf != NULL)
		hudDeleteArray(valBuf);
	if (keyBuf != NULL)
		hudDeleteArray(keyBuf);
	if (buf != NULL)
		hudDeleteArray(buf);
	return 1;
}
