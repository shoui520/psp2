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
uint8_t btTableRetries[8] __attribute__((section(".itcm.rw")));			/* 0x55330 */
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
		if (node->queued > profile->queued)
			break;
		link = &node->next;
	}
	btLtsRomResetState(profile, link);
}

__attribute__((noinline)) void btLmpLcPrepareTable(struct BtDeviceProfile *profile, uint32_t clock, uint8_t state)
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

/*
 * The slot of a link that was due ended: mode 5 or 9 by which windows it has, at the ticks of its
 * offset (or after its interval, when it holds). A link outside its mode's mask gets its window and
 * rate from the sample rates and is moved to the end of its window.
 */
void btLmpLcParseTable(struct BtDeviceProfile *profile, uint32_t clock, unsigned int kind)
{
	unsigned int interval = profile->interval;
	uint32_t start = profile->due;
	unsigned int wait;

	if (btLmpLcDispatchTable(profile->index) != 0) {
		unsigned int offset;
		unsigned int mode;
		uint32_t at;

		if (kind == 5) {
			if (profile->longWindow != 0) {
				profile->window = profile->longWindow;
				offset = 0;
				mode = 9;
				if (btTableModeEntry == 0xff) {
					btTableModeSlot = profile->longWindow + clock;
					btTableModeEntry = btSlotQueueIndex[9] - 1;
				}
			} else {
				offset = profile->offset;
				profile->window = profile->shortWindow;
				mode = 5;
			}
		} else {
			if (profile->shortWindow != 0 || profile->hold != 0) {
				mode = 5;
				profile->window = profile->shortWindow;
			} else {
				mode = 9;
				profile->window = profile->longWindow;
			}
			offset = profile->offset;
		}
		at = clock + offset;
		if (mode == 5 && profile->hold != 0) {
			at = profile->due + profile->interval;
			if ((int32_t)(clock - at) > 0)
				at = clock;
		}
		btLmpLcPrepareTable(profile, at, mode);
		return;
	}
	wait = profile->window;
	if (wait >= profile->span)
		wait = profile->span;
	if (profile->active == 0 && btLcoRomAcquireContext(profile->index) != 0)
		profile->since = clock;
	if ((int32_t)(profile->since + btTargetNode - clock) >= 0) {
		unsigned int setting = profile->secondSetting;

		interval = 0;
		if (setting <= profile->firstSetting)
			setting = profile->firstSetting;
		profile->rate = (setting >> 1) + btTargetStatus;
	}
	if (btLmpLcCheckTable(btTableLinkMask, profile->index) == 0 && profile->active == 0 && btTableModeMask != 0)
		profile->rate = 1;
	btLtsRomApplyState(profile, start + wait + interval - 1, 9);
}

/*
 * Whether the link has missed its slots: its packet type's length is over the target, it is the
 * queue's head and late, or the ROM counts more retries than the target allows.
 */
int btLmpLcBuildTable(struct BtDeviceProfile *profile, uint32_t clock)
{
	int missed = 0;
	struct BtDeviceProfile *head = btSlotQueues[btSlotQueueIndex[1]].head;
	unsigned int index = profile->index;
	const uint8_t *slot = romHctHostSlot[index];

	if (*(const BtHalf *)(btPacketTypes + 12 * slot[2] + 4) > btTargetConfig &&
	    clock - profile->queued > btTargetConfig)
		missed = 1;
	if (head != NULL && head->peer == index && (int32_t)(clock - profile->due) >= (int)btTargetConfig)
		missed = 1;
	if (slot[4] != 0 && btLcoRomManageContext(index) != 0) {
		unsigned int retries = (uint8_t)(btTableRetries[profile->index] + 1);

		btTableRetries[profile->index] = retries;
		if (retries > btTargetSlot)
			missed = 1;
	}
	if (head != NULL && head->interval == 1)
		missed = 0;
	return missed;
}

/* The link's slot is due at clock in the state (kind): it is scheduled again. */
void btLmpLcComputeTable(struct BtDeviceProfile *profile, uint32_t clock, unsigned int kind)
{
	uint32_t start = profile->due;
	int quiet = 0;
	int missed;
	unsigned int adjust;

	btLcoRomResetContext(profile->index);
	missed = btLmpLcBuildTable(profile, clock);
	if (btLmpLcDispatchTable(profile->index) != 0 && btLmpLcInspectEntry(btSource.packet, profile->index) == 0)
		quiet = 1;
	adjust = profile->adjust;
	if ((missed | quiet | adjust) == 0)
		return;
	if ((int32_t)(clock - (start + profile->window)) < 0) {
		uint32_t due;

		if (btTableLinkModes[profile->index] != 1 || adjust == 0)
			return;
		due = profile->due;
		btLtsRomQueryState(profile);
		profile->adjust = 0;
		if (profile->state == 5) {
			btLtsRomApplyState(profile, profile->due, kind);
		} else {
			btLtsRomApplyState(profile, clock, kind);
			profile->due = due;
		}
		return;
	}
	btLtsRomQueryState(profile);
	if (missed != 0 && btLmpLcDispatchTable(profile->index) == 0) {
		unsigned int setting;

		profile->adjust = 0;
		setting = profile->secondSetting > profile->firstSetting ? profile->secondSetting : profile->firstSetting;
		profile->rate = setting > 1 ? setting >> 1 : 1;
		if (profile->active == 0 && btSource.frame == 0 && btSource.header == 0)
			clock += profile->interval;
		btLtsRomApplyState(profile, clock, 6);
		return;
	}
	if (btLmpLcInspectEntry(btSource.packet, profile->index) != 0) {
		btLmpLcFilterEntry(profile, clock);
		return;
	}
	profile->adjust = 0;
	btLmpLcParseTable(profile, clock, kind);
}

/* The slot of the event's profile is due: it and every other link in the same state in the mode mask. */
void btLmpLcQueryTable(const struct BtSlotEvent *event, uint32_t clock)
{
	struct BtDeviceProfile *owner = event->profile;
	unsigned int link;

	btLmpLcComputeTable(owner, clock, event->state);
	for (link = 1; link < 8; link = (uint16_t)(link + 1)) {
		struct BtDeviceProfile *profile = &btDeviceProfile[link];

		if (profile->state == event->state && owner != profile &&
		    btLmpLcInspectEntry(btTableModeMask, profile->index) != 0)
			btLmpLcComputeTable(profile, clock, event->state);
	}
}
