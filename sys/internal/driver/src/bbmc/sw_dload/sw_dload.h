/* The modem software download module (SceBbmcSwdlMod): its thread, heap and client. */
#pragma once
#include <scetypes.h>
#include "../bbmc_message.h"

/* name unknown: the client the module reports its notification bits to */
typedef struct SwdlClient {
	int (*notify)(SceUInt32 mask, const BbmcNotificationArgs *args);
	void *argument;
} SwdlClient;

int swdlInitialize(SceUInt32 version, const SwdlClient *client);
