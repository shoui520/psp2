/*
 * Interfaces to the Bluetooth controller in the Mask ROM, as seen from the
 * RAM code: message services, controller entry points, the controller's
 * link tables and its callbacks. The ROM is not part of the image, so each
 * prototype and field is inferred from the callers; the names are provisional.
 * Addresses are in boot/rom_syms.ld.
 */
#ifndef ROBIN_BTROM_H
#define ROBIN_BTROM_H

#include <stdint.h>
#include "robin/buffer.h"

/* Messages are allocated by the ROM, begin with a 32-bit message identifier, and are
 * passed by the address of the pointer, which the ROM clears. */
void *osa_MsgAlloc(uint32_t size);
int osa_MsgSend(void **message, uint32_t queue);
int osa_MsgFree(void **message);
OsaBuffer *osa_BufferAlloc(struct OsaBufferPool *pool, uint32_t size, uint32_t flags);
void *osa_MsgQueueRecv(void *queue);
/* Posts an event record to the queue of an object created by osa_QueueCreate(). */
int mdevRomQueryState(void *object, uint32_t event, const void *record);
int mdevRomProcessState(void *device);
int osa_MsgQueueCount(void *queue);
void osa_BlockPoolCreate(void *pool, void *name, void *start, void *size);
int mdevRegisterDriver(void *driver, void *reserved);
/* Next message of the calling task. */
uint32_t *mdevReceiveMessage(void);

/* Link tables of the controller: 120 bytes per link, index 1..7. */
uint8_t *btHccRomApplyState(void);
uint8_t *btLcoRomVerifyState(uint32_t link);

void btBdhRomSyncState(uint32_t timer, uint32_t mode);
void *btBdhRomResolveState(uint32_t handle, uint32_t flag);
void btBdhRomProcessContext(void);
void btBdhRomScheduleState(void);
void btBdhRomUpdateContext(void *data, uint32_t length);
void btBdhRomHandleContext(void *message);
void btBdhRomRecordState(void *message);
void btBdhRomNotifyState(uint32_t handle, uint32_t queue);
void btBdhRomFilterState(void);
int btBdhRomTrackState(void *message);
int btBdhRomApplyState(uint32_t id, void *value);
void btBdhRomFormatState(void);
void btBdhRomSelectState(void);
void btBdhRomAcquireState(void);
void btBdhRomVerifyState(void);
void btBdhRomClearState(uint32_t handle, uint32_t flag);
void btBdhRomSetupState(uint32_t handle, uint32_t flag);
void btBdhRomConvertState(uint32_t handle);
void btBdhRomAdjustState(void);
void btBdhRomInspectState(uint32_t flag);
void btBdhRomManageState(void);
void btLscRomApplyState(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void btLscRomNotifyState(uint32_t a, uint32_t b, uint32_t c);
int btLscRomResolveState(uint32_t a, uint32_t b);

/* Message queues (read from the ROM). */
extern const uint32_t romQueueHct;
extern const uint32_t romQueueBtCore;

/* Host-controller state kept in the ROM's RAM. */
struct RomHctLinkQueue {
	uint8_t flags;
	int8_t credits;
	uint8_t pending;
	uint8_t reserved;
	uint8_t counter;
};
struct RomHctLinkInfo {
	uint8_t flags;
	uint8_t reserved;
	uint16_t aclCount;
	uint8_t active;
	uint8_t outstanding;
	uint16_t handle;
};
_Static_assert(sizeof(struct RomHctLinkQueue) == 5, "link queue entry");
_Static_assert(sizeof(struct RomHctLinkInfo) == 8, "link info entry");

extern struct RomHctLinkInfo romHctLinkInfo[];
extern void *romHctPendingCommand[];
extern struct RomHctLinkQueue romHctLinkQueue[];
extern uint8_t romHctHostSlot[][24];
extern void (*romHctReleaseBuffer)(void *);
extern void (*romHctAcquireSession)(void *);
extern void *(*romHctSourceCounter)(uint32_t);
extern uint8_t romHctMode;
extern uint8_t romHctFlowEnabled;
extern uint8_t romHctDeferredCount;
extern uint8_t romHctFrameFlag;
extern uint8_t romHctHeaderSize;
extern uint8_t romHctDescriptor;
extern uint8_t romHctQueueMode;
extern uint16_t romHctTimer;
extern uint16_t romHctChannel;
extern uint8_t romHctParam;
extern uint8_t romHctControl[];
extern uint16_t romHctWindow;
extern int (*romHctFilterAcl)(OsaBuffer *);
extern int (*romHctFilterEvent)(OsaBuffer *);
extern int (*romHctRxHook)(OsaBuffer *);
extern uint8_t romTargetTable;

#endif /* ROBIN_BTROM_H */
