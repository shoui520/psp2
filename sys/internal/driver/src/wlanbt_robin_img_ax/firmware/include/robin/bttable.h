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
	uint8_t reserved4[16];
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
extern uint8_t btTableLinkModes[8];				/* 0x55338: each link's mode, 0 to 3 */

extern const uint16_t romStateActiveTable[];			/* ROM 0x03f419e0: the slots of each packet type */
extern uint8_t btLinkPacketType;				/* 0x400f710 */
extern uint8_t btSlotQueueIndex[];				/* 0x400f99c: the queue of each state */
extern struct BtSlotQueue btSlotQueues[];			/* 0x400dcdc */

void btLmpLcHandleTable(int link, uint16_t length);		/* 0x4414e */
struct BtSourceState *btLmpLcProcessTable(int link);		/* 0x4415c */
void btLmpLcUpdateTable(int link, const uint8_t *packet);	/* 0x44192 */
void btLmpLcConfigureTable(uint16_t *ticks, int next, int limit);	/* 0x441da */
int btLmpLcCheckTable(unsigned int mask, int index);		/* 0x44214 */
void btLmpLcValidateTable(struct BtDeviceProfile *profile, struct BtDeviceProfile **link, uint32_t clock,
			  uint8_t state);			/* 0x44224 */
void btLmpLcPrepareTable(struct BtDeviceProfile *profile, uint32_t clock, uint8_t state);	/* 0x4424c */
int btLmpLcDispatchTable(int link);				/* 0x4426a */

#endif /* ROBIN_BTTABLE_H */
