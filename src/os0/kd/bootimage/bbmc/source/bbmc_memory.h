#pragma once

#include <psp2kern/types.h>
#include <sce/syscon.h>

typedef struct BbmcControl BbmcControl;

BbmcControl *bbmcGetControl(void);
int bbmcNotifyFeatureEvent(BbmcControl *control, SceUInt32 featureId,
	SceUInt32 eventMask);
int bbmcCollectIndexedResponseEvents(BbmcControl *control);
void bbmcScheduleDeferredNetworkRetry(BbmcControl *control);
int bbmcPrepareUsbShutdown(BbmcControl *control);
void bbmcRecordTransitionStatus(BbmcControl *control, int status,
	unsigned int step);
int bbmcSendSysconPowerCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	unsigned int value);
int bbmcCreateHeap(SceSize size);
int bbmcDeleteHeap(void);
void *bbmcAlloc(SceSize size, SceSize alignment);
int bbmcFree(void *ptr);
