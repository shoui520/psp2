/*
 * The link controller's host link buffers (0x44ca6 on): the timing of the host link when the
 * radio's rate changes, and the link's teardown back to the ROM. The names of the firmware's
 * placeholders are kept; see robin/btbuffer.h.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
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
