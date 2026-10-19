/* Application events delivered to a process. */
#pragma once

#include <stddef.h>
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

int sceAppMgrReceiveEventNum(int *eventNum);
int sceAppMgrReceiveEventInternal(void *appEvent);
int sceAppMgrSendSystemEvent2(const char *appName, const char *titleId, int event);

/* appmgr_app_event.c */
struct AppState;
int _appListPushEvent(struct AppState *app, const SceAppMgrSystemEvent *event);
int sceAppMgrReceiveSystemEvent(SceAppMgrSystemEvent *event);
int sceAppMgrSendEventInternal(const SceAppMgrSystemEvent *event, SceUID pid);
int sceAppMgrSendSystemEvent(const char *appName, int event);
