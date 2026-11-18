/* Host controller transport of the Bluetooth controller (bt/hct.c, bt/hctclock.c). */
#ifndef ROBIN_HCT_H
#define ROBIN_HCT_H

#include <stdint.h>
#include "robin/buffer.h"

#define HCT_LINK_COUNT		11
#define HCT_ACL_LINKS		7

struct HctLinkMessage;
struct HctReleaseMessage;

extern uint8_t hctCoexTickCount;
extern uint8_t hctCoexDirty;
extern uint8_t hctHostActive;
extern uint8_t hctModeCurrent;
extern uint8_t hctModeRequested;
extern uint8_t hctSleepRequested;
extern uint8_t hctDeviceContext;	/* read and written a byte wide */
extern void *btSdioDev;
extern void *btHciDev;
extern void *btSdioActiveHandle;
extern uint32_t hctChannelMask;
extern OsaBuffer *hctPending;

void hctComputeCoexIntervals(void);
void hctCoexTick(void);
int hctClockCheckRequirements(void);
int hctClockApply(void);
int hctClockRequirementSet(int selector, uint32_t value);
uint32_t *hctEnableCoexScheduling(void);
void hctFlushPending(void);
void hctQueuePending(OsaBuffer *buffer);
void hctDeliverToHost(uint8_t *payload, uint32_t event);
void hctCompleteTransfer(uint32_t handle, uint32_t flag);
int hctRxCallback(OsaBuffer *buffer);
void hctDropPending(uint32_t key, const void *address);
void hctDeliverAclToHost(uint8_t *payload);
void hctReleaseLinkContext(struct HctReleaseMessage *message);
void hctSendAclToHost(struct HctLinkMessage *message);
void hctRouteMessage(struct HctLinkMessage *message);
int hctPostPacket(uint8_t *packet, uint32_t flag);
int hctEventCallback(void *device, OsaBuffer *buffer, uintptr_t argument,
		     unsigned int event);
void hctReceiveEvent(struct HctLinkMessage *message);
void hctSendCommand(struct HctLinkMessage *message);
void hctSendAclData(struct HctLinkMessage *message);
int hctCreditReturned(struct HctLinkMessage *message);
void hctTransferDone(struct HctLinkMessage *message);
void hctHandleLinkData(struct HctLinkMessage *message);
int hctCommandDone(struct HctLinkMessage *message);
int hctInitControllerDescriptor(void);
int hctSetEnabled(int enable);
void hctSetCoreMode(int mode);
void hctApplyMode(int resume);
void hctOpenSdio(void);
void hctSendAfhMap(void);
void hctEnableHost(void);
int hctSdioIdle(void);
int hctLinkActive(unsigned int handle);
extern uint32_t romCoexPrimaryFlag;
int hctRegisterDriver(void);
int hctTransportReady(int transport);
void hctTask(void);

extern uint32_t romHctCoexMode;			/* 0x400f448: the transport's baud rate */

#endif /* ROBIN_HCT_H */
