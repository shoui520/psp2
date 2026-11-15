/*
 * The link controller's host link buffers (0x44ca6 on): the timing of the host link when the
 * radio's rate changes, and the link's teardown back to the ROM. The names of the firmware's
 * placeholders are kept; see robin/btbuffer.h.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
#include "robin/btcontext.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/bttable.h"
#include "robin/eventpower.h"

#define LINK_HALF(offset)	(*(BtHalf *)(btLinkContext + (offset)))

__attribute__((noinline)) void btLmpLcHandleBuffer(void)
{
	btLdaRomSyncState();
	btLmpLcRecordTable();
}

/*
 * With the link controller's mode bits set, no scan flags and the host timing on, the host link's
 * timing is doubled or halved for the radio's rate; otherwise the scan slot's count is cleared.
 */
void btLmpLcProcessBuffer(void)
{
	unsigned int divisor = 1;
	unsigned int factor = 1;

	if (((LINK_HALF(34) >> 4) & 3) != 0 && btLinkScanFlags == 0 && btHostTiming.enabled == 1) {
		unsigned int rate;

		LINK_HALF(404) |= btHostTiming.scale << 4;
		rate = LINK_HALF(116);
		if (btTableModeQueue & 1) {
			if (!(rate & 1))
				divisor = 2;
		} else if (rate & 1) {
			factor = 2;
		}
		btHostTiming.ticks[0] = btHostTiming.ticks[0] * factor / divisor;
		btHostTiming.ticks[1] = btHostTiming.ticks[1] * factor / divisor;
		btHostTiming.ticks[2] = btHostTiming.ticks[2] * factor / divisor;
		btHostTiming.ticks[3] = btHostTiming.ticks[3] * factor / divisor;
		btHostTiming.ticks[4] = btHostTiming.ticks[4] * factor / divisor;
		btHostTiming.slots[0] = btHostTiming.slots[0] * factor / divisor;
		btHostTiming.slots[1] = btHostTiming.slots[1] * factor / divisor;
		btLinkScanState = 1;
		btLmpLcHandleBuffer();
		return;
	}
	osa_LockAcquire();
	*(BtHalf *)(btLinkScanSlot + 36) = (*(BtHalf *)(btLinkScanSlot + 36) >> 12) << 12;
	osa_LockRelease();
	LINK_HALF(490) = 0;
}

void btLmpLcUpdateBuffer(void)
{
	*(BtHalf *)(btLinkScanSlot + 58) &= ~8;
	btLscRomSyncState(8, 0xcd0, 0, 0, romQueueLsc);
}

/*
 * The host link ends: a pending slot is given back, the scan slot and the link controller's mode
 * cleared, and the link's host slot handed to the ROM (its handles freed when it was in state 6,
 * otherwise by a message 0x0c21 to the link controller). The slot's message is freed.
 */
void btLmpLcConfigureBuffer(void)
{
	unsigned int link = btLinkHostIndex;
	uint8_t *slot;

	if (btLinkPendingSlot != 0) {
		unsigned int pending = btLinkPendingSlot;

		*(BtHalf *)(btLinkSlotTable + 8 * pending) = 1;
		*btLinkSlotMask &= ~(1 << pending);
		*btLinkStatus &= ~(1 << pending);
		btLdaRomProcessState(btLinkHostState + 24, pending);
		btLinkPendingSlot = 0;
	}
	btLinkEndHook();
	osa_LockAcquire();
	*(BtHalf *)(btLinkScanSlot + 34) &= ~0x80;
	*(BtHalf *)(btLinkScanSlot + 36) = (*(BtHalf *)(btLinkScanSlot + 36) >> 12) << 12;
	osa_LockRelease();
	LINK_HALF(34) &= 0xcd;
	slot = romHctHostSlot[link];
	if (slot[0] != 6) {
		uint8_t *message = osa_MsgAlloc(8);

		*(BtWord *)message = 0xc21;
		*(BtHalf *)(message + 4) = link;
		osa_MsgSend((void **)&message, romQueueLc);
	} else {
		btBdhRomPrepareState(0, slot[2]);
		btLdaRomProcessState(btLinkHostState + 24, slot[2]);
		btLdaRomProcessState(btLinkHostState + 24, slot[3]);
		btLdaRomProcessState(btLinkHostState + 32, slot[4]);
		btLinkLimitHook(slot[2], slot[3]);
		btTableRadioWrite((slot[4] << 5) | 0x6080, 0);
		btLdaRomProcessState(btLinkHostState + 20, (uint8_t)link);
		slot[0] = 0;
	}
	if (*(BtWord *)(slot + 16) != 0)
		osa_MsgFree((void **)(slot + 16));
}

