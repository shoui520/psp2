/*
 * The link manager's packet tables (bt/lctable.c, 0x4414e on): which links run in which mode, the
 * packet type and length of the last packet, and the slot queues of the ROM's scheduler. The names of
 * the firmware's placeholders are kept (btLmpLc...Table, btTable...); the roles are inferred from what
 * the code does.
 */
#ifndef ROBIN_BTTABLE_H
#define ROBIN_BTTABLE_H

#include <stdint.h>
#include "robin/btsource.h"

/* One of the ROM's slot queues (20 bytes, 0x400dcdc): profiles in order of their due clock. */
struct BtSlotQueue {
	struct BtDeviceProfile *head;
	uint8_t reserved4[8];
	/* gives the first profile its slots from clock, at most limit ticks; returns the ticks used */
	unsigned int (*take)(struct BtSlotQueue *queue, struct BtDeviceProfile *profile, uint32_t clock,
			     unsigned int limit);
	uint8_t reserved16[4];
};
_Static_assert(sizeof(struct BtSlotQueue) == 20, "slot queue");

extern uint8_t btTableEnabled;					/* 0x55294: 0 turns the mode masks off */
extern uint8_t btTableLinkMask;					/* 0x55295: bit n: link n runs in mode 0 */
extern uint8_t btTableProfile;					/* 0x55296 */
extern uint8_t btTableEvent;					/* 0x55297 */
extern uint8_t btTableCommand;					/* 0x55298 */
extern uint8_t btTablePayload;					/* 0x55299 */
extern uint8_t btTableRegister;					/* 0x5529a */
extern uint8_t btTableRecord;					/* 0x5529b */
extern uint8_t btTableModeState;				/* 0x5529c */
extern uint8_t btTableModeContext;				/* 0x5529d */
extern uint8_t btTableModeEntry;				/* 0x5529e */
extern uint8_t btTableModeTable;				/* 0x5529f */
extern uint8_t btTableModeBuffer;				/* 0x552a0 */
extern uint8_t btTableModePacket;				/* 0x552a1 */
extern uint8_t btTableModeMask;					/* 0x552a2: bit n: link n runs in mode 1 or 3 */
extern uint8_t btTableModeHeader;				/* 0x552a3 */
extern uint16_t btTableModeDescriptor;				/* 0x552a4 */
extern uint16_t btTableModeQueue;				/* 0x552a6 */
extern uint16_t btTablePacketType;				/* 0x552a8: index into romStateActiveTable */
extern uint16_t btTablePacketLength;				/* 0x552aa */
extern uint8_t btTableOffsets[8];			/* 0x55308 */
extern uint8_t btTableShortWindows[8];			/* 0x55310 */
extern uint8_t btTableLongWindows[8];			/* 0x55318 */
extern uint8_t btTableExtras[8];				/* 0x55320 */
extern uint8_t btTableHoldWindows[8];			/* 0x55328 */
extern void (*btTableRadioWrite)(uint16_t reg, uint32_t value);	/* 0x552b4 */
extern const uint8_t btPeerBusy[];				/* 0x400b393: non-zero: the handle is busy */
extern const uint8_t *btTablePeerBusy;				/* 0x552b8 */
extern uint8_t btTableRetries[8] __attribute__((aligned(4)));	/* 0x55330: each link's missed slots */
extern uint8_t btTableLinkModes[8] __attribute__((aligned(4)));	/* 0x55338: each link's mode, 0 to 3 */
extern uint32_t btTableShortStates;				/* 0x552c4 */
extern uint32_t btTableModeSlot;				/* 0x552c8: the clock of the next mode 9 slot */

/* The ROM's event of a due slot. */
struct BtSlotEvent {
	struct BtDeviceProfile *profile;
	uint32_t reserved4;
	uint8_t state;
};

extern const uint16_t romStateActiveTable[];			/* ROM 0x03f419e0: the slots of each packet type */
extern uint8_t btLinkPacketType;				/* 0x400f710 */
extern uint8_t btSlotQueueIndex[];				/* 0x400f99c: the queue of each state */
extern struct BtSlotQueue btSlotQueues[];			/* 0x400dcdc */

/*
 * The ROM's hooks of a slot state: enter as a profile joins its queue (its arguments depend on the
 * state), due when the queue's first slot is due.
 */
struct BtStateHooks {
	void (*enter)(void);
	void (*due)(struct BtSlotEvent *event, uint32_t clock);
};
extern struct BtStateHooks btStateHooks[12];			/* 0x400f9ac */
extern uint8_t *btPacketTypes;					/* 0x400f968: 12 bytes per type, halfword 4 its length, halfword 6 its limit (bits 0-14) */
extern uint8_t btTargetStatus;					/* 0x400de53 */
extern uint16_t btTargetNode;					/* 0x400de56 */
extern uint8_t btDeviceWindow;					/* 0x400cd28 */
extern uint8_t btDeviceBitmap;					/* 0x400cd2c */
/* The ROM's handler of a due slot, which btLmpLcResolveTable() wraps. */
extern int (*btNextPayload)(const struct BtSlotEvent *event, struct BtDeviceProfile *profile, uint32_t clock,
			    uint32_t argument, unsigned int burst, unsigned int limit);	/* 0x400ed84 */
