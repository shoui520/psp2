/* Application launch parameters queued for delivery. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include "appmgr_shell_event.h"

typedef struct AppmgrExtraAppParam {
	unsigned char arguments[6144];
	SceSize argumentSize;
	char appName[32];
} AppmgrExtraAppParam;

_Static_assert(sizeof(AppmgrExtraAppParam) == 6180 &&
	       offsetof(AppmgrExtraAppParam, argumentSize) == 6144 &&
	       offsetof(AppmgrExtraAppParam, appName) == 6148,
	       "extra application parameters");

extern AppmgrExtraAppParam g_appmgrExtraAppParam;

typedef struct AppmgrParamQueueEntry {
	char param[1024];
	SceUID callbackId;
	SceUID sourcePid;
	char sourceAppName[32];
} AppmgrParamQueueEntry;

typedef struct AppmgrParamQueue {
	int count;
	int writeIndex;
	AppmgrParamQueueEntry entries[4];
} AppmgrParamQueue;

_Static_assert(sizeof(AppmgrParamQueueEntry) == 1064, "launch parameter entry");
_Static_assert(offsetof(AppmgrParamQueueEntry, callbackId) == 1024 &&
	       offsetof(AppmgrParamQueueEntry, sourcePid) == 1028 &&
	       offsetof(AppmgrParamQueueEntry, sourceAppName) == 1032,
	       "launch parameter metadata");
_Static_assert(sizeof(AppmgrParamQueue) == 4264, "launch parameter queue");

/* sceAppMgrGetAppParam2's output: the parameter and the application that sent it. */
typedef struct AppmgrAppParam {
	char param[1024];
	char sourceAppName[32];
	SceUInt32 reserved[8];
} AppmgrAppParam;

_Static_assert(sizeof(AppmgrAppParam) == 1088, "application parameter");

void _paramQueueCompact(AppmgrParamQueue *queue);

typedef struct AppmgrNotificationQueue {
	unsigned char *buffer;
	SceSize entrySize;
	SceUInt32 count;
	SceUInt32 writeIndex;
	SceUInt32 capacity;
	SceUID readSemaphore;
	SceUID writeSemaphore;
	SceUID mutex;
} AppmgrNotificationQueue;

_Static_assert(sizeof(AppmgrNotificationQueue) == 32, "notification queue");

typedef struct AppmgrNotificationRequest {
	SceUInt32 type;
	unsigned char payload[60];
} AppmgrNotificationRequest;

_Static_assert(sizeof(AppmgrNotificationRequest) == 64, "notification request");

int _appmgrQueuePush(const AppmgrNotificationRequest *request, int wait);
int _appmgrQueuePop(AppmgrNotificationRequest *request);
int appmgrParamQueueInit(AppmgrParamQueue *queue);
int appmgrParamQueuePush(AppmgrParamQueue *queue, const char *param,
		SceUID callbackId, SceUID sourcePid, const char *sourceAppName);

/* appmgr_event.c */
int sceAppMgrPeekLaunchParamForShell(int appId, char *param);
int sceAppMgrOverwriteLaunchParamForShell(int appId, const char *param);
int _paramQueueCancelAll(AppmgrParamQueue *queue);
int _sceAppMgrGetParam(char *param);
int sceAppMgrGetAppParam2(AppmgrAppParam *appParam);
int sceAppMgrGetExtraAppParam(AppmgrExtraAppParam *extraParam);
const char *appmgrParamQueuePeek(const AppmgrParamQueue *queue);
int _sendParam(const char *name, const AppmgrParamQueueEntry *entry,
	       unsigned int *flags, int excludedAppId);
int _appmgrQueueInit(void);

/* appmgr_event_syscalls.c */
int appmgrGetExtraAppParamSyscall(AppmgrExtraAppParam *extraParam);
int appmgrReceiveEventNumSyscall(int *eventNum);
int appmgrReceiveShellEventSyscall(AppmgrShellEvent *event);
int appmgrReceiveEventSyscall(void *appEvent);
int appmgrSendSystemEventSyscall(const char *appName, int event);
int appmgrSendSystemEvent2Syscall(const char *appName, const char *titleId, int event);