/* The scan slot's flags of a host link. */
#define SCAN_LINK_FLAGS(link)	(*(BtHalf *)(btLinkScanSlot + 40 + 2 * (link)))

/* The host link's radio link starts its mode table: the ROM is told with a message 0x0cd4. */
void btLmpLcCheckBuffer(unsigned int link)
{
	if (link == 0 || romHctHostSlot[btLinkHostIndex][4] != link || btLinkIndexRecord == 0)
		return;
	btTableModeTable = 1;
	osa_LockAcquire();
	SCAN_LINK_FLAGS(link) |= 2;
	osa_LockRelease();
	btLscRomResolveState(0xcd4, link);
	btLscRomSyncState(4, 0xcd4, link, 0, romQueueLsc);
}

/*
 * The link controller's messages about the host link: 0x0c47 goes to btLmpLcVerifyState(), 0x0c4b
 * (the link's rate), 0x0c50 (its end), 0x0cd0 and 0x0cd4 are handled here and freed. Returns 1 for
 * a message taken.
 */
int btLmpLcValidateBuffer(uint8_t *message)
{
	int taken = 0;

	switch (*(const BtWord *)message) {
	case 0xc47:
		btLmpLcVerifyState();
		return taken;
	case 0xc4b:
		taken = 1;
		if (message[5] == 0) {
			btTableEvent = btBdhRomProcessState(250, 1);
			btBdhRomUpdateState(250, 70);
			btTableCommand = 1;
		}
		if (message[4] == 3)
			message[5] = 1;
		btTableModeQueue = LINK_HALF(116);
		btLdaRomResolveState(message, btTableModeQueue);
		if ((~LINK_HALF(34) & 0xa0) == 0) {
			osa_LockAcquire();
			*(BtHalf *)(btLinkScanSlot + 58) |= 8;
			osa_LockRelease();
		}
		if (!(LINK_HALF(34) & 0x10) && (message[4] & 2)) {
			osa_LockAcquire();
			SCAN_LINK_FLAGS(romHctHostSlot[btLinkHostIndex][4]) |= 2;
			osa_LockRelease();
		}
		break;
	case 0xc50:
		if (btTableCommand != 0) {
			btBdhRomUpdateState(250, btTableEvent);
			btTableCommand = 0;
			btTableEvent = 0;
		}
		osa_LockAcquire();
		SCAN_LINK_FLAGS(romHctHostSlot[btLinkHostIndex][4]) &= ~2;
		osa_LockRelease();
		btTableModeTable = 0;
		btLmpLcConfigureBuffer();
		taken = 1;
		break;
	case 0xcd0:
		taken = 1;
		if ((~LINK_HALF(34) & 0xa0) == 0) {
			osa_LockAcquire();
			*(BtHalf *)(btLinkScanSlot + 58) |= 8;
			osa_LockRelease();
		}
		break;
	case 0xcd4:
		if (*(const BtHalf *)(message + 4) == romHctHostSlot[btLinkHostIndex][4])
			btTableModeTable = 0;
		taken = 1;
		break;
	default:
		return taken;
	}
	osa_MsgFree((void **)&message);
	return taken;
}

void btLmpLcPrepareBuffer(void)
{
	btLdaRomResetState();
	if (btLinkChannelState == 0)
		btHostTiming.policy = 0x1f;
}

/*
 * The host's command 0x0c48 on a link of kind 42 not yet in use is refused (45) when its length
 * is not the link's (a link of kind 2) or too short (kind 1); every command then goes to the ROM,
 * and one over the link's limit is refused after it.
 */
int btLmpLcDispatchBuffer(uint8_t *command, const uint16_t *count, uint32_t a2, uint32_t a3, uint32_t a4,
			  uint32_t a5)
{
	uint32_t kind = *(const BtWord *)command;
	const uint8_t *links = btLcoRomVerifyState(0);
	const uint8_t *records = btHccRomApplyState();
	unsigned int limit;
	int result;

	if (kind == 0xc48) {
		unsigned int length = command[8] | (command[9] << 8);
		unsigned int handle = *(const BtHalf *)(command + 4);

		if ((command[7] >> 1) == 42 && records[120 * handle + 20] == 0) {
			const uint8_t *link = links + 30 * handle;

			if (link[24] == 2) {
				if (*(const BtHalf *)(link + 20) != length)
					return 45;
			} else if (link[24] == 1) {
				if ((unsigned int)(link[4] + link[6] + 4) > length)
					return 45;
			}
		}
	}
	result = btLcoRomManageState(command, count, a2, a3, a4, a5);
	if (result != 0)
		return result;
	limit = *(const BtHalf *)(records + 120 * *(const BtHalf *)(command + 4) + 42);
	if (limit != 0 && *count > limit)
		return 45;
	return result;
}

