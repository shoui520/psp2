/*
 * SceRazorHud module entry points, public exports, and GXM callback bridge.
 */
#include "razor_hud_internal.h"

void *g_hudManager;
uint32_t g_paIoBaseAddress;
uint8_t g_userMarkerBufCustom;
void *g_userMarkerBufPtr;
uint32_t g_userMarkerBufSize;
uint64_t g_hudLastReloadTime;

static char g_int64FmtRing[8][32];
static uint32_t g_int64FmtIdx;

uint64_t hudGetProcessTimeAndStore(void)
{
	uint64_t t = sceKernelGetProcessTimeWide();
	g_hudLastReloadTime = t;
	return t;
}

uint32_t hudFindParamResourceIndex(const SceGxmProgram *prog, const char *name)
{
	const SceGxmProgramParameter *param = sceGxmProgramFindParameterByName(prog, name);
	return sceGxmProgramParameterGetResourceIndex(param);
}

const SceGxmProgramParameter *hudFindProgramParam(SceGxmRegisteredProgram *progId, const char *name)
{
	const SceGxmProgram *prog = sceGxmShaderPatcherGetProgramFromId(progId);
	return sceGxmProgramFindParameterByName(prog, name);
}

const char *hudFormatInt64Commas(int64_t val)
{
	char *buf;
	int len, pos;

	g_int64FmtIdx = (g_int64FmtIdx + 1U) % 8U;
	buf = g_int64FmtRing[g_int64FmtIdx];
	sceClibSnprintf(buf, 32U, "%lld", val);
	len = (int)sceClibStrnlen(buf, 32U);
	pos = len - 3;
	while (pos > 0) {
		if (len + 1 > pos)
			hudMemmoveWrap(buf + pos + 1, buf + pos, (uint32_t)(len + 1 - pos));
		buf[pos] = ',';
		if (pos <= 3)
			break;
		pos -= 3;
		len = (int)sceClibStrnlen(buf, 32U);
	}
	return buf;
}

int hudGuardAcquire(uint32_t *guard)
{
	return *(volatile uint8_t *)guard == 0;
}

void hudGuardRelease(uint32_t *guard)
{
	*(volatile uint8_t *)guard = 1;
}

static int hudCbTraceTrigger(void)
{
	if (g_hudManager != NULL)
		return hudMgrTraceTrigger(g_hudManager);
	return 0;
}

static void hudCbRingOverflow(int a1, int a2, int a3, int a4)
{
	if (g_hudManager != NULL)
		hudMgrOnRingOverflow(g_hudManager, a1, a2, a3, a4);
}

static void hudCbRingUpdate(int a1, int a2, int a3, int a4)
{
	if (g_hudManager != NULL)
		hudMgrOnRingUpdate(g_hudManager, a1, a2, a3, a4);
}

static void hudCbRingInit(int a1, int a2, int a3, int a4)
{
	if (g_hudManager != NULL)
		hudMgrOnRingInit(g_hudManager, a1, a2, a3, a4);
}

static void hudCbOnSwap(const SceGxmColorSurface *surf, SceGxmSyncObject *sync)
{
	if (g_hudManager != NULL)
		hudMgrOnSwap(g_hudManager, surf, sync);
}

static void hudCbPollHostTool(void)
{
	if (g_hudManager != NULL)
		hudMgrPollHostTool(g_hudManager);
}

static void hudCbSetGxmVersions(uint32_t v0, uint32_t v1)
{
	if (g_hudManager != NULL)
		hudMgrSetGxmVersions(g_hudManager, v0, v1);
}

static void hudCbTerm(void)
{
	if (g_hudManager != NULL) {
		hudMgrStopAll(g_hudManager, 1);
		if (g_hudManager != NULL) {
			hudMgrDtor(g_hudManager);
			hudDelete(g_hudManager);
		}
		g_hudManager = NULL;
	}
	hudMemTerm();
}

int hudLoadDefaultConfig(void *cfg)
{
	char *path = (char *)hudNewArray(1024U);
	int ok;

	sceClibStrncpy(path, "host0:", 1024U);
	sceClibStrncat(path, "hud_settings.ini", 1024U);
	path[1023] = '\0';
	if (hudConfigLoadFile(cfg, path) != 0) {
		hudDeleteArray(path);
		return 1;
	}

	sceClibStrncpy(path, "app0:", 1024U);
	sceClibStrncat(path, "hud_settings.ini", 1024U);
	path[1023] = '\0';
	if (hudConfigLoadFile(cfg, path) != 0) {
		hudDeleteArray(path);
		return 1;
	}

	sceClibStrncpy(path, "sd0:", 1024U);
	sceClibStrncat(path, "hud_settings.ini", 1024U);
	path[1023] = '\0';
	if (hudConfigLoadFile(cfg, path) != 0) {
		hudDeleteArray(path);
		return 1;
	}

	sceClibStrncpy(path, "ux0:data/", 1024U);
	sceClibStrncat(path, "hud_settings.ini", 1024U);
	path[1023] = '\0';
	ok = hudConfigLoadFile(cfg, path);
	hudDeleteArray(path);
	return ok;
}

