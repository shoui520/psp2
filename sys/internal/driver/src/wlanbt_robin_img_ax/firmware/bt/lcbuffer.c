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
#include "robin/rfbus.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/threadx.h"

#define LINK_HALF(offset)	(*(BtHalf *)(btLinkContext + (offset)))
/* The due clock each link's slot was last taken at (btLmpLcAdjustBuffer()). */
uint32_t btLinkTakenClock[8] __attribute__((section(".itcm.bss")));		/* 0x59ee8 */

/* Each link's last eight rate classes (btLmpLcSelectHeader()). */
struct BtRateHistory btRateHistory[8] __attribute__((section(".itcm.bss")));	/* 0x59f08 */

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

/*
 * A host's request for a new link's slots: the search starts where the request says (or, for mark
 * 0xf8, after the link it continues or the ROM's last links), counted from the device's clock. A
 * free slot is sent to the link controller as message 0x0c62 and kept in btTableSlotRequest;
 * otherwise message 0x0c63 refuses the request (0x1b, or 0x34 for a request of mode 1).
 */
void btLmpLcApplyBuffer(uint8_t *request)
{
	const uint8_t *records = btLdrRomReleaseState();
	unsigned int link = *(const BtHalf *)(request + 4);
	unsigned int host = *(const BtHalf *)(request + 6);
	int period = request[9] >> 1;
	unsigned int wrap = request[10];
	unsigned int refusal = 1;
	unsigned int extra;
	unsigned int device;
	uint32_t now;
	uint32_t native;
	uint16_t found;
	uint8_t slot;
	unsigned int offset;
	unsigned int start;
	uint8_t *message;

	if (request[14] == 1)
		refusal = 0;
	else
		request[15] = 0;
	extra = request[15] >> 1;
	device = romHctHostSlot[host][4];
	if (device == 0)
		now = native = btLtsRomParseState(device);
	else
		btLtsRomBuildState(device, &now, &native);
	if (wrap == 0xf8) {
		int clock;

		if (btLinkSessionCheck(link) == 1) {
			clock = *(const int *)(const void *)(records + 116 * link + 32);
			if (device != 0 && clock > (int)now)
				clock = native + clock - now;
		} else if (btLdrRomAcquireState() != 0) {
			uint16_t other;

			clock = native;
			for (other = 8; other <= 10; other++) {
				if (btLinkSessionCheck(other) == 1) {
					const uint8_t *record = records + 116 * other;

					clock = *(const int *)(const void *)(record + 32) + record[47] +
						btLinkKindSlots(*(const BtHalf *)(record + 40));
				}
			}
		} else {
			clock = native;
		}
		if (clock >= 0x2000000) {
			wrap = 2;
			slot = (clock - 0x2000000) % period;
		} else {
			wrap = 0;
			slot = clock % period;
		}
	} else {
		slot = request[8] >> 1;
	}
	if (btLinkWrapCheck(wrap))
		start = (uint8_t)((int)native % period);
	else
		start = (uint8_t)((int)(native ^ 0x2000000) % period);
	if (slot > start)
		offset = (uint8_t)(slot - start + period);
	else
		offset = (uint8_t)(slot - start);
	btLmpLcSyncBuffer(&found, &slot, now + offset, request);
	if (found == 11) {
		struct BtSlotRequest *pending;

		message = osa_MsgAlloc(12);
		*(BtWord *)message = 0xc62;
		*(BtHalf *)(message + 4) = host;
		message[6] = slot * 2;
		message[7] = wrap;
		*(BtHalf *)(message + 8) = *(const BtHalf *)(request + 12);
		message[10] = request[14];
		pending = btTableSlotRequest;
		pending->active = 1;
		pending->link = host;
		pending->offset = slot * 2;
		pending->period = period * 2;
		pending->wrap = wrap;
		pending->packet = *(const BtHalf *)(request + 12);
		pending->mode = request[14];
		pending->extra = extra * 2;
	} else {
		message = osa_MsgAlloc(8);
		*(BtWord *)message = 0xc63;
		*(BtHalf *)(message + 4) = host;
		message[6] = refusal == 1 ? 0x1b : 0x34;
		message[7] = request[14];
	}
	osa_MsgSend((void **)&message, romQueueLcp);
}

/* A link's mode changes: links in mode 1 or 3 (or held in mode 0 or 2) are marked in the mode mask. */
void btLmpLcResolveBuffer(int link, unsigned int mode)
{
	struct BtDeviceProfile *profile = &btDeviceProfile[link];
	unsigned int bit = 1 << link;
	unsigned int mask = btTableModeMask;
	unsigned int set = (uint8_t)(bit | mask);

	btTableLinkModes[link] = mode;
	switch (mode) {
	case 0:
	case 2:
		if (profile->hold != 0) {
			btTableModeMask = set;
			btTableLinkModes[link] = 3;
		} else {
			btTableModeMask = mask & ~bit;
		}
		break;
	case 1:
		if ((profile->state == 5 || profile->state == 9) && profile->active == 0)
			btLmpLcBuildBuffer(link);
		btTableModeMask |= bit;
		break;
	case 3:
		btTableModeMask = set;
		break;
	}
}

__attribute__((noinline)) void btLmpLcScheduleBuffer(int link, uint16_t interval)
{
	btDeviceProfile[link].interval = interval;
}

/*
 * The link controller's messages to the slot code: links leaving (0x0bf7) and all of them
 * (0x0c19), slot requests (0x0c64), interval changes (0x0c1e, 0x0c74, 0x0cd2), mode changes
 * (0x0cd3) and the ROM's rate and device events. Returns 1 for a message taken and freed.
 */
