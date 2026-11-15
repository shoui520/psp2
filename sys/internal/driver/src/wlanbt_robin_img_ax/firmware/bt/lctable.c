/*
 * The link manager's packet tables (0x4414e on): the packets each link sends and receives are
 * sampled for its rates (bt/lcsource.c), and the ROM's slot queues are kept in the order of the
 * profiles' due clocks. The file name and the roles are inferred; the function names are the
 * firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/bttable.h"
#include "robin/eventpower.h"

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
struct BtSyncFrameCounts btSyncFrameCounts __attribute__((section(".itcm.rw")));	/* 0x552ac */
/* The hook the ROM writes radio registers through. */
void (*btTableRadioWrite)(uint16_t reg, uint32_t value) __attribute__((section(".itcm.rw"))) =
	btLdaRomCheckState;							/* 0x552b4 */
/* The ROM's per-handle busy bytes (btLmpLcInspectPacket()). */
const uint8_t *btTablePeerBusy __attribute__((section(".itcm.rw"))) = btPeerBusy;	/* 0x552b8 */
/* The event clock btLmpLcClearFrame() started the scan at; HCI Read Clock counts from it. */
uint32_t btTableClockStart __attribute__((section(".itcm.rw")));		/* 0x552bc */
/* The ROM's record of the last host request's slots. */
struct BtSlotRequest *btTableSlotRequest __attribute__((section(".itcm.rw"))) = &btSlotRequest;	/* 0x552c0 */
/* The link states (bit n: state n) whose slots may be cut short (btLmpLcManagePacket()). */
uint32_t btTableShortStates __attribute__((section(".itcm.rw"))) = 0x116;	/* 0x552c4 */
uint32_t btTableModeSlot __attribute__((section(".itcm.rw")));		/* 0x552c8 */
/* What btLmpLcQueryFrame() reaches the ROM's connection setup through. */
uint8_t *btTableSyncLimits __attribute__((section(".itcm.rw"))) = btHostSyncLimits;	/* 0x552cc */
uint8_t (*btTablePendingAddresses)[6] __attribute__((section(".itcm.rw"))) = btPendingAddresses;	/* 0x552d0 */
uint8_t *btTableHostSettings __attribute__((section(".itcm.rw"))) = btHostSettings;	/* 0x552d4 */
uint8_t **btTableHeldEvent __attribute__((section(".itcm.rw"))) = &btHeldConnectEvent;	/* 0x552d8 */
int (*btTableEventCheck)(const uint8_t *) __attribute__((section(".itcm.rw"))) = btHccRomProcessState;	/* 0x552dc */
void (*btTableConnectClear)(const uint8_t *) __attribute__((section(".itcm.rw"))) = btBdhRomSyncContext;	/* 0x552e0 */
int (*btTableConnectDone)(unsigned int, const uint8_t *) __attribute__((section(".itcm.rw"))) =
	btHccRomUpdateState;							/* 0x552e4 */
void (*btTableConnectReply)(const uint8_t *, unsigned int, unsigned int) __attribute__((section(".itcm.rw"))) =
	romLibRecordEntry;							/* 0x552e8 */
int (*btTableConnectCheck)(const uint8_t *, unsigned int, unsigned int, uint8_t *)
	__attribute__((section(".itcm.rw"))) = btBdhRomComputeContext;	/* 0x552ec */
void (*btTableConnectQuery)(const uint8_t *, uint32_t *, uint32_t *, uint8_t *)
	__attribute__((section(".itcm.rw"))) = btBdhRomQueryContext;		/* 0x552f0 */
