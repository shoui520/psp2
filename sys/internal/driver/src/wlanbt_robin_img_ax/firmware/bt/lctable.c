/*
 * The link manager's packet tables (0x4414e on): the packets each link sends and receives are
 * sampled for its rates (bt/lcsource.c), and the ROM's slot queues are kept in the order of the
 * profiles' due clocks. The file name and the roles are inferred; the function names are the
 * firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/bttable.h"

typedef uint16_t __attribute__((may_alias)) BtHalf;

uint8_t btTableEnabled __attribute__((section(".itcm.rw"))) = 1;		/* 0x55294 */
uint8_t btTableLinkMask __attribute__((section(".itcm.rw")));			/* 0x55295 */
uint8_t btTableProfile __attribute__((section(".itcm.rw")));			/* 0x55296 */
uint8_t btTableEvent __attribute__((section(".itcm.rw")));			/* 0x55297 */
uint8_t btTableCommand __attribute__((section(".itcm.rw")));			/* 0x55298 */
uint8_t btTablePayload __attribute__((section(".itcm.rw")));			/* 0x55299 */
uint8_t btTableRegister __attribute__((section(".itcm.rw"))) = 1;		/* 0x5529a */
uint8_t btTableRecord __attribute__((section(".itcm.rw"))) = 0x10;		/* 0x5529b */
uint8_t btTableModeState __attribute__((section(".itcm.rw"))) = 6;		/* 0x5529c */
uint8_t btTableModeContext __attribute__((section(".itcm.rw"))) = 0x0f;		/* 0x5529d */
uint8_t btTableModeEntry __attribute__((section(".itcm.rw"))) = 0xff;		/* 0x5529e */
uint8_t btTableModeTable __attribute__((section(".itcm.rw")));			/* 0x5529f */
uint8_t btTableModeBuffer __attribute__((section(".itcm.rw")));			/* 0x552a0 */
uint8_t btTableModePacket __attribute__((section(".itcm.rw")));			/* 0x552a1 */
uint8_t btTableModeMask __attribute__((section(".itcm.rw")));			/* 0x552a2 */
uint8_t btTableModeHeader __attribute__((section(".itcm.rw")));			/* 0x552a3 */
uint16_t btTableModeDescriptor __attribute__((section(".itcm.rw"))) = 4;	/* 0x552a4 */
uint16_t btTableModeQueue __attribute__((section(".itcm.rw")));			/* 0x552a6 */
uint16_t btTablePacketType __attribute__((section(".itcm.rw")));		/* 0x552a8 */
uint16_t btTablePacketLength __attribute__((section(".itcm.rw")));		/* 0x552aa */
uint8_t btTableLinkModes[8] __attribute__((section(".itcm.rw")));		/* 0x55338 */

/* A packet was sent on the link: its type (from the ROM) and length are kept for btLmpLcProcessTable(). */
void btLmpLcHandleTable(int link, uint16_t length)
{
	btTablePacketLength = length;
	btTablePacketType = btLcoRomScheduleContext(link);
}

/* The sent packet is sampled in the link's first ring; type 3 is not marked with the link's status bit. */
struct BtSourceState *btLmpLcProcessTable(int link)
{
	unsigned int status;

	btLcoRomNotifyContext(link);
	status = (*btLinkStatus >> link) & 1;
	if (btTablePacketType != 3)
		btTablePacketType |= status << 4;
	return btLmpLcClearEntry(link, 0, btTablePacketLength, romStateActiveTable[btTablePacketType]);
}

/*
 * A packet was received on the link: a type of fewer than 32 slots carries an 8-bit header, and a
 * data packet (types 1 and 2 in the low bits) is sampled with its length in the link's second ring.
 */
void btLmpLcUpdateTable(int link, const uint8_t *packet)
{
	unsigned int header;
	unsigned int kind;

	if (romStateActiveTable[btLinkPacketType] < 32)
		*(BtHalf *)(packet + 2) = (uint8_t)*(const BtHalf *)(packet + 2);
	header = *(const BtHalf *)(packet + 2);
	btLcoRomSetupContext(link);
	kind = header & 3;
	if (kind == 1 || kind == 2) {
		unsigned int slots = romStateActiveTable[btLinkPacketType];

		if (slots < 32)
			header = (uint8_t)header;
		btLmpLcConvertEntry(link, btLinkPacketType, (header >> 3) & 0x3ff, slots);
	}
}

/*
 * While a ring holds samples: the ticks left to the next slot, put in *ticks when they fit (next:
 * 0 when at least btSource.descriptor are left; otherwise the ticks missing, when no more than limit).
 */
void btLmpLcConfigureTable(uint16_t *ticks, int next, int limit)
{
	int left;

	if (btSource.frame == 0 && btSource.header == 0)
		return;
	left = (int)(btDeviceRecord - btLmpLcTrackEntry()) - 1;
	if (next != 0) {
		if ((int)btSource.descriptor > left)
			return;
		left = 0;
	} else {
		left = btSource.descriptor - left;
		if (left > limit)
			return;
	}
	*ticks = left;
}

__attribute__((noinline)) int btLmpLcCheckTable(unsigned int mask, int index)
{
	return ((1 << index) & mask) != 0;
}

/* The profile is due at clock in the state: it goes into the queue after the profiles due before it. */
__attribute__((noinline)) void btLmpLcValidateTable(struct BtDeviceProfile *profile, struct BtDeviceProfile **link,
						      uint32_t clock, uint8_t state)
{
	struct BtDeviceProfile *node;

	profile->due = clock;
	profile->state = state;
	while ((node = *link) != NULL) {
		if ((int32_t)(node->due - clock) > 0)
			break;
		if (node->priority > profile->priority)
			break;
		link = &node->next;
	}
	btLtsRomResetState(profile, link);
}

void btLmpLcPrepareTable(struct BtDeviceProfile *profile, uint32_t clock, uint8_t state)
{
	btLmpLcValidateTable(profile, &btSlotQueues[btSlotQueueIndex[state]].head, clock, state);
}

/* Whether the link runs in its mode's mask: mode 0 in btTableLinkMask, 1 and 3 in btTableModeMask. */
int btLmpLcDispatchTable(int link)
{
	if (btTableEnabled == 0)
		return 0;
	switch (btTableLinkModes[link]) {
	case 0:
		return btLmpLcCheckTable(btTableLinkMask, link);
	case 1:
	case 3:
		return btLmpLcCheckTable(btTableModeMask, link);
	default:
		return 0;
	}
}