int btLmpLcNotifyBuffer(uint8_t *message)
{
	int taken = 0;
	uint8_t *reply;

	switch (*(const BtWord *)message) {
	case 0xbf7: {
		unsigned int link = *(const BtHalf *)(message + 4);
		unsigned int bit;

		if (link >= 8)
			return taken;
		bit = 1 << link;
		btTableLinkMask &= ~bit;
		btTableModeMask &= ~bit;
		btTableLinkModes[link] = 0;
		return taken;
	}
	case 0xc19:
		__builtin_memset(btTableRetries, 0, sizeof(btTableRetries));
		__aeabi_memclr4(btRateHistory, sizeof(btRateHistory));
		btTableLinkMask = 0;
		__builtin_memset(btTableLinkModes, 0, sizeof(btTableLinkModes));
		btTableModeMask = 0;
		return taken;
	case 0xc1e:
		btLdrRomNotifyState(message);
		btLmpLcScheduleBuffer(*(const BtHalf *)(message + 4), btTableModeDescriptor);
		break;
	case 0xc23:
		if (btLinkPendingRate != 0xffff) {
			btLtsRomConvertState(btLinkPendingRate);
			btLinkPendingRate = 0xffff;
			btNextRate(1, 0);
			reply = osa_MsgAlloc(4);
			*(BtWord *)reply = 0xc22;
			osa_MsgSend((void **)&reply, romQueueLcp);
		}
		break;
	case 0xc5a:
		break;
	case 0xc64:
		btLmpLcApplyBuffer(message);
		break;
	case 0xc74:
		btLmpLcResetBuffer(message);
		break;
	case 0xcc8:
		btLmpLcSetupTable();
		break;
	case 0xccf:
		btNextBitmap(12, btDeviceContext.queue, 1, btDeviceContext.timings[1].frame,
			     btDeviceContext.timings[1].packet, btDeviceContext.timings[1].buffer);
		break;
	case 0xcd2: {
		const uint8_t *records = btLdrRomReleaseState();
		unsigned int link = *(const BtHalf *)(message + 4);

		btDeviceProfile[link].interval = *(const BtHalf *)(records + 116 * link + 4);
		break;
	}
	case 0xcd3:
		btLmpLcResolveBuffer(*(const BtHalf *)(message + 4), message[6]);
		break;
	default:
		return taken;
	}
	taken = 1;
	osa_MsgFree((void **)&message);
	return taken;
}

/* The class of a link's packet flags: one of 15 combinations the ROM knows, else 14. */
int btLmpLcFormatBuffer(const uint8_t *flags)
{
	unsigned int mask = 0;

	if (flags[13] == 1 || flags[12] != 0)
		mask = 0x800;
	if (flags[1] == 1)
		mask |= 1;
	if (flags[2] == 1)
		mask |= 2;
	if (flags[0] == 1)
		mask |= 4;
	if (flags[10] == 1)
		mask |= 8;
	if (flags[4] == 1)
		mask |= 0x10;
	if (flags[3] == 1)
		mask |= 0x20;
	if (flags[8] != 0)
		mask |= 0x40;
	if (flags[9] != 0)
		mask |= 0x80;
	if (flags[7] == 1)
		mask |= 0x100;
	if (flags[5] == 1)
		mask |= 0x200;
	if (flags[6] == 1)
		mask |= 0x400;
	if (mask != 4 && mask != 12)
		mask &= ~4;
	switch (mask) {
	case 0:
		return 0;
	case 4:
	case 0xc:
		return 1;
	case 0x20:
		return 2;
	case 0x22:
		return 3;
	case 0x32:
		return 4;
	case 2:
	case 0x12:
		return 5;
	case 1:
	case 9:
		return 6;
	case 0x201:
		return 7;
	case 0x400:
		return 9;
	case 0x100:
		return 10;
	case 0x40:
		return 11;
	case 0x80:
	case 0x209:
		return 12;
	default:
		return 14;
	}
}

/*
 * The ROM's role change of a link, unless it is the one the host policy already runs: a link
 * taking role 1 marks the policy (bit 19), which stays only in the states 10, 11, 12 and 55.
 */
void btLmpLcInspectBuffer(unsigned int link, unsigned int role)
{
	unsigned int flags;

	if (btHostPolicy.link == link && btHostPolicy.role == role && btHostPolicy.busy != 0)
		return;
	btLcoRomNotifyState(link, role);
	if (role == 1)
		btHostPolicy.flags |= 0x80000;
	flags = btHostPolicy.flags;
	if ((flags & 0x80000) && btHostPolicy.state != 10 && btHostPolicy.state != 11 &&
	    btHostPolicy.state != 12 && btHostPolicy.state != 55)
		btHostPolicy.flags = flags & ~0x80000;
	if (role == 1)
		btHostPolicy.flags &= ~(role << 17);
}

/*
 * One step of the ROM's role switch (btHostPolicy.step): a pending restart (step 16) or abort
 * (step 12) is taken first; then the steps run while the local and the peer's flag of the step
 * agree, steps 2 and 3 also settling the role bits. Returns the local flag of the step it ended at.
 */
int btLmpLcTrackBuffer(void)
{
	uint8_t *source = btLcoRomRecordState();
	uint8_t *record = btLcoRomFilterState();
	uint32_t from;
	uint32_t to;
	int step;
	unsigned int local;
	unsigned int peer;

	if (record[11] == 1) {
		record[11] = 0;
		if (btHostPolicy.state != 0x37) {
			btHostPolicy.step = 16;
			return 1;
		}
	}
	if (record[12] != 0) {
		btHostPolicy.step = 12;
		record[12] = 0;
		return 1;
	}
	from = btRoleSource(source);
	to = btRoleRecord(record);
	btRoleUpdate(from, to);
	for (;;) {
		unsigned int hold;
		unsigned int allowed;
		int same;

		step = btHostPolicy.step;
		if (step == 3 && !(btHostPolicy.flags & 0x80000) && (btPeerPolicy & 0x80000))
			btPeerPolicy &= ~0x80000;
		if (step == 2) {
			uint32_t flags = btHostPolicy.flags;
			uint32_t other = btPeerPolicy;
			unsigned int flag3 = (flags >> 3) & 1;
			unsigned int flag2 = (flags >> 2) & 1;
			unsigned int flag19 = (flags >> 19) & 1;
			unsigned int peer2 = (other >> 2) & 1;
			unsigned int peer3 = (other >> 3) & 1;
			unsigned int peer19 = (other >> 19) & 1;

			if (flag3 && peer3 && !peer2) {
				if (!flag2) {
					if (flag19 && !peer19 && btRoleAllowed()) {
						btHostPolicy.flags = (btHostPolicy.flags & ~8) | 4;
						btPeerPolicy |= 0x80000;
						btHostPolicy.step++;
					}
				} else if (!flag19 && peer19 && btRoleAllowed()) {
					btHostPolicy.flags &= ~8;
					btPeerPolicy &= ~0x80000;
					btHostPolicy.step++;
				}
			} else if (!(flag3 | peer3 | peer2) && flag2 && flag19 && peer19) {
				btPeerPolicy = other & ~0x80000;
			}
		}
		step = btHostPolicy.step;
		local = (btHostPolicy.flags >> step) & 1;
		peer = (btPeerPolicy >> step) & 1;
		hold = 0;
		if (step == 11)
			hold = (btHostPolicy.flags >> 17) & 1;
		allowed = 0;
		if (step == 4 || step == 5 || step == 6) {
			allowed = (btHostPolicy.flags >> 18) & 1;
			if (local != peer)
				hold = allowed;
		}
		if (local == 1 && peer == 0 && allowed == 1) {
			btHostPolicy.busy = 0;
			btRoleUpdate(from, to);
		} else {
			btHostPolicy.step = step - 1;
		}
		same = local == peer;
		if (same && local != 0 && step == 11)
			same = 0;
		if (hold != 1 && same != 1)
			break;
	}
	btHostPolicy.step = (int8_t)(btHostPolicy.step + 1);
	if (record[2] == 1 && btHostPolicy.step < 14)
		record[3] = 0;
	return local;
}