void (*btTableInquiryFilterClear)(void) __attribute__((section(".itcm.rw"))) = romLibAdjustEntry;	/* 0x552f4 */
void (*btTableConnectFail)(const uint8_t *) __attribute__((section(".itcm.rw"))) = btBdhRomResetContext;	/* 0x552f8 */
/* The clock the mode buffer was set at (btLmpLcAcquirePacket()). */
uint32_t btTableModeClock __attribute__((section(".itcm.rw")));		/* 0x55300 */
/* The clock the target's slots run out at (btLmpLcParsePacket()). */
uint32_t btTableTargetClock __attribute__((section(".itcm.rw")));		/* 0x55304 */
/* Each rate class's slot windows (btLmpLcSetupTable() passes them to btLmpLcApplyTable()). */
uint8_t btTableOffsets[8] __attribute__((section(".itcm.rw"))) = { 7, 7, 7, 7, 7, 7, 0, 0 };	/* 0x55308 */
uint8_t btTableShortWindows[8] __attribute__((section(".itcm.rw"))) = { 4, 7, 10, 10, 13, 13, 13, 13 };	/* 0x55310 */
uint8_t btTableLongWindows[8] __attribute__((section(".itcm.rw")));	/* 0x55318 */
uint8_t btTableExtras[8] __attribute__((section(".itcm.rw"))) = { 4, 4, 4, 1, 1, 1, 1, 1 };	/* 0x55320 */
uint8_t btTableHoldWindows[8] __attribute__((section(".itcm.rw"))) = { 0, 3, 6, 9, 12, 12, 12, 12 };	/* 0x55328 */
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

void btLmpLcResetTable(void)
{
}

/* Puts the link in (set) or takes it out of the mode 0 mask. */
void btLmpLcSyncTable(int set, int link)
{
	unsigned int bit = 1 << link;

	if (set != 0)
		btTableLinkMask |= bit;
	else
		btTableLinkMask &= ~bit;
}

/*
 * Sets the windows of the profile. Outside mode 1 a link with a host slot has no offset (its offset
 * is its long window), and when the queue's head of state 9 is another link with a host slot the
 * offset grows by extra.
 */
void btLmpLcApplyTable(struct BtDeviceProfile *profile, uint8_t shortWindow, uint8_t offset, uint8_t longWindow,
		       uint8_t extra)
{
	struct BtDeviceProfile *head;

	profile->shortWindow = shortWindow;
	profile->offset = offset;
	profile->longWindow = longWindow;
	if (btTableLinkModes[profile->index] == 1)
		return;
	if (romHctHostSlot[profile->index][4] != 0) {
		profile->offset = 0;
		profile->longWindow = offset;
	}
	head = btSlotQueues[btSlotQueueIndex[9]].head;
	if (head != NULL && head != profile && romHctHostSlot[head->index][4] != 0)
		profile->offset += extra;
}

/*
 * The slot of the profile is taken (the ROM's handler btNextPayload does it): first the link's
 * settings and window are taken from its rings or its state, and given back afterwards.
 */
int btLmpLcResolveTable(const struct BtSlotEvent *event, struct BtDeviceProfile *profile, uint32_t clock,
			uint32_t argument)
{
	unsigned int burst = 8;
	int active = btLcoRomManageContext(profile->index);
	uint8_t secondSetting = profile->secondSetting;
	uint8_t firstSetting = profile->firstSetting;
	unsigned int state;
	unsigned int flags;
	int result;

	if (profile->adjust == 0) {
		if (btTableLinkModes[profile->index] == 1 && profile->active == 0) {
			btLmpLcSelectEntry(profile->index, clock);
			if (btSource.header & (1 << profile->index))
				profile->firstSetting = btLmpLcRecordHeader(profile->index);
			else
				profile->firstSetting = 1;
			btLmpLcVerifyEntry(profile->index, clock);
			if (((btSource.frame & (1 << profile->index)) | active) != 0) {
				profile->secondSetting = btLmpLcAdjustHeader(profile->index);
			} else {
				burst = 4;
				profile->secondSetting = 1;
			}
		} else {
			profile->due = clock;
		}
		profile->adjust = 1;
		state = event->state;
		if (state == 9 || state == 5) {
			if (btLmpLcDispatchTable(profile->index) != 0 &&
			    btLmpLcInspectEntry(btSource.packet, profile->index) == 0)
				goto limit;
			if (profile->active != 0) {
				profile->window = profile->span;
			} else {
				int late;

				if (btLmpLcInspectEntry(btSource.packet, profile->index) != 0)
					late = btLmpLcSelectEntry(profile->index, clock) < btSource.queue ||
					       (btLmpLcInspectEntry(btTableLinkMask, profile->index) == 0 &&
						btLmpLcVerifyEntry(profile->index, clock) < btSource.queue);
				else
					late = (int32_t)(profile->since + btTargetNode - clock) >= 0;
				if ((active | late) != 0) {
					burst = 8;
					profile->window = profile->rate;
				} else {
					burst = 1;
					profile->window = 1;
				}
			}
		} else if (profile->active != 0) {
			profile->window = profile->span > 21 ? 21 : profile->span;
		} else {
			profile->window = profile->rate;
		}
limit:
		profile->limit = profile->window;
	}
	profile->queued = clock;
	if (profile->limit != 0)
		profile->limit--;
	state = event->state;
	flags = profile->flags & ~3u;
	if (state == 6 || state == 5 ||
	    *(const BtHalf *)(btPacketTypes + 12 * romHctHostSlot[profile->index][2] + 4) > 72)
		profile->flags = flags + 1;
	else
		profile->flags = btDeviceWindow | btDeviceBitmap | flags;
	result = btNextPayload(event, profile, clock, argument, burst, 0xffff);
	profile->secondSetting = secondSetting;
	profile->firstSetting = firstSetting;
	return result;
}