extern uint32_t btTargetState;					/* 0x400ddf0: the ROM's target link */
extern uint8_t btTargetContext;					/* 0x400ddf4: the target's device */
extern uint8_t btTargetEntry;					/* 0x400ddf5 */
extern uint8_t btTargetUsed;					/* 0x400ddf7: the target's slots taken */
extern uint8_t btTargetSlots;					/* 0x400ddf8: the slots it has */
extern uint8_t (*btTableHostLinks)[40];			/* 0x552fc */
/* The ROM's host link records, 40 bytes each: byte 1 is the link's rate class. */
extern uint8_t btHostLinks[][40];				/* DTCM 0x400c25c */
extern uint32_t btTableModeClock;				/* 0x55300 */
extern uint32_t btTableTargetClock;				/* 0x55304 */
extern uint8_t btLinkDefaultSpan;				/* 0x400f818 */
extern uint8_t btLinkScanFlags;					/* 0x400f72d: bit 1: the radio registers follow */
extern uint8_t btLinkScanState;					/* 0x400f72e */
extern uint8_t *btLinkScanSlot;					/* 0x400f908 */
extern uint16_t btLinkHostIndex;				/* 0x400f742: the host slot of the link */
extern uint8_t btLinkPayloadCount;				/* 0x400f7fa */
extern uint8_t btHostFlags;					/* 0x400bd3f: bit 0: sessions take messages */
extern uint8_t btHostSession;					/* 0x400bd50 */
extern uint16_t btHostBitmap;					/* 0x400c4e8 */
extern uint8_t btTargetSlot;					/* 0x400de58: the retries a link is allowed */

void btLmpLcHandleTable(int link, uint16_t length);		/* 0x4414e */
struct BtSourceState *btLmpLcProcessTable(int link);		/* 0x4415c */
void btLmpLcUpdateTable(int link, const uint8_t *packet);	/* 0x44192 */
void btLmpLcConfigureTable(uint16_t *ticks, int next, int limit);	/* 0x441da */
int btLmpLcCheckTable(unsigned int mask, int index);		/* 0x44214 */
void btLmpLcValidateTable(struct BtDeviceProfile *profile, struct BtDeviceProfile **link, uint32_t clock,
			  uint8_t state);			/* 0x44224 */
void btLmpLcPrepareTable(struct BtDeviceProfile *profile, uint32_t clock, uint8_t state);	/* 0x4424c */
int btLmpLcDispatchTable(int link);				/* 0x4426a */
void btLmpLcParseTable(struct BtDeviceProfile *profile, uint32_t clock, unsigned int kind);	/* 0x44292 */
int btLmpLcBuildTable(struct BtDeviceProfile *profile, uint32_t clock);	/* 0x4438e */
void btLmpLcComputeTable(struct BtDeviceProfile *profile, uint32_t clock, unsigned int kind);	/* 0x44426 */
void btLmpLcQueryTable(const struct BtSlotEvent *event, uint32_t clock);	/* 0x44524 */
void btLmpLcResetTable(void);					/* 0x4459c */
void btLmpLcSyncTable(int set, int link);			/* 0x4459e */
void btLmpLcApplyTable(struct BtDeviceProfile *profile, uint8_t shortWindow, uint8_t offset, uint8_t longWindow,
		       uint8_t extra);				/* 0x445b6 */
int btLmpLcResolveTable(const struct BtSlotEvent *event, struct BtDeviceProfile *profile, uint32_t clock,
			uint32_t argument);			/* 0x44616 */
int btLmpLcScheduleTable(const struct BtSlotEvent *event, struct BtDeviceProfile *profile, uint32_t clock,
			 uint32_t argument);			/* 0x447ae */
void btLmpLcNotifyTable(const struct BtSlotEvent *event, uint32_t clock);	/* 0x447b0 */
void btLmpLcFormatTable(int link, uint32_t a1, uint32_t a2);	/* 0x447b2 */
int btLmpLcInspectTable(int link);				/* 0x44802 */
void btLmpLcTrackTable(uint8_t *command, uint8_t *result, uint32_t argument);	/* 0x44814 */
void btLmpLcManageTable(uint8_t *value, uint8_t *previous);	/* 0x44856 */
int btLmpLcAcquireTable(struct BtDeviceProfile *profile, const struct BtDeviceProfile *second, uint8_t *other,
			uint8_t *entry, uint8_t *setting, uint8_t *active);	/* 0x44864 */
int btLmpLcReleaseTable(void);					/* 0x44956 */
void btLmpLcSetupTable(void);					/* 0x44994 */
int btLmpLcClearTable(uint32_t event, struct BtDeviceProfile *profile, uint32_t clock, uint32_t argument,
		      uint32_t burst, uint32_t limit);		/* 0x44a8c */
void btLmpLcConvertTable(int link);				/* 0x44b34 */
void btLmpLcVerifyTable(uint32_t *request);			/* 0x44b44 */
int btLmpLcSelectTable(uint16_t *request);			/* 0x44b9e */
int btLmpLcAdjustTable(void);					/* 0x44bf2 */
void btLmpLcRecordTable(void);					/* 0x44c2a */
void btLmpLcFilterTable(void);					/* 0x44c6e */

/* Not converted yet. */
uint8_t btLmpLcAdjustHeader(int link);				/* 0x48f28: the ROM's rate setting of the second ring */
uint8_t btLmpLcRecordHeader(int link);				/* 0x48f3a: the same for the first ring */
unsigned int btLmpLcSelectHeader(int link);			/* 0x48edc: the link's rate class, 0 to 7 */
int btLmpLcFilterHeader(void);					/* 0x48f4c */
void btLmpLcValidateDescriptor(int link);			/* 0x4948e */

#endif /* ROBIN_BTTABLE_H */