/* The profile takes the slot timing of a link in mode 1 that it shares the slots with. */
__attribute__((noinline)) void btLmpLcParseBuffer(struct BtDeviceProfile *profile,
						  const struct BtDeviceProfile *source)
{
	btLtsRomQueryState(profile);
	profile->adjust = 0;
	btLtsRomApplyState(profile, source->due, source->state);
	btLmpLcApplyTable(profile, source->shortWindow, source->offset, source->longWindow, 0);
	profile->window = source->window;
}

/*
 * A link is given its slots: those of another link in mode 1 (state 5 or 9), or else a mode 5
 * slot of its own from now. Its rate settings are taken from its rings.
 */
void btLmpLcBuildBuffer(int link)
{
	struct BtDeviceProfile *profile = &btDeviceProfile[link];
	unsigned int other;

	for (other = 1; other < 8; other = (uint16_t)(other + 1)) {
		const struct BtDeviceProfile *source = &btDeviceProfile[other];

		if (profile != source && (source->state == 5 || source->state == 9) &&
		    btTableLinkModes[other] == 1) {
			btLmpLcParseBuffer(profile, source);
			goto settings;
		}
	}
	btLtsRomQueryState(profile);
	btLtsRomApplyState(profile, btLtsRomScheduleState(btLtsRomParseState(0)), 5);
	btLmpLcApplyTable(profile, btTableShortWindows[0], btTableOffsets[0], btTableLongWindows[0], 0);
	profile->window = profile->shortWindow;
settings:
	profile->secondSetting = btLmpLcAdjustHeader(profile->index);
	profile->firstSetting = btLmpLcRecordHeader(profile->index);
}

/* A link's slots from the clock: those of the mode 1 link it shares, or the ROM's. */
void btLmpLcComputeBuffer(int link, uint32_t clock, unsigned int next, uint32_t interval)
{
	if (btTableLinkModes[link] == 1) {
		btLmpLcBuildBuffer(link);
		return;
	}
	if (next != 0)
		btNextWindow(link, clock, interval);
	else
		btLtsRomValidateContext(link, clock, interval);
}

/*
 * The ROM's link record decides a link's slots: a record in mode 0 starts from the clock, one in
 * mode 1 (unless held) from the ROM's next control slot; a link on a host device, and every other
 * record, goes to the ROM.
 */
void btLmpLcQueryBuffer(int link, uint32_t held)
{
	unsigned int device = romHctHostSlot[link][4];
	uint8_t *records = btLdrRomReleaseState();
	struct BtDeviceProfile *profile = &btDeviceProfile[link];
	const uint8_t *record;
	uint32_t clock;

	if (device != 0) {
		btLdrRomInspectState(link, held);
		return;
	}
	record = records + 116 * link;
	if (record[68] == 0) {
		btLtsRomConvertState(15);
		clock = btLtsRomParseState(0);
	} else if (record[68] == 1 && held == 0) {
		btNextControl(15, link);
		btLtsRomNotifyState(profile, btLtsRomScheduleState(btLtsRomParseState(0)));
		clock = btLtsRomResolveState();
	} else {
		btLdrRomInspectState(link, held);
		return;
	}
	btLmpLcComputeBuffer(link, clock, record[1], *(const BtHalf *)(record + 4));
}

/* The host's command for a link the ROM does not run yet sets its interval (in halves) and kind. */
void btLmpLcResetBuffer(uint8_t *command)
{
	uint8_t *records = btLdrRomReleaseState();
	uint8_t *record = records + 116 * *(const BtHalf *)(command + 4);

	if (record[68] == 0 && record[52] == 0) {
		btLdrRomFormatState(command);
		return;
	}
	*(BtHalf *)(record + 4) = (*(const BtHalf *)(command + 6) + 1) >> 1;
	records[116 * *(const BtHalf *)(command + 4) + 1] = command[8];
}

/* The slots a packet kind takes: 3 for the kinds 16, 32, 256 and 512, else 1. */
static inline unsigned int btLinkKindSlots(unsigned int kind)
{
	if (kind == 16 || kind == 32 || kind == 256 || kind == 512)
		return 3;
	return 1;
}

/*
 * Looks for a free slot for a new link from *slot on (the clock counting with it), against every
 * link the ROM runs on the same host device: a link in mode 1 holds the slots its clock and its
 * packets take, any other link those of its interval within the guard. The first free slot (or the
 * one the search ended at) is given back in *slot, with the link it stopped at in *link.
 */