int btLmpLcScheduleTable(const struct BtSlotEvent *event, struct BtDeviceProfile *profile, uint32_t clock,
			 uint32_t argument)
{
	return btLmpLcResolveTable(event, profile, clock, argument);
}

void btLmpLcNotifyTable(const struct BtSlotEvent *event, uint32_t clock)
{
	btLmpLcQueryTable(event, clock);
}

/* The link holds its slots: one tick windows, and outside mode 1 it goes into mode 3. */
void btLmpLcFormatTable(int link, uint32_t a1, uint32_t a2)
{
	struct BtDeviceProfile *profile = &btDeviceProfile[link];

	profile->hold = 1;
	profile->span = btTableRegister;
	profile->rate = 0;
	profile->window = btTableRegister;
	btLmpLcApplyTable(profile, btTableRegister, (uint8_t)a2, 0, 1);
	btLtsRomUpdateContext(link, a1, a2);
	if (btTableLinkModes[link] != 1) {
		btTableModeMask |= 1 << link;
		btTableLinkModes[link] = 3;
	}
}

int btLmpLcInspectTable(int link)
{
	btDeviceProfile[link].hold = 0;
	return 0;
}

/*
 * The host's command 0x0c48: a count over the length the command allows (less two) is cleared for
 * the ROM's handler, and given back to the command and its result.
 */
void btLmpLcTrackTable(uint8_t *command, uint8_t *result, uint32_t argument)
{
	unsigned int count;
	int length;

	if (*(const BtWord *)command != 0xc48) {
		btLcoRomReleaseState(command, result, argument);
		return;
	}
	count = (command[10] << 8) | command[9];
	length = ((command[12] << 8) | command[11]) - 2;
	if ((int)count > length) {
		command[9] = 0;
		command[10] = 0;
	}
	btLcoRomReleaseState(command, result, argument);
	command[9] = count;
	command[10] = count >> 8;
	*(BtHalf *)(result + 2) = count;
}

/* A value over 1 is moved out and replaced by 1. */
__attribute__((noinline)) void btLmpLcManageTable(uint8_t *value, uint8_t *previous)
{
	if (*value > 1) {
		*previous = *value;
		*value = 1;
	}
}

/*
 * What a link's slot setting is against the target link of the ROM: *other is 1 when the target is
 * another device, *entry the target's entry unless the target is this link; *setting the mean of the
 * link's two rate settings (in mode 1, those its rings would give), *active whether the link is in
 * use. A second link on the same device gives setting 1 and inactive.
 */