/*
 * The ROM's handler of a role switch step, with steps 2 and 11 taken here: step 2 asks the host
 * link's limits (when the host has any), step 11 the role command; a step not yet answered is
 * run again and marks the peer's flag (bit 2, bit 11) while the switch is busy.
 */
void btLmpLcManageBuffer(int step)
{
	const uint8_t *state = btLcoRomAdjustState(step);

	if (step == 2) {
		const uint8_t *source = btLcoRomRecordState();
		const uint8_t *record = btLcoRomFilterState();

		btHostPolicy.pending = 1;
		btHostPolicy.busy = 1;
		btPeerPending = 0;
		if (btHostFlags != 0) {
			if (btLinkLimitQuery() == -1) {
				unsigned int any = source[0] != 0 || source[1] != 0 || record[0] != 0;

				if (btHostSetting != any || btHostRoleWanted != btHostRoleNow)
					btLinkLimitPacket();
				else if (btRoleAllowed() == 1)
					btLinkLimitFrame();
			}
			if (btPeerPending != 0)
				return;
		}
		btHostPolicy.step--;
		if (btHostPolicy.busy == 1)
			btPeerPolicy |= 4;
	} else if (step == 11) {
		unsigned int factor;
		int allow;

		btHostPolicy.pending = 1;
		btHostPolicy.busy = 1;
		btPeerPending = 1;
		factor = btLinkSlotFactor;
		if (btHostPolicy.state == 14)
			allow = btPeerLimit > factor;
		else if (btHostPolicy.state == 0x37 && state[0] != 0 && btPeerLimit < factor)
			allow = 0;
		else
			allow = 1;
		if (btRoleCommand((int8_t)factor, allow) != 0)
			btPeerPending = 0;
		else if (btPeerPending != 0)
			return;
		btHostPolicy.step--;
		if (btHostPolicy.busy == 1)
			btPeerPolicy |= 0x800;
	} else {
		btLcoRomInspectState(step);
	}
}

/*
 * The peer's limit changes to limit (apply 1; otherwise it is only recorded): each connected link
 * (state 2, not kind 6) is moved to the new limit, down or up. Returns 0 when a link is still
 * moving, and marks the change pending.
 */
int btLmpLcAcquireBuffer(int limit, int apply)
{
	uint8_t *states = btLcoRomVerifyState(0);
	uint8_t *links = btHccRomApplyState();
	int settled = 1;
	int notify;
	int direction;
	unsigned int floor;
	unsigned int link;

	if (apply != 1) {
		btPeerLimit = limit;
		return 1;
	}
	if (limit == 0) {
		notify = 1;
		direction = btPeerLimit == 1 ? 0 : 1;
	} else if (limit == 1) {
		notify = btPeerLimit == 0 ? -1 : 0;
		direction = btPeerLimit == 0 ? 0 : 1;
	} else {
		direction = btPeerLimit > limit ? 1 : -1;
		notify = btPeerLimit == 0 ? -1 : 0;
	}
	btPeerLimit = limit;
	floor = (uint8_t)(limit != 0 ? limit : 1);

	for (link = 0; link < 11; link = (uint16_t)(link + 1)) {
		const uint8_t *entry = &links[120 * link];
		uint8_t *state;

		if (entry[0] != 2 || entry[21] == 6)
			continue;
		state = &states[30 * link];
		if (direction == -1) {
			if (state[8] <= btPeerLimit) {
				if (state[16] != 1 && state[8] > state[6]) {
					uint8_t message[3];

					message[0] = 0;
					message[1] = btPeerLimit;
					message[2] = 0;
					btLcoRomSetupState(29, link, message, btLinkPayloadHook);
				}
				state[8] = 0;
			}
		} else if (direction == 1) {
			unsigned int current = state[6];

			if (current > floor) {
				state[15] = 1;
				if (state[16] == 0) {
					state[8] = current;
					state[6] = floor;
					btHccRomResolveState(link, entry[20]);
				}
			}
		}

		if (direction == -1) {
			unsigned int target = btLinkEventHook(btLinkPolicyHook(link, btPeerLimit));

			if (state[4] != target) {
				uint8_t message[3];

				message[0] = target;
				message[1] = 0;
				message[2] = 1;
				btLcoRomSetupState(29, link, message, btLinkPayloadHook);
			}
		} else if (direction == 1) {
			unsigned int pending = state[5];

			if (pending > floor || (pending == 0 && state[4] > floor)) {
				state[11] = 1;
				if (state[10] == 0 && state[12] == 0 && pending == 0) {
					state[5] = floor;
					if (btLinkRecordHook(link) == 1)
						btLinkProfileHook(link);
				}
			} else if (pending == 0 && state[4] == floor && *btLcoRomAdjustState(floor) != 0) {
				state[11] = 1;
				if (state[10] == 0 && state[12] == 0)
					btLinkRecordHook(link);
			}
		}

		if (state[15] == 1 || state[11] == 1)
			settled = 0;
		if (state[11] == 0 && notify != 0)
			btLinkRegisterHook(link);
	}
	if (settled == 1)
		return 1;
	btPeerLimitPending = 1;
	return 0;
}

/*
 * The ROM's slot hook: the link's slot is released with its windows as the due clock. Then, while
 * the mask or the record wants it and the link of state 9's queue is not held, not masked and has a
 * ring in use, a link whose span ran out before clock is scheduled again.
 */
