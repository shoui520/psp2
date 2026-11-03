/* Application events delivered to a process. */
#pragma once

#include <fw.h>
#include <stddef.h>
#include <sysmem/debug.h>
#include <scetypes.h>
#include <appmgr.h>

typedef struct SceAppMgrEvent {
	int event;
	SceUID appId;
	char param[56];
} SceAppMgrEvent;

_Static_assert(sizeof(SceAppMgrEvent) == 64, "application event size");
_Static_assert(offsetof(SceAppMgrEvent, appId) == 4 &&
	       offsetof(SceAppMgrEvent, param) == 8,
	       "application event fields");

/* Sony's record for the event queueing helper it inlines from this header: the
 * file name in the debug record is this one, not the inlining source's. */
static const SceKernelDebugInfo utilSendEventInfo = {
	.fileHash = 0x058a9649,
	.lineHash = 0x1ee6aa03,
	.funcHash = 0x14aa8dfe,
#if FW_DEBUG
	.file = "appmgr_app_event.h",
	.line = 50,
	.func = "utilSendEvent",
#endif
};

int sceAppMgrReceiveEventNum(int *eventNum);
int sceAppMgrReceiveEventInternal(void *appEvent);
int sceAppMgrSendSystemEvent2(const char *appName, const char *titleId, int event);

/* appmgr_app_event.c */
struct AppState;
int _appListPushEvent(struct AppState *app, const SceAppMgrSystemEvent *event);
int sceAppMgrReceiveSystemEvent(SceAppMgrSystemEvent *event);
int sceAppMgrSendEventInternal(const SceAppMgrSystemEvent *event, SceUID pid);
int sceAppMgrSendSystemEvent(const char *appName, int event);
