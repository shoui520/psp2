/*
 * The MAC's Block Ack hardware: the slots that hold a session's window, the
 * rings of frames a transmit slot has in flight and the callbacks of the
 * transmit rings (wlan/baslot.c). The names are inferred from use.
 */
#ifndef ROBIN_BASLOT_H
#define ROBIN_BASLOT_H

#include <stdint.h>
#include "robin/mac.h"
#include "robin/stream.h"
#include "robin/txring.h"

/* A transmit slot's registers; the slots are 256 bytes apart from MAC register 0xa40. */
struct MacBaSlot {
	uint32_t windowSize;		/* the window's frames, less one */
	uint32_t timeout;		/* the Block Ack timeout, less one */
	uint32_t buffer;		/* where the slot keeps its scoreboard */
	uint32_t reserved12;
	uint32_t parameter;
	uint32_t reserved20[3];
	uint32_t sequence;		/* the window's starting sequence number */
	uint32_t sequenceInfo;		/* the sequence number less one, in bits 4-15 */
	uint32_t reserved40[2];
	uint32_t ringRead;		/* the entry of the ring of frames sent and not yet reported */
	uint32_t ringWrite;
	uint32_t statusRead;
	uint32_t statusWrite;
	uint8_t reserved64[192];
};

_Static_assert(sizeof(struct MacBaSlot) == 256, "Block Ack slot");

#define MAC_BA_SLOT(slot)	((volatile struct MacBaSlot *)(SOC_MAC_REGISTER_BASE + 0xa40 + 0x100 * (slot)))
#define MAC_BA_RX_PARAMETER(slot) MAC_REG(0xa10 + 0x100 * (slot))
#define MAC_AC0_SLOT_BITS	0x11000001u	/* what a transmit slot sets in MAC_AC0_BA_ACTIVE */
#define MAC_ACD_RX_SLOT_BITS	0x100000u	/* an extra bit a receive slot sets */

/* The transmit rings' parameters and callbacks, filled by wlanTxRingOpsInit(). */
struct WlanTxRingOps {
	uint32_t *statusBuffer;		/* where the Block Ack hardware reports */
	uint32_t slotParameter;		/* written to every transmit slot */
	uint32_t windowParameter;	/* written to the Block Ack windows' control */
	void (*retarget)(struct WlanMacTxRequest *request, unsigned int ring);
	void (*complete)(struct WlanMacTxRequest *request);
	uint32_t (*bufferAddress)(void);
};

extern int (*wlanRateParameterHook)(unsigned int rate);	/* 0x54e90 */
extern struct WlanTxRingOps wlanTxRingOps;			/* 0x54e1c */

void wlanTxRingOpsInit(void);					/* 0x310a0 */
uint32_t wlanTxRingBufferAddress(void);				/* 0x3109c */

int wlanBaHwReset(void);					/* 0x28f30 */
void wlanRateParametersClear(void);				/* 0x29060 */
void wlanBaEntriesInit(void);					/* 0x29114 */
void wlanBaEntryRecord(unsigned int tid, const uint8_t *peer, uint16_t sequence, int mode,
		       unsigned int slot);				/* 0x29180 */
int wlanBaSlotSetup(const struct WlanBaRequest *request, unsigned int window, unsigned int slot);
									/* 0x29264 */
void wlanBaSlotDetach(unsigned int ring, unsigned int slot);	/* 0x2931c */
void wlanTxRingRetargetAll(unsigned int ring, unsigned int target, uint32_t *readIndex,
			   volatile uint32_t *writeIndex);	/* 0x29378 */
void wlanTxRingRetargetMatching(unsigned int target, unsigned int ring, unsigned int type,
				const uint8_t *address, uint32_t *readIndex,
				volatile uint32_t *writeIndex);	/* 0x293ae */
void wlanBaSlotClear(unsigned int slot);			/* 0x2940e */
int wlanRxBaHwSetup(const struct WlanBaRequest *request, uint8_t *slot);	/* 0x294cc */
int wlanRxBaHwUpdate(const struct WlanBaRequest *request, unsigned int slot);	/* 0x29572 */
void wlanBaWindowsStart(void);					/* 0x295b6 */
int wlanRxBaHwRelease(const uint8_t *peer, unsigned int tid);	/* 0x296f6 */

#endif /* ROBIN_BASLOT_H */
