/* Application events delivered to a process. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

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