static void hudCbInit(uint32_t param)
{
	void *mgr;
	void *cfg;

	hudMemInit();
	mgr = hudNew(2080U);
	hudMgrCtor(mgr);
	g_hudManager = mgr;
	cfg = hudNew(5256U);
	hudConfigCtor(cfg);
	hudLoadDefaultConfig(cfg);
	hudConfigApplyToManager(cfg, g_hudManager);
	hudMgrSetParamBufSize(g_hudManager, param);
	hudDelete(cfg);
}

int module_start(uint32_t arglen, const void *argp)
{
	void *callbacks[10];

	(void)arglen;
	(void)argp;
	callbacks[0] = (void *)hudCbInit;
	callbacks[1] = (void *)hudCbTerm;
	callbacks[2] = (void *)hudCbSetGxmVersions;
	callbacks[3] = (void *)hudCbPollHostTool;
	callbacks[4] = (void *)hudCbOnSwap;
	callbacks[5] = (void *)hudCbRingOverflow;
	callbacks[6] = (void *)hudCbRingInit;
	callbacks[7] = (void *)hudCbRingUpdate;
	callbacks[8] = (void *)hudCbTraceTrigger;
	callbacks[9] = &g_paIoBaseAddress;
	sceGxmSetCallbackInternal(0x10005U, callbacks);
	return 0;
}

int module_stop(uint32_t arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sceGxmSetCallbackInternal(0U, NULL);
	return 0;
}

int SceRazorHudInternalForReplay_7C4895CA(void)
{
	if (g_hudManager != NULL) {
		void *cfg = hudNew(5256U);
		hudConfigCtor(cfg);
		hudConfigDisableAll(cfg);
		hudConfigApplyToManager(cfg, g_hudManager);
		hudDelete(cfg);
	}
	return 0;
}

int hudReloadConfigPath(const char *path)
{
	void *cfg;

	if (g_hudManager == NULL)
		return 0;
	cfg = hudNew(5256U);
	hudConfigCtor(cfg);
	if (path != NULL) {
		if (hudConfigLoadFile(cfg, path) != 0) {
			hudConfigApplyToManager(cfg, g_hudManager);
			hudDelete(cfg);
			return 0;
		}
	} else if (hudLoadDefaultConfig(cfg) != 0) {
		hudConfigApplyToManager(cfg, g_hudManager);
		hudDelete(cfg);
		return 0;
	}
	hudDelete(cfg);
	return HUD_ERR_CONFIG_NOT_FOUND;
}

int sceRazorHudReloadSettings(void)
{
	void *cfg;

	if (g_hudManager == NULL)
		return 0;
	cfg = hudNew(5256U);
	hudConfigCtor(cfg);
	if (hudLoadDefaultConfig(cfg) != 0) {
		hudConfigApplyToManager(cfg, g_hudManager);
		hudDelete(cfg);
		return 0;
	}
	hudDelete(cfg);
	return HUD_ERR_CONFIG_NOT_FOUND;
}

int sceRazorGpuPerfGetMode(void)
{
	if (g_hudManager != NULL)
		return hudMgrGetPerfMode(g_hudManager);
	return 0;
}