void btLmpLcReleaseBuffer(struct BtSlotEvent *event, uint32_t clock)
{
	struct BtDeviceProfile *profile = event->profile;
	uint32_t due = profile->due;
	struct BtDeviceProfile *head;

	profile->due = profile->anchor;
	btLtsRomReleaseState(event);
	profile->due = due;
	if (btLinkMaskState != 1 && btLinkIndexRecord != 1)
		return;
	head = btSlotQueues[btSlotQueueIndex[9]].head;
	if (head == NULL || head->hold != 0)
		return;
	if (btLmpLcInspectEntry(btTableLinkMask, head->index) != 0)
		return;
	if (!(btSource.frame & (1u << head->index)) && !(btSource.header & (1u << head->index)))
		return;
	if ((int32_t)(clock - (profile->span + due)) <= 0)
		return;
	btLtsRomQueryState(profile);
	btLtsRomApplyState(profile, btLtsRomNotifyState(profile, clock), event->state);
}

/*
 * The link's step record took a step at clock: its next slot is mode 11, or mode 7 again once
 * 0x400 ticks have passed since its due clock (kind 7) or its anchor (otherwise, with the window
 * set to 9).
 */
void btLmpLcSetupBuffer(struct BtDeviceProfile *profile, uint32_t clock, struct BtStepRecord *step,
			const struct BtSlotTiming *timing, unsigned int kind)
{
	uint32_t due;

	btLinkStepHook(step, timing, clock, 0);
	profile->adjust = 0;
	btLtsRomQueryState(profile);
	due = btLtsRomNotifyState(profile, clock);
	if (kind == 7) {
		if ((int32_t)(due - profile->due) < 0x400) {
			btLtsRomApplyState(profile, due, 11);
			return;
		}
	} else if ((int32_t)(due - profile->anchor) < 0x400) {
		if (profile->keepWindow == 0)
			profile->window = timing->packet;
		btLtsRomApplyState(profile, due, 11);
		return;
	} else {
		profile->window = 9;
	}
	btLtsRomApplyState(profile, due, 7);
}

/* The ROM's slot hook of modes 7 and 11: at the end of the link's window, its next slot. */
void btLmpLcClearBuffer(struct BtSlotEvent *event, uint32_t clock)
{
	struct BtDeviceProfile *profile = event->profile;
	unsigned int window = profile->window;
	struct BtStepRecord *step;
	const struct BtSlotTiming *timing;
	unsigned int keep;
	unsigned int packet;

	if ((int32_t)(clock - (window + profile->due)) < 0)
		return;
	if (profile->stepKind == 1) {
		step = &btDeviceContext.steps[1];
		timing = &btDeviceContext.timings[1];
	} else {
		step = &btDeviceContext.steps[0];
		timing = &btDeviceContext.timings[0];
	}
	keep = profile->keepWindow;
	packet = timing->packet;
	if (event->state == 7) {
		if (step->steps < 9)
			return;
		step->steps = 0;
		if (keep == 0) {
			profile->anchor = profile->due;
			if (profile->window != packet) {
				profile->window = timing->packet - profile->window;
				goto apply;
			}
		} else if (profile->repeats == 0) {
			profile->window += 9;
			btLmpLcHandleDescriptor(step);
			profile->repeats++;
			return;
		} else {
			profile->anchor = profile->due;
			if (packet == 9)
				goto restart;
			btLmpLcHandleDescriptor(step);
			profile->repeats++;
			profile->window = 9;
			profile->adjust = 0;
			goto apply;
		}
	} else if (event->state == 11) {
		if (keep != 0) {
			if (profile->repeats < profile->repeatLimit) {
				profile->window = window + 9;
				profile->repeats++;
				btLmpLcHandleDescriptor(step);
				btTargetRepeat = 0;
				return;
			}
restart:
			profile->repeats = 0;
			profile->window = 9;
		}
	} else {
		return;
	}
	btLmpLcSetupBuffer(profile, clock, step, timing, event->state);
	return;
apply:
	btLtsRomQueryState(profile);
	btLtsRomApplyState(profile, clock, 11);
}

/* The ROM's queue hook: the profile joins the queue, and its slot kind counts a step. */
void btLmpLcConvertBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile)
{
	btLtsRomSetupState(queue, profile);
	if (profile->stepKind == 1)
		btDeviceContext.steps[1].steps++;
	else
		btDeviceContext.steps[0].steps++;
}

/*
 * Places the profile's slot near clock: at the first multiple of its interval from the place the
 * ROM gives where it does not overlap a queued link's slot, or, where the queued link was taken
 * later, ahead of it, which then moves that link behind its own.
 */
void btLmpLcVerifyBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile, uint32_t clock)
{
	unsigned int step = 0;
	struct BtDeviceProfile *node;
	uint32_t base;
	int mode;

	if (btLmpLcReleaseTable() && btLmpLcDispatchTable(profile->index) == 0 && profile->interval < 9)
		base = btLinkPlaceHook(profile, profile->interval + clock, 0);
	else
		base = btLinkPlaceHook(profile, clock, 0);

	for (node = queue->head; node != NULL; node = node->next) {
		uint32_t start = profile->interval * step + base - profile->guard;
		uint32_t end = profile->baseWindow + ((profile->secondSetting + profile->firstSetting) >> 1) + start - 2;
		uint32_t nodeStart = node->due - node->guard;
		uint32_t nodeEnd = node->baseWindow + node->guard +
				   (((node->secondSetting + node->firstSetting) >> 1) + nodeStart) - 2;

		if (((int32_t)(start - nodeStart) >= 0 && (int32_t)(nodeEnd - start) >= 0) ||
		    ((int32_t)(nodeStart - start) >= 0 && (int32_t)(end - nodeStart) >= 0)) {
			if (start - btLinkTakenClock[profile->index] > nodeStart - btLinkTakenClock[node->index]) {
				btLtsRomQueryState(node);
				btLmpLcVerifyBuffer(queue, node, nodeEnd);
				break;
			}
			step = (uint8_t)(step + 1);
		}
	}
	profile->adjust = 0;
	profile->window = profile->baseWindow;
	mode = btLmpLcFilterHeader() != 0 && btLdrRomResolveState(profile->index) != 0;
	btLtsRomApplyState(profile, profile->interval * step + base, mode ? 4 : 2);
}

