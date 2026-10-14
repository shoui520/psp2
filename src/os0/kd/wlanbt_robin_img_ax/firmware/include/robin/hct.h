/* Host controller transport of the Bluetooth controller (bt/hct.c). */
#ifndef ROBIN_HCT_H
#define ROBIN_HCT_H

#include <stdint.h>
#include "robin/buffer.h"

struct HctLinkMessage;
struct HctReleaseMessage;

extern uint8_t hctCoexTickCount;
extern uint8_t hctCoexDirty;
extern uint8_t hctHostActive;
extern uint8_t hctModeCurrent;
extern uint8_t hctModeRequested;
extern uint8_t hctSleepRequested;
extern uint32_t hctDeviceContext;
extern void *btSdioDev;
extern void *btHciDev;
extern void *btSdioActiveHandle;
extern uint32_t hctChannelMask;
extern OsaBuffer *hctPending;

void hctComputeCoexIntervals(void);
void hctCoexTick(void);
uint32_t *hctEnableCoexScheduling(void);
void hctFlushPending(void);
void hctQueuePending(OsaBuffer *buffer);
void hctDeliverToHost(uint8_t *payload, uint32_t event);
void hctCompleteTransfer(uint32_t handle, uint32_t flag);
int hctRxCallback(OsaBuffer *buffer);
void hctDropPending(uint32_t key, const void *address);
void hctReleaseLinkContext(struct HctReleaseMessage *message);
void hctSendAclToHost(struct HctLinkMessage *message);
int hctRouteMessage(struct HctLinkMessage *message);
int hctPostPacket(uint8_t *packet, uint32_t flag);
int hctEventCallback(void *device, OsaBuffer *buffer, uintptr_t argument,
		     unsigned int event);
void hctReceiveEvent(struct HctLinkMessage *message);
int hctSendCommand(struct HctLinkMessage *message);
void hctSendAclData(struct HctLinkMessage *message);
int hctCreditReturned(struct HctLinkMessage *message);
int hctTransferDone(struct HctLinkMessage *message);
void hctHandleLinkData(struct HctLinkMessage *message);
int hctCommandDone(struct HctLinkMessage *message);
int hctInitControllerDescriptor(void);
int hctSetEnabled(int enable);
int hctSetCoreMode(int mode);
int hctApplyMode(int resume);
void hctOpenSdio(void);
int hctSendAfhMap(void);
int hctEnableHost(void);
int hctSdioIdle(void);
int hctLinkActive(unsigned int handle);
int hctRegisterDriver(void);
int hctTransportReady(int transport);
void hctTask(void);

#endif /* ROBIN_HCT_H */