int hudLiveSetMetricsGroupInternal(int internal, uint32_t metrics)
{
	if (g_hudManager != NULL)
		return hudMgrLiveSetMetricsGroup(g_hudManager, internal, metrics);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuLiveSetMetricsGroup(uint32_t metrics)
{
	if (g_hudManager != NULL)
		return hudMgrLiveSetMetricsGroup(g_hudManager, 0, metrics);
	return HUD_ERR_NOT_INITIALIZED;
}

int hudLiveStartInternal(int internal)
{
	if (g_hudManager != NULL)
		return hudMgrLiveStart(g_hudManager, internal);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuLiveStart(void)
{
	if (g_hudManager != NULL)
		return hudMgrLiveStart(g_hudManager, 0);
	return HUD_ERR_NOT_INITIALIZED;
}

int hudLiveStopInternal(int internal)
{
	if (g_hudManager != NULL)
		return hudMgrLiveStop(g_hudManager, internal);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuLiveStop(void)
{
	if (g_hudManager != NULL)
		return hudMgrLiveStop(g_hudManager, 0);
	return HUD_ERR_NOT_INITIALIZED;
}

int hudLiveSetBufferInternal(int internal, void *buffer, uint32_t size, void *resultsPtr)
{
	SceRazorGpuLiveResultInfo *results = (SceRazorGpuLiveResultInfo *)resultsPtr;

	if (results != NULL) {
		results->resultData = NULL;
		results->entryCount = 0;
		results->overflowCount = 0;
		results->bufferSize = 0;
		results->startTime = 0;
		results->endTime = 0;
		results->metricGroup = 0;
	}
	if (g_hudManager != NULL)
		return hudMgrLiveSetBuffer(g_hudManager, internal, buffer, size, results);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuLiveSetBuffer(void *buffer, uint32_t bufSize, SceRazorGpuLiveResultInfo *results)
{
	if (results != NULL) {
		results->resultData = NULL;
		results->entryCount = 0;
		results->overflowCount = 0;
		results->bufferSize = 0;
		results->startTime = 0;
		results->endTime = 0;
		results->metricGroup = 0;
	}
	if (g_hudManager != NULL)
		return hudMgrLiveSetBuffer(g_hudManager, 0, buffer, bufSize, results);
	return HUD_ERR_NOT_INITIALIZED;
}

void hudGxmFinishInternal(void)
{
	if (g_hudManager != NULL)
		hudMgrGxmFinish(g_hudManager);
}

int sceRazorGpuLiveTriggerDebugDump(void)
{
	SceDateTime dt;
	char stamp[32];
	char path[128];
	int ret;

	sceClibMemset(&dt, 0, sizeof(dt));
	sceRtcGetCurrentClockLocalTime(&dt);
	sceClibSnprintf(stamp, sizeof(stamp), "%04d_%02d_%02d_%02d%02d%02d",
		dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
	sceClibSnprintf(path, sizeof(path), "host0:gpu_live_metrics_debug_trace_buffer_%s.pmon", stamp);

	if (g_hudManager == NULL)
		return HUD_ERR_NOT_INITIALIZED;

	ret = hudMgrLiveStop(g_hudManager, 0);
	if (ret == 0) {
		int dumpRet = hudMgrLiveDump(g_hudManager, 0, path);
		ret = (g_hudManager != NULL) ? hudMgrLiveStart(g_hudManager, 0) : HUD_ERR_NOT_INITIALIZED;
		if (dumpRet != 0)
			ret = dumpRet;
	}
	return ret;
}

int sceRazorGpuTraceSetFilename(const char *filename, SceInt32 countLen)
{
	if (g_hudManager != NULL)
		return hudMgrTraceSetFilename(g_hudManager, filename, (uint32_t)countLen);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuTraceSetMetricsGroup(SceInt32 metrics)
{
	if (g_hudManager != NULL)
		return hudMgrTraceSetMetricsGroup(g_hudManager, (int)metrics);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorHudSetDisplayEnabled(int enabled)
{
	if (g_hudManager != NULL)
		return hudMgrSetDisplayEnabled(g_hudManager, (uint8_t)enabled);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorHudSetDisplayFrameCount(int frameCount)
{
	if (g_hudManager != NULL)
		return hudMgrSetDisplayFrameCount(g_hudManager, frameCount);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuTraceTrigger(void)
{
	if (g_hudManager != NULL)
		return hudMgrTraceTrigger(g_hudManager);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuTraceRingBufferSize(void)
{
	if (g_hudManager != NULL)
		return hudMgrTraceRingBufferSize(g_hudManager);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorGpuTraceRingBufferUsed(void)
{
	if (g_hudManager != NULL)
		return hudMgrTraceRingBufferUsed(g_hudManager);
	return HUD_ERR_NOT_INITIALIZED;
}

int sceRazorCpuHudUserMarkerEnableThreadId(int threadId, int enable)
{
	void *filter = hudThreadFilterGetInstance();
	return hudThreadFilterSetThread(filter, threadId, enable != 0 ? 1 : 0);
}

int sceRazorCpuHudUserMarkerEnableAllThreads(int enable)
{
	void *filter = hudThreadFilterGetInstance();
	hudThreadFilterEnableAll(filter, enable != 0 ? 1 : 0);
	return 0;
}

int sceRazorCpuHudSetUserMarkerTraceBuffer(void *buffer, uint32_t size)
{
	return hudMgrSetUserMarkerBuffer(buffer, size);
}
