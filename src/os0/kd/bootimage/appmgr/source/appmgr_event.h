/* Application launch parameters queued for delivery. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

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