int btLmpLcAcquireTable(struct BtDeviceProfile *profile, const struct BtDeviceProfile *second, uint8_t *other,
			uint8_t *entry, uint8_t *setting, uint8_t *active)
{
	*other = 0;
	*entry = btTargetEntry;
	if (btTargetState != 0) {
		if (btTargetContext != 0) {
			if (btTargetContext != profile->active)
				*other = 1;
			else
				*entry = 0;
		} else if (btTargetState == (uint32_t)(uintptr_t)profile) {
			*entry = 0;
		}
		if (profile->state == 11 || profile->state == 10)
			*entry = 0;
	}
	if (profile->active == 0 && btTableLinkModes[profile->index] == 1 &&
	    (profile->state == 5 || profile->state == 9)) {
		uint8_t secondSetting = profile->secondSetting;
		uint8_t firstSetting = profile->firstSetting;

		if (btSource.header & (1 << profile->index))
			profile->firstSetting = btLmpLcRecordHeader(profile->index);
		else
			profile->firstSetting = 1;
		if (btSource.frame & (1 << profile->index))
			profile->secondSetting = btLmpLcAdjustHeader(profile->index);
		else
			profile->secondSetting = 1;
		*setting = (profile->secondSetting + profile->firstSetting) >> 1;
		*active = 0;
		profile->secondSetting = secondSetting;
		profile->firstSetting = firstSetting;
	} else {
		*setting = (profile->secondSetting + profile->firstSetting) >> 1;
		*active = profile->active != 0;
	}
	if (second != NULL && profile->active != 0 && profile->active == second->active) {
		*active = 0;
		*setting = 1;
		return 1;
	}
	return second != NULL ? profile->active : 0;
}

/* Some link runs in mode 1 or 3, or has a mask. */
__attribute__((noinline)) int btLmpLcReleaseTable(void)
{
	return (btTableLinkMask | btTableModeMask) != 0;
}

/* Gives each link in use (1 to 7) its slot windows, unless the ROM's target is busy. */
void btLmpLcSetupTable(void)
{
	const uint8_t *links = btLdrRomReleaseState();
	unsigned int link;

	btLtsRomFilterState();
	if (romTargetTable != 0 && romTargetTable != 5)
		return;
	btLmpLcRecordEntry();
	if (romTargetTable != 0)
		return;
	for (link = 1; link < 8; link = (uint16_t)(link + 1)) {
		const uint8_t *record = links + 116 * link;
		struct BtDeviceProfile *profile;

		if (record[0] == 0 || record[68] != 0 || record[52] != 0)
			continue;
		profile = &btDeviceProfile[link];
		if (btLmpLcDispatchTable(link)) {
			unsigned int rate = btLmpLcSelectHeader(link);

			if (profile->hold != 0) {
				unsigned int interval = profile->interval;
				unsigned int window = btTableRegister;
				int offset = btTableHoldWindows[rate];

				if (interval < window)
					window = interval;
				if ((int)(interval - window) < offset)
					offset = interval - window;
				btLmpLcApplyTable(profile, window, interval - offset - btTableRegister, offset, 1);
			} else {
				btLmpLcApplyTable(profile, btTableShortWindows[rate], btTableOffsets[rate],
						  btTableLongWindows[rate], btTableExtras[rate]);
			}
		} else if (romHctHostSlot[link][4] != 0) {
			if (btLcoRomManageContext(link) == 0 && btLmpLcReleaseTable() && btLmpLcFilterHeader()) {
				profile->span = 22;
				profile->interval = 16;
			} else {
				profile->span = btLinkDefaultSpan;
				profile->interval = 1;
			}
		}
	}
}

/*
 * The ROM's slot tracking of a link, wrapped: while the link waits in mode 2 with a mode 1 or 3
 * link elsewhere, its rate settings over 1 are taken down to 1 for the ROM and put back after. A
 * tracked slot about to be due goes to the ROM's link handler at once.
 */
