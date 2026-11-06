/*
 * A notice for the shell to show: what sceAppMgrSendNotificationRequest takes from an application
 * and SceAppMgr queues until the shell receives it.
 */
#pragma once

#include <scetypes.h>

typedef struct AppmgrNotificationRequest {
	SceUInt32 type;
	unsigned char payload[60];
} AppmgrNotificationRequest;

_Static_assert(sizeof(AppmgrNotificationRequest) == 64, "notification request");
