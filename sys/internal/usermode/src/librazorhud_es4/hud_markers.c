/*
 * CPU user marker thread filter, Razor host tool IPC, and utility helpers.
 */
#include "razor_hud_internal.h"

typedef struct HudThreadFilterEntry {
	int threadId;
	uint8_t enabled;
	uint8_t reserved[3];
} HudThreadFilterEntry;

typedef struct HudThreadFilter {
	uint32_t count;
	uint32_t reserved;
	HudThreadFilterEntry entries[100];
	uint8_t defaultEnable;
	uint8_t pad[3];
} HudThreadFilter;

typedef struct HudHostToolCmd {
	uint32_t flags;
	char configPath[512];
} HudHostToolCmd;

typedef struct HudHostToolInfo {
	uint32_t active;
	uint32_t size;
	void (*markerFn)(void);
	void (*hookFn)(void);
	HudHostToolCmd *cmdBuf;
	uint32_t cmdBufSize;
	uint64_t *statusPtr;
	uint32_t statusSize;
} HudHostToolInfo;

static HudThreadFilter *g_threadFilter;
static HudHostToolCmd g_hostToolCmd;
static uint64_t g_hostToolStatus;

HudHostToolInfo g_hostToolInfo = {
	0,
	sizeof(HudHostToolInfo),
	hudHostToolMarker,
	hudHostToolHook,
	&g_hostToolCmd,
	sizeof(HudHostToolCmd),
	&g_hostToolStatus,
	sizeof(uint64_t),
};

void hudThreadFilterEnableAll(void *filterPtr, uint8_t enable)
{
	HudThreadFilter *f = (HudThreadFilter *)filterPtr;

	f->defaultEnable = enable;
	f->count = 0;
}

int hudThreadFilterIsEnabled(void *filterPtr, int threadId)
{
	HudThreadFilter *f = (HudThreadFilter *)filterPtr;
	uint32_t i;

	for (i = 0; i < f->count; i++) {
		if (f->entries[i].threadId == threadId)
			return f->entries[i].enabled;
	}
	return f->defaultEnable;
}

int hudThreadFilterSetThread(void *filterPtr, int threadId, uint8_t enable)
{
	HudThreadFilter *f = (HudThreadFilter *)filterPtr;
	uint32_t i;

	for (i = 0; i < f->count; i++) {
		if (f->entries[i].threadId == threadId) {
			f->entries[i].enabled = enable;
			return 0;
		}
	}
	if (f->count >= 100)
		return HUD_ERR_ENOMEM;
	f->entries[f->count].threadId = threadId;
	f->entries[f->count].enabled = enable;
	f->count++;
	return 0;
}

void *hudThreadFilterGetInstance(void)
{
	HudThreadFilter *f;
	uint32_t i;

	if (g_threadFilter != NULL)
		return g_threadFilter;
	f = (HudThreadFilter *)hudNew(sizeof(HudThreadFilter));
	f->count = 0;
	f->defaultEnable = 1;
	for (i = 0; i < 100; i++) {
		f->entries[i].threadId = 0;
		f->entries[i].enabled = 0;
	}
	g_threadFilter = f;
	return f;
}

void hudHostToolSetStatus(uint64_t status)
{
	g_hostToolStatus = status;
	g_hostToolCmd.flags &= ~2U;
}

void hudHostToolNotifyDone(uint64_t status)
{
	g_hostToolCmd.flags = 0;
	g_hostToolStatus = status;
	hudHostToolHook();
}

int hudHostToolPollStart(void *outPath, uint32_t maxLen)
{
	uint32_t hasStart;

	g_hostToolInfo.active = 1;
	hasStart = g_hostToolCmd.flags & 1U;
	if (hasStart != 0) {
		if (outPath != NULL) {
			sceClibMemset(outPath, 0, maxLen);
			sceClibStrncpy((char *)outPath, g_hostToolCmd.configPath, maxLen - 1);
			g_hostToolCmd.configPath[maxLen - 1] = '\0';
		}
		g_hostToolCmd.flags &= ~1U;
	}
	return (int)hasStart;
}

int hudHostToolPollTrigger(void)
{
	if ((g_hostToolCmd.flags & 2U) != 0) {
		g_hostToolCmd.flags &= ~2U;
		return 1;
	}
	return 0;
}

int hudHostToolPollCancel(void)
{
	if ((g_hostToolCmd.flags & 4U) != 0) {
		g_hostToolCmd.flags = 0;
		return 1;
	}
	return 0;
}

void *hudMemmoveWrap(void *dst, const void *src, uint32_t n)
{
	return sceClibMemmove(dst, src, n);
}

void *hudMemsetWrap(void *dst, int c, uint32_t n)
{
	return sceClibMemset(dst, c, n);
}