int btLmpLcClearTable(uint32_t event, struct BtDeviceProfile *profile, uint32_t clock, uint32_t argument,
		      uint32_t burst, uint32_t limit)
{
	uint32_t second = 0;
	uint32_t first = 0;
	int result;

	if (btLmpLcReleaseTable() && !btLmpLcDispatchTable(profile->index) &&
	    btDeviceProfile[profile->index].state == 2) {
		btLmpLcManageTable(&profile->secondSetting, (uint8_t *)&second);
		btLmpLcManageTable(&profile->firstSetting, (uint8_t *)&first);
	}
	result = btLtsRomTrackState(event, profile, clock, argument, burst, limit);
	if (profile->tracked == 1 &&
	    (int)(profile->due - profile->guard - btLtsRomScheduleState(btLtsRomParseState(0))) < 2)
		btLdaRomVerifyState(profile->active, btLdrRomScheduleState(profile->index));
	if ((uint8_t)first != 0)
		profile->firstSetting = first;
	if ((uint8_t)second != 0)
		profile->secondSetting = second;
	return result;
}

int btLmpLcConvertTable(int link)
{
	btBdhRomReleaseState(link);
	return btLmpLcValidateDescriptor(link);
}

/*
 * A host request the ROM checks: a request of kind 36 whose link is in use (or not, with bit 0 of
 * the packet's flags clear) is kept while the coexistence allows it; every other goes back to the ROM.
 */
void btLmpLcVerifyTable(uint32_t *request)
{
	const uint8_t *links = btHccRomApplyState();
	const uint8_t *packet = (const uint8_t *)(uintptr_t)*request;
	uint32_t kind;

	btLdrRomDispatchState(packet + 7, &kind, 0);
	if ((uint16_t)kind != 36) {
		btHccRomSyncState(request);
		return;
	}
	if (links[120 * *(const BtHalf *)(packet + 4) + 3] != 0) {
		unsigned int mode = (packet[8] >> 3) & 3;

		if (mode == 0)
			return;
		if (mode != 1) {
			btHccRomSyncState(request);
			return;
		}
	} else if (packet[8] & 1) {
		btHccRomSyncState(request);
		return;
	}
	if (btCoexParam() == 0 && btLinkPayloadCount <= 1)
		btHccRomSyncState(request);
}

/* A host slot's message is freed before the ROM takes the request, and taken again for a session. */
int btLmpLcSelectTable(uint16_t *request)
{
	int result;

	if (*(BtWord *)&romHctHostSlot[*request][16] != 0)
		osa_MsgFree((void **)&romHctHostSlot[*request][16]);
	result = btLdaRomValidateState(request);
	if (result != 0 && (btHostFlags & 1) && btHostSession != 0 && *(BtWord *)&romHctHostSlot[*request][16] == 0)
		*(BtWord *)&romHctHostSlot[*request][16] = (uint32_t)(uintptr_t)osa_MsgAlloc(242);
	return result;
}

/* Frees the messages of every host slot and resets the ROM's host state. */
int btLmpLcAdjustTable(void)
{
	unsigned int slot;

	for (slot = 0; slot < 11; slot = (uint8_t)(slot + 1)) {
		if (*(BtWord *)&romHctHostSlot[slot][16] != 0)
			osa_MsgFree((void **)&romHctHostSlot[slot][16]);
	}
	btLdaRomComputeState();
	btHostBitmap = 0xffff;
	btTableModeTable = 0;
	return 0;
}

/* With the link controller's mode bits set, the host link's radio registers 0x6080 and 0x2000. */
__attribute__((noinline)) void btLmpLcRecordTable(void)
{
	unsigned int value;

	if (((*(const BtHalf *)(btLinkContext + 34) >> 4) & 3) == 0)
		return;
	value = romHctHostSlot[btLinkHostIndex][4];
	btTableRadioWrite((value << 5) | 0x6080, 0);
	btTableRadioWrite((value << 11) | 0x2000, 0);
}

void btLmpLcFilterTable(void)
{
	int scanning = btLinkScanState != 0;

	btLdaRomApplyState(btLinkScanState);
	if (!scanning)
		return;
	osa_LockAcquire();
	*(BtHalf *)(btLinkScanSlot + 36) = (*(BtHalf *)(btLinkScanSlot + 36) >> 12) << 12;
	osa_LockRelease();
	if (btLinkScanFlags & 2)
		btLmpLcRecordTable();
}
