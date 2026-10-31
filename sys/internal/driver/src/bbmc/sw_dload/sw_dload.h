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
int swdlTerminate(void);
/* name unknown: ask the modem to switch into its download mode (event 0x800000) */
int swdlRequestDownloadMode(void);
/* name unknown: wait for the download device to attach, 35 s when extended, else 5 s */
int swdlWaitAttach(int extended);
/* name unknown: wait 5 s for the download device to detach */
int swdlWaitDetach(void);