/* Every queued link whose window ran out by clock is placed again. */
void btLmpLcSelectBuffer(struct BtSlotQueue *queue, uint32_t clock)
{
	struct BtDeviceProfile *profile;

	while ((profile = queue->head) != NULL) {
		unsigned int window = (uint16_t)(profile->window + (profile->adjust == 3 ? 1 : 0));
		int limit;

		if (profile->span <= window)
			window = profile->span;
		limit = profile->interval - ((profile->secondSetting + profile->firstSetting) >> 1);
		if (limit <= (int)window)
			window = limit;
		if ((int32_t)(clock - ((uint16_t)window + profile->due)) < 0)
			return;
		btLtsRomQueryState(profile);
		btLmpLcVerifyBuffer(queue, profile, clock);
		btLtsRomInspectState(profile);
	}
}

/* The ROM's queue hook: the profile's slot is taken; its span is 2 while other links are masked. */
void btLmpLcAdjustBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile)
{
	btLtsRomAcquireState(queue, profile);
	btLinkTakenClock[profile->index] = profile->due;
	if (profile->active == 0 && btTableLinkMask != 0 && (btTableLinkMask & ~(1u << profile->index)) != 0)
		profile->span = 2;
	else
		profile->span = btLinkDefaultSpan;
}

/*
 * Whether the link of state 9's queue is busy at clock: its packet type takes more than half the
 * target's time, or half of it has passed since its slot was due.
 */
__attribute__((noinline)) int btLmpLcRecordBuffer(uint32_t clock)
{
	const struct BtDeviceProfile *head = btSlotQueues[btSlotQueueIndex[9]].head;
	unsigned int half;

	if (head == NULL)
		return 0;
	half = btTargetConfig >> 1;
	if (*(const BtHalf *)(btPacketTypes + 12 * romHctHostSlot[head->index][2] + 4) > half)
		return 1;
	return (int32_t)(clock - head->due) >= (int)half;
}

/*
 * The ROM's slot hook of mode 1: at the end of the link's window, every queued link whose window
 * ran out is scheduled again, and the link's slot, extended by a mode 8 slot on btExtendProfile
 * where it asks for one and the state 9 link is not busy.
 */
void btLmpLcFilterBuffer(struct BtSlotEvent *event, uint32_t clock)
{
	struct BtDeviceProfile *profile = event->profile;
	uint32_t due = profile->due;
	struct BtDeviceProfile *node;

	if ((int32_t)(clock - (profile->window + due)) < 0)
		return;
	if (event->state != 1) {
		btLtsRomQueryState(profile);
		return;
	}
	btLtsRomQueryState(profile);
	while ((node = event->profile) != NULL && (int32_t)(clock - (node->due + node->window)) >= 0) {
		btLtsRomQueryState(node);
		btLtsRomApplyState(node, btLtsRomNotifyState(node, clock), event->state);
	}
	if (profile->extend != 0 && profile->extendWindow != 0 && btLmpLcFilterHeader() == 0 &&
	    btLmpLcRecordBuffer(clock) == 0) {
		unsigned int average = (profile->secondSetting + profile->firstSetting) >> 1;

		btLtsRomQueryState(&btExtendProfile);
		__aeabi_memcpy4(&btExtendProfile, profile, sizeof(btExtendProfile));
		btLtsRomComputeState(&btExtendProfile);
		btExtendProfile.window = profile->extendWindow - average + 1;
		btLtsRomApplyState(&btExtendProfile, due + average, 8);
	}
	profile->adjust = 0;
	btLtsRomApplyState(profile, btLtsRomNotifyState(profile, clock), event->state);
}

/* Gives the ROM's slot states their queues and the image's hooks, and starts the slot tables over. */
void btLmpLcHandlePacket(void)
{
	btStateHooks[1].enter = (void (*)(void))btLmpLcFormatEntry;
	btStateHooks[1].due = btLmpLcFilterBuffer;
	btStateHooks[2].enter = (void (*)(void))btLmpLcAdjustBuffer;
	btStateHooks[4].enter = (void (*)(void))btLmpLcAdjustBuffer;
	btStateHooks[2].due = (void (*)(struct BtSlotEvent *, uint32_t))btLmpLcSelectBuffer;
	btStateHooks[4].due = (void (*)(struct BtSlotEvent *, uint32_t))btLmpLcSelectBuffer;
	btSlotQueueIndex[4] = 1;
	btSlotQueueIndex[6] = 2;
	btSlotQueueIndex[1] = 3;
	btSlotQueueIndex[2] = 4;
	btSlotQueueIndex[3] = 5;
	btSlotQueueIndex[5] = 6;
	btSlotQueueIndex[8] = 9;
	btSlotQueueIndex[10] = 8;
	btStateHooks[9].enter = (void (*)(void))btLmpLcResolveTable;
	btStateHooks[6].enter = (void (*)(void))btLmpLcResolveTable;
	btStateHooks[9].due = (void (*)(struct BtSlotEvent *, uint32_t))btLmpLcQueryTable;
	btStateHooks[6].due = (void (*)(struct BtSlotEvent *, uint32_t))btLmpLcQueryTable;
	__aeabi_memclr4(btRateHistory, sizeof(btRateHistory));
	__builtin_memset(btTableLinkModes, 0, sizeof(btTableLinkModes));
	btTableEnabled = 1;
	btStateHooks[5].enter = (void (*)(void))btLmpLcScheduleTable;
	btStateHooks[5].due = (void (*)(struct BtSlotEvent *, uint32_t))btLmpLcNotifyTable;
	btStateHooks[11].due = btLmpLcClearBuffer;
	btStateHooks[7].enter = (void (*)(void))btLmpLcConvertBuffer;
	btStateHooks[7].due = btLmpLcClearBuffer;
	btStateHooks[10].due = btLmpLcReleaseBuffer;
	btTableModeBuffer = 0;
	btLmpLcManageEntry();
	btTableModeSlot = 0;
	btTableModeEntry = 0xff;
	btLtsRomClearState();
}

/*
 * The ROM's link context handler, wrapped: the first time a link's record reaches state 3 or 4
 * this is noted, and a record in state 4 is detached (reason 0x13) and set to state 14.
 */