void btLmpLcSyncBuffer(uint16_t *linkOut, uint8_t *slotOut, int clock, const uint8_t *request)
{
	const uint8_t *records = btLdrRomReleaseState();
	unsigned int slot = *slotOut;
	unsigned int self = *(const BtHalf *)(request + 4);
	unsigned int kind = *(const BtHalf *)(request + 12);
	int period = request[9] >> 1;
	unsigned int mark = request[10];
	unsigned int extra = request[15] >> 1;
	unsigned int device = romHctHostSlot[*(const BtHalf *)(request + 6)][4];
	unsigned int start = slot;
	unsigned int factor = btLinkSlotFactor;
	unsigned int guard;
	unsigned int link;
	int free;

	if (factor == 0)
		factor = 1;
	guard = (uint16_t)((int)(factor * 2) / 2);
	do {
		link = 1;
		free = 1;
		while (records[116 * link] == 0 && link < 11)
			link = (uint16_t)(link + 1);
		while (link != 11) {
			const uint8_t *record;
			const uint8_t *host;
			unsigned int interval;
			unsigned int count;
			unsigned int span;
			unsigned int length;
			unsigned int hits;
			unsigned int round;
			int window;
			int next;

			if (free != 1)
				goto advance;
			record = records + 116 * link;
			if (btLinkSessionCheck(link) && link != self && romHctHostSlot[link][4] == device) {
				if (record[28] == 1) {
					if (clock % period == *(const int *)(const void *)(record + 32) % period)
						goto advance;
				} else {
					unsigned int mine;
					unsigned int theirs;
					unsigned int own = btLinkKindSlots(kind);
					unsigned int other = btLinkKindSlots(*(const BtHalf *)(record + 40));

					if (*(const int *)(const void *)(record + 36) < period)
						window = (uint8_t)*(const int *)(const void *)(record + 36);
					else
						window = period;
					mine = (uint8_t)(own + extra);
					theirs = (uint8_t)(record[47] + other);
					while (own + other <= (unsigned int)window && mine + theirs > (unsigned int)window) {
						if (theirs > other)
							theirs = (uint8_t)(theirs - 1);
						else if (mine > own)
							mine = (uint8_t)(mine - 1);
						else
							break;
					}
					while (free == 1 && mine >= 1) {
						unsigned int k;

						for (k = theirs; free == 1 && k >= 1; k = (uint8_t)(k - 1)) {
							int otherClock = *(const int *)(const void *)(record + 32) + k - 1;

							if ((int)(clock + mine - 1) % window == otherClock % window) {
								free = 0;
								clock = otherClock;
								if (otherClock >= 0x2000000)
									otherClock -= 0x2000000;
								slot = (uint8_t)(otherClock % period);
							}
						}
						mine = (uint8_t)(mine - 1);
					}
					if (free != 1)
						goto advance;
				}
			} else if (free != 1) {
				goto advance;
			}
			if (record[68] == 0 || record[68] == 4)
				goto next_link;
			interval = *(const BtHalf *)(record + 84);
			if (interval % period != 0 && period % interval != 0)
				goto next_link;
			host = romHctHostSlot[link];
			if (host[4] == 0 && btLinkPayloadRegister != 0)
				goto next_link;
			if (device != 0 && host[4] == device && (record[69] == 4 || record[69] == 3) &&
			    (int)(btLinkPayloadCount - btLinkPayloadRegister) > 0)
				goto next_link;
			window = interval < (unsigned int)period ? (uint8_t)interval : period;
			count = btLinkKindSlots(kind);
			next = btNextSession(link);
			if (device == 0 && host[4] != 0)
				count = (uint8_t)(count + 1);
			span = btLinkSpanEnabled ? *(const BtHalf *)(record + 90) : 1;
			length = (uint16_t)(guard + span - 1);
			hits = 0;
			for (round = 1; round <= count; ) {
				unsigned int missed = 0;
				int base = clock + round;
				unsigned int j;

				for (j = 1; j <= length; j = (uint16_t)(j + 1)) {
					if ((base - 1) % window == (int)(next + j - 1) % window) {
						hits = (uint16_t)(hits + 1);
						free = 0;
						if ((int)(length - j) < (int)(guard + (uint8_t)(count - round))) {
							if (missed >= guard)
								free = 1;
						} else if (mark != 0xf8 || j > guard) {
							free = 1;
						}
						break;
					}
					missed = (uint16_t)(missed + 1);
				}
				round = (uint8_t)(round + 1);
				if (hits != 0)
					break;
			}
			if (free != 1)
				goto advance;
next_link:
			do
				link = (uint16_t)(link + 1);
			while (records[116 * link] == 0 && link < 11);
		}
		break;
advance:
		clock++;
		slot = (uint8_t)(slot + 1);
		if ((int)slot == period)
			slot = 0;
	} while (slot != start);
	*linkOut = link;
	*slotOut = slot;
}
