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
/* Pool counters: the blocks of a pool, and by query 8 and 16 the free ones of each kind. */
int osa_BufferClone(struct OsaBufferPool *pool);
int osa_PoolQuery(struct OsaBufferPool *pool, int query);
/* The blocks of the pool, as a transmit quota counts them. */
int osa_PoolCapacity(struct OsaBufferPool *pool);
/* A message list of the Mask ROM's OSA, for osa_MsgQueueSend() and osa_MsgQueueRecv(). */
struct OsaMsgList {
	uint32_t words[5];
};

void *osa_MsgQueueRecv(void *queue);
int osa_MsgQueueSend(void *queue, void *message);
/* Like osa_MsgQueueSend(), but the message becomes the first of the list; the last argument is 20 in every use. */
int osa_MsgQueueSendFront(void *queue, void *message, uint32_t reserved, uint32_t size);
/* Posts an event record to the queue of an object created by osa_QueueCreate(). */
int mdevRomQueryState(void *object, uint32_t event, const void *record);

/*
 * The Mask ROM's hierarchical state machines. A machine starts with its
 * current state; its states (12 bytes each, kept by the ROM) follow. A
 * handler is given the machine and the event (its number first) and returns
 * NULL when it handled the event, the event for the parent state otherwise.
 * mdevRomQueryState() dispatches an event; a state is sent ROM_HSM_EXIT when
 * it is left and ROM_HSM_ENTRY when it is entered.
 */
#define ROM_HSM_ENTRY	(-1)
#define ROM_HSM_EXIT	(-2)

struct RomHsmState {
	uint32_t rom[3];
};

typedef const int32_t *(*RomHsmHandler)(void *machine, const int32_t *event);

void romHsmStateInit(struct RomHsmState *state, struct RomHsmState *parent, RomHsmHandler handler);
void romHsmStart(void *machine, struct RomHsmState *initial);
void romHsmTransition(void *machine, struct RomHsmState *target);
int mdevRomProcessState(void *device);
void *osa_MsgQueuePeek(void *queue);		/* the first message, left in the list */
struct OsaPoolEntry;
/* A pool of fixed blocks: its descriptor, its configuration (struct OsaPoolConfig), its control block and its memory. */
int osa_BlockPoolCreate(void *pool, void *config, void *control, void *memory);
int osa_BlockPoolDelete(void *pool, int (*release)(struct OsaPoolEntry *entry));
int mdevRegisterDriver(void *driver, void *reserved);
/* Next message of the calling task. */
uint32_t *mdevReceiveMessage(void);

/* The controller's temperature-controlled transmit level (bt/lcpower.c); roles are inferred. */
void btBdhRomConfigureState(uint32_t value, uint32_t flag);
uint8_t *btLcoRomComputeContext(uint32_t link);
void btBdhRomBuildState(uint32_t a, uint32_t b);
void btLscRomScheduleState(uint32_t mode, uint32_t value);
void btBdhRomCheckState(uint8_t *context);
void btBdhRomValidateState(uint32_t a, uint32_t b, void *buffer);
void btBdhRomParseState(void);
void btLdaRomNotifyState(void);
void btBdhRomDispatchState(uint8_t *command);
uint8_t btBdhRomProcessState(uint32_t reg, uint32_t flag);
void btBdhRomUpdateState(uint32_t reg, uint32_t value);

int btHccRomPrepareState(void);
int mdevRomSyncState(uint32_t a1, void *a2, uint32_t a3);
int btBdhRomHandleState(void *a1, uint32_t a2, void *a3, const char *name);
void *btLtsRomPrepareContext(void *node, uint32_t a2);
uint32_t btLdrRomUpdateState(uint32_t a1);
uint8_t *btLcoRomResetContext(uint32_t link);
int btLdaRomPrepareState(uint32_t a1);

uint8_t *btLcoRomProcessState(uint32_t index);
uint8_t *btLdrRomReleaseState(void);

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