void btLmpLcProcessPacket(unsigned int link, uint32_t argument)
{
	uint8_t *record = btLcoRomDispatchContext(link);

	if (record[2] == link && btLinkReleaseSeen == 0 && btLinkReleaseHeld == 0) {
		uint8_t *state = &record[32];

		if (state[0] == 3) {
			btLinkReleaseSeen = 1;
			return;
		}
		if (state[0] == 4) {
			btLinkReleaseSeen = 1;
			if (record[1] == 1)
				btLscRomResolveState(0xc18, *(BtHalf *)(record + 6));
			if (record[2] == 0)
				btLmpLcVerifyHeader(*(BtHalf *)(record + 6), 1, 0x13);
			btLdrRomQueryState(*(BtHalf *)(record + 6), (int8_t)record[20], 0x13, 0x5dc0, 0);
			btLscRomApplyState(0x94, 0xbf9, *(BtHalf *)(record + 6), record[20]);
			state[1] = 2;
			state[0] = 14;
			return;
		}
	}
	btLcoRomValidateContext(link, argument);
}

/* The ROM's context update runs until a link record reached state 3 or 4. */
void btLmpLcUpdatePacket(void)
{
	if (btLinkReleaseSeen == 0)
		btLcoRomUpdateContext();
}

/*
 * The image's part of two link commands: 0xa4 (its address is a link's already: the link's record
 * kind is cleared) and 0x212 (for the matched peer's address, the link context handler of link 1).
 * Returns 1 when the command was handled here.
 */
int btLmpLcConfigurePacket(uint32_t *command)
{
	const uint8_t *address = (const uint8_t *)(command + 1);
	uint16_t link;

	if (command[0] == 0xa4) {
		btLcoRomBuildContext(command);
		if (btLinkAddressHook(address, &link) == 0 && btLcoRomConfigureContext(address, &link) != 0)
			btHccRomApplyState()[120 * link + 1] = 0;
		return 1;
	}
	if (command[0] == 0x212 && btLinkMatchEnabled == 1 && (btLinkMatchFirst == 1 || btLinkMatchSecond == 1) &&
	    memcmp(address, btLinkMatchPeer + 4, 6) == 0) {
		btLmpLcProcessPacket(1, (uint32_t)(uintptr_t)address);
		return 1;
	}
	return 0;
}

/*
 * Sets a link's mode 7 slots up: the slots it may repeat follow from its interval. Called in an
 * interrupt or the timer thread, it only takes the next slot clock of the second slot kind and
 * posts message 0xccf to the link controller.
 */
void btLmpLcCheckPacket(unsigned int link, uint32_t clock, unsigned int kind, uint32_t argument,
			unsigned int interval, uint32_t extra)
{
	struct BtDeviceProfile *profile;

	if (_tx_thread_system_state != 0 || _tx_thread_current_ptr == &_tx_timer_thread) {
		unsigned int period = btDeviceContext.timings[1].buffer;
		uint32_t base;
		uint32_t *message;

		if (period > 0x200)
			period = 0x200;
		else if (period < 0x40)
			period = 0x40;
		base = btLtsRomParseState(0);
		btDeviceContext.queue = base + btLinkClockHook() % period;
		message = osa_MsgAlloc(4);
		*message = 0xccf;
		osa_MsgSend((void **)&message, romQueueSlot);
		return;
	}
	profile = &btDeviceProfile[link];
	profile->repeatLimit = (int)(interval - 9) / 9 + interval / 9;
	profile->repeats = 0;
	if (kind == 2 && (btDeviceContext.entry & 1) && btDeviceContext.timings[1].frame == 1)
		clock += 9;
	btLtsRomCheckContext(link, clock, kind, argument, interval, extra);
	profile->options |= 0x80;
}

/*
 * The ROM's check of a link's request, wrapped: the image's hook runs first; for a link record of
 * kind 0 the ROM decides, otherwise the hook's answer is passed on and the request is accepted.
 */
int btLmpLcValidatePacket(unsigned int link, uint32_t first, uint32_t second, uint8_t *out)
{
	uint8_t *links = btHccRomApplyState();
	int result = 1;
	uint8_t answer;

	btLinkValidateHook(link, first, second, &answer);
	if (links[120 * link + 20] == 0)
		result = btHccRomCheckState(link, first, second, out);
	else if (out != NULL)
		*out = answer;
	return result;
}

/* The ROM's configuration of a value, wrapped: a non-zero value first reports event 0x32 once. */
void btLmpLcPreparePacket(unsigned int value)
{
	if (value != 0 && btPeerReport == 1) {
		btPeerReport = 0;
		btLcoRomFormatState(0x32, 0xffff);
		return;
	}
	btHccRomConfigureState(value);
}

/*
 * The ROM's request clear, wrapped: a request with a link (byte 6 neither 0 nor 0xff) marks the
 * link controller, sets radio register 0x157 and the level of register 0xfa, keeping both; without
 * one, both are put back.
 */
void btLmpLcDispatchPacket(uint8_t *request)
{
	btLdaRomClearState(request);
	if (request[6] != 0 && request[6] != 0xff) {
		uint8_t value;

		LINK_HALF(0x6a) |= 0x80;
		rfBusAcquire();
		value = rfBusRead(0x157);
		btTableModePacket = value;
		rfBusWrite(0x157, (value & 0xe3) + 0x14);
		rfBusRelease();
		btTablePayload = btBdhRomProcessState(0xfa, 1);
		btBdhRomUpdateState(0xfa, 0xa8);
		return;
	}
	btBdhRomUpdateState(0xfa, btTablePayload);
	btTablePayload = 0;
	rfBusAcquire();
	rfBusWrite(0x157, btTableModePacket);
	rfBusRelease();
}

/*
 * The slots to give a request of count slots at clock (at most limit, or 1 where *flag is set),
 * from the target's: when the target's slots are full and their clock has run out, the ROM selects
 * them and every session starts over.
 */
unsigned int btLmpLcParsePacket(uint32_t clock, unsigned int count, unsigned int limit, uint8_t *flag)
{
	unsigned int want = *flag != 0 ? 1 : limit;
	unsigned int used = btTargetUsed;
	int full = used + count >= btTargetSlots;
	unsigned int entry;
	unsigned int take;
	unsigned int i;

	if (used == 0)
		btTableTargetClock = btTargetEntry + clock;
	if (full) {
		int8_t left = (int8_t)(btTableTargetClock - clock + 1);

		btTargetEntry = left < 0 ? 0 : left;
		if (*flag == 0 &&
		    (int32_t)(btLtsRomScheduleState(btLtsRomParseState(0)) - btTableTargetClock) >= 0) {
			unsigned int result;

			btLinkModeControl->header = btTableModeHeader;
			result = btLtsRomSelectState(clock, count, want, flag);
			for (i = 1; i < 14; i = (uint8_t)(i + 1))
				btLinkSessions[i].active = 1;
			btTableModeBuffer = 0;
			return result;
		}
	}
	if (want >= count)
		want = count;
	if (full) {
		if (btTargetEntry <= want)
			want = btTargetEntry;
		want = want > 1 ? want : 1;
		*flag = 1;
	}
	entry = btTargetEntry;
	take = entry < want ? entry : want;
	btTargetEntry = entry - take;
	for (i = 0; i < want; i = (uint8_t)(i + 1))
		btLtsRomFormatState(clock, 1, 0, btTargetEntry);
	btLinkSessions[btTableModeHeader].active = 1;
	return want;
}

/* Posts messages 0xc05 and 0xc03 about the link handle to the link controller's queue. */
int btLmpLcBuildPacket(uint16_t handle)
{
	struct BtHandleMessage *message;

	message = osa_MsgAlloc(8);
	message->id = 0xc05;
	message->handle = handle;
	osa_MsgSend((void **)&message, romQueueLcp);
	message = osa_MsgAlloc(8);
	message->id = 0xc03;
	message->handle = handle;
	osa_MsgSend((void **)&message, romQueueLcp);
	return 1;
}

/*
 * The ROM's target context handler, wrapped: a target in state 5 whose deadline is still ahead is
 * expired first (state 5 again, mode header 0); the target's entry starts over.
 */
void btLmpLcComputePacket(void)
{
	if (romTargetTable == 5 &&
	    (int32_t)(btTargetDeadline - btLtsRomScheduleState(btLtsRomParseState(0)) - 1) > 0) {
		btTargetExpireHook();
		romTargetTable = 5;
		btTableModeHeader = 0;
	}
	btLtsRomHandleContext();
	btTargetEntry = 0;
}

/*
 * The slots a link at the rate needs, wrapped: for a link record in state 3 (substate 1 or 2: with
 * the host slot's timing, otherwise 20 and 20 ticks) the image counts them, 0 for 10 ticks or less;
 * otherwise the ROM does.
 */
unsigned int btLmpLcQueryPacket(unsigned int link, unsigned int rate)
{
	const uint8_t *record = btLdrRomReleaseState() + 116 * link + 64;
	unsigned int span;
	unsigned int extra;
	unsigned int hold;
	unsigned int ticks;

	if (record[4] != 3)
		return btLdrRomTrackState(link, rate);
	if (record[5] == 1 || record[5] == 2) {
		span = (uint16_t)(btLinkRateBase + romHctHostSlot[link][12]);
		hold = romHctHostSlot[link][13];
		extra = btLinkRateExtra;
	} else {
		span = 40;
		extra = 0;
		hold = 0;
	}
	ticks = ((rate * 5) >> 2) * span / 1000 + extra + hold;
	if (ticks <= 10)
		return 0;
	return (uint16_t)(ticks * 2 / 625 + 1);
}

/* The ROM's link sync, except for kind 5 on a link record of kind 14. */
void btLmpLcResetPacket(unsigned int link, unsigned int kind)
{
	if (btHccRomApplyState()[120 * link + 21] == 14 && kind == 5)
		return;
	btLdrRomSyncState(link, kind);
}

/* With a link record, bit 2 of the scan slot's flags at 0x22 is set. */
__attribute__((noinline)) void btLmpLcSyncPacket(void)
{
	if (btLinkIndexRecord != 0) {
		osa_LockAcquire();
		*(BtHalf *)(btLinkScanSlot + 0x22) |= 4;
		osa_LockRelease();
	}
}

/* The ROM's release, then the scan flag, and every host bitmap bit set. */
void btLmpLcApplyPacket(void)
{
	btLdaRomReleaseState();
	btLmpLcSyncPacket();
	btHostBitmap = 0xffff;
}

/* The ROM's acquire in the mode, once the scan slot's bits 8-9 asked for it (they and bits 2-3 are cleared). */
__attribute__((noinline)) void btLmpLcResolvePacket(unsigned int mode)
{
	BtHalf *flags;

	osa_LockAcquire();
	flags = (BtHalf *)(btLinkScanSlot + 0x22);
	if (((*flags >> 8) & 3) == 0) {
		osa_LockRelease();
		return;
	}
	*flags &= ~0x30c;
	osa_LockRelease();
	btLdaRomAcquireState(mode);
}

/*
 * The ROM's request tracking, wrapped: in rate state 3 a request for the host link's handle
 * acquires in mode 0 instead (state 2); in state 1, a request of kind 1 while the host bitmap is
 * empty posts message 0xc44 with the request's value first.
 */
void btLmpLcSchedulePacket(uint8_t *request)
{
	unsigned int handle = *(BtHalf *)(request + 4);

	if (btHostRate == 3) {
		btHostRate = 2;
		if (btHostBitmap == handle)
			btLmpLcResolvePacket(0);
		return;
	}
	if (btHostRate == 1 && btHostBitmap != handle && btHostBitmap == 0xffff && request[6] == 1) {
		struct BtHandleMessage *message = osa_MsgAlloc(8);

		message->id = 0xc44;
		message->handle = handle;
		message->value = request[7];
		osa_MsgSend((void **)&message, romQueueLcp);
	}
	btLdaRomTrackState(request);
	btLmpLcSyncPacket();
}

void btLmpLcNotifyPacket(void)
{
	btLmpLcResolvePacket(1);
	btLmpLcSyncPacket();
}

void btLmpLcFormatPacket(void)
{
	btLmpLcResolvePacket(0);
	btLmpLcSyncPacket();
}

/*
 * Whether the request's handle, while pending, may go on: its ROM state is 7 or 11, or it is 0 and
 * the handle is no longer busy (then it is no longer pending either).
 */
int btLmpLcInspectPacket(const uint8_t *request)
{
	unsigned int handle = *(const BtHalf *)(request + 4);
	unsigned int state;

	if (btHandlePending[handle] == 0)
		return 0;
	state = btHccRomBuildState(handle);
	if (state == 11 || state == 7)
		return 1;
	if (state != 0 || btTablePeerBusy[handle] != 0)
		return 0;
	btHandlePending[handle] = 0;
	return 1;
}

/*
 * The sessions free in the mode control's ring of the target's sessions; with the mode buffer
 * set, a full ring counts as all but one free.
 */
unsigned int btLmpLcTrackPacket(void)
{
	struct BtModeControl *control = btLinkModeControl;
	unsigned int sessions = btTargetSessions;
	int8_t used;
	unsigned int free;

	control->sessions = sessions;
	used = (int8_t)(btTableModeHeader - control->current);
	if (used < 0)
		used = (int8_t)(used + sessions);
	free = (uint8_t)(sessions - used - 1);
	if (btTableModeBuffer != 0 && free == 0)
		free = (uint8_t)(btTargetSessions - 1);
	return free;
}

/*
 * Hands out the free target sessions' slots from the device record's clock: each round walks the
 * 13 slot queues, giving the first queue whose link fits its slots there, else filling the gap up
 * to the nearest link. Returns the clock it got to. The ROM's argument is where the fill hook says
 * to stop (the original keeps it in the argument's stack slot, so its low byte is the start value).
 */
uint32_t btLmpLcManagePacket(unsigned int argument)
{
	uint32_t clock;
	uint16_t remaining;
	uint8_t stop = (uint8_t)argument;

	btLinkModeControl->header = btTableModeHeader;
	clock = btDeviceRecord;
	romTargetTable = 0;
	remaining = btLmpLcTrackPacket();
	while ((int16_t)remaining > 0) {
		struct BtDeviceProfile *chosen = NULL;
		unsigned int best = 0xffff;
		unsigned int taken = 0;
		unsigned int queue = 0;

		do {
			struct BtSlotQueue *slots = &btSlotQueues[queue];
			struct BtDeviceProfile *profile = slots->head;
			uint8_t before;
			uint8_t setup;
			uint8_t after;
			uint8_t hold;
			uint32_t start;
			unsigned int need;

			while (profile != NULL && btLtsRomAdjustState(queue) == 0)
				;
			if (profile == NULL)
				goto next;
			profile->guard = btLdrRomResolveState(profile->index);
			start = profile->due - profile->guard;
			btSlotNeedHook(profile, chosen, &before, &setup, &after, &hold);
			need = (uint8_t)(before + setup + after + hold);
			if (btTableModeEntry != 0xff) {
				if ((int32_t)(clock - btTableModeSlot) >= 0) {
					btTableModeEntry = 0xff;
				} else if (queue > btTableModeEntry &&
					   btLmpLcInspectEntry(btTableModeMask, profile->index) == 0) {
					unsigned int wait = (uint16_t)(btTableModeSlot - clock);

					if (wait <= best)
						best = wait;
				}
			}
			if ((int32_t)(clock - start + before + setup) < 0 || best < need) {
				int16_t gap = (int16_t)(start - clock);

				if (gap > 0 && (int)best > gap) {
					best = (uint16_t)gap;
					chosen = profile;
				}
			} else if (((1u << profile->state) & btTableShortStates) &&
				   (int32_t)(clock + before + setup - (profile->window + start) + 1) > 0) {
				unsigned int lead = before + setup;

				if (lead <= best)
					best = lead;
				best = best >= 1 ? best : 1;
			} else {
				unsigned int lead;

				if (need > remaining)
					return clock;
				lead = (uint8_t)(before + setup);
				remaining -= lead;
				clock += lead;
				best = (uint16_t)(best - lead);
				while (lead-- != 0)
					btLtsRomFormatState(clock, 1, btTargetState, 0);
				taken = slots->take(slots, profile, clock, (uint16_t)(best - hold));
				if (profile->adjust >= 2 || profile->tracked != 0) {
					btTargetIndex = profile->index;
					btTargetMode = profile->state;
					btTargetStart = clock;
					btTargetWindow = best - after - hold;
					romTargetTable = 1;
				}
			}
next:
			queue = (uint8_t)(queue + 1);
		} while (taken == 0 && queue < 13);

		btLmpLcConfigureTable(&remaining, taken, best);
		if (taken == 0) {
			btTargetEnterHook(4);
			taken = btSlotFillHook(clock, best, remaining, &stop);
			btTargetLeaveHook(4);
		}
		clock += taken;
		btSlotDoneHook(clock);
		if (btDeviceWaitHook != NULL && btDeviceWaitHook(clock) != 0)
			btDeviceWaitHook = NULL;
		if ((romTargetTable | stop) != 0)
			return clock;
		remaining -= taken;
	}
	return clock;
}

/*
 * Opens the next target session (the ring wraps at its size) for the profile, or an empty one; the
 * device record's clock moves on and the session and profile become the target's.
 */
int btLmpLcAcquirePacket(uint32_t unused, uint16_t flags, struct BtDeviceProfile *profile, uint8_t entry)
{
	int restarted = 0;
	uint32_t now;
	uint32_t later;
	struct BtLinkSession *session;
	unsigned int header;

	btTargetEnterHook(2);
	now = btLtsRomParseState(0);
	if (btLinkModeControl->current == btTableModeHeader) {
		btLtsRomPrepareState();
		restarted = 1;
	}
	header = btTableModeHeader;
	session = &btLinkSessions[header];
	header = (uint8_t)(header + 1);
	btTableModeHeader = header;
	if (btLinkModeControl->sessions <= header)
		btTableModeHeader = 0;
	if (profile != NULL) {
		btLtsRomSyncState(profile, session);
		if ((profile->state == 9 || profile->state == 5) && btLcoRomTrackContext(profile->index) != 0)
			session->flags |= btSlotSessionFlags;
		session->active |= flags;
		btTargetContext = profile->active;
		btTargetUsed = 0;
	} else {
		session->active = flags;
		btTargetContext = 0;
		btTargetUsed = btTargetUsed + 1;
	}
	later = btLtsRomParseState(0);
	if ((btTableModeBuffer != 0 && later != btTableModeClock) || (restarted && later != now))
		btTableModeHeader = btLinkModeControl->current;
	btTableModeBuffer = 0;
	if (restarted)
		btLtsRomDispatchState();
	btDeviceRecord = btDeviceRecord + 1;
	btTargetSession = session;
	btTargetState = (uint32_t)(uintptr_t)profile;
	btTargetEntry = entry;
	btTargetLeaveHook(2);
	return 1;
}
