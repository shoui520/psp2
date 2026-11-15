/*
 * The frames an IBSS holds back for peers in power save (wlan/txhold.c, 0x3b7d6..0x3c1ec): while
 * the ATIM window is open a frame for a peer that sleeps waits in a list, and an ATIM frame for
 * the peer is sent; the held frames follow once the peer is awake. The firmware's placeholder
 * names (macMgmt*Header, macMgmt*Descriptor, wlanPs*Table, wlanPs*Buffer) are kept; the file
 * name, the structures and their fields are inferred.
 */
#ifndef ROBIN_TXHOLD_H
#define ROBIN_TXHOLD_H

#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/wlan.h"

#define WLAN_TX_HOLD_ITEMS	28	/* the frames that can be held in all */
#define WLAN_TX_HOLD_PEERS	6	/* the peers that are announced in one window */

/* What is done with a held frame when it is taken off the list (WlanTxHoldItem.mode). */
enum {
	WLAN_TX_HOLD_RELEASE = 0,	/* wlanTxReleaseFrame(): the frame is dropped */
	WLAN_TX_HOLD_RESUBMIT = 1,	/* wlanTxResubmit(): it is sent */
	WLAN_TX_HOLD_FREE = 2,		/* the item is free */
};

struct WlanTxHoldItem {
	uint8_t mode;
	uint8_t reserved1[3];
	uint32_t group;		/* 1: the frame is for a group address; such frames come first in a list */
	OsaBuffer *frame;
	struct WlanTxHoldItem *next;
};

_Static_assert(sizeof(struct WlanTxHoldItem) == 16, "held frame");

struct WlanTxHoldList {
	struct WlanTxHoldItem *head;
	struct WlanTxHoldItem *tail;
	uint8_t groupCount;	/* the items with group set */
	uint8_t unicastCount;
	uint8_t reserved10[2];
};

_Static_assert(sizeof(struct WlanTxHoldList) == 12, "held frames' list");

/* The machine of the held frames (0x595f0): its states are below the root, build. */
struct WlanTxHoldMachine {
	struct RomHsmState *state;
	struct RomHsmState build;	/* root: starts the machine in compute */
	struct RomHsmState compute;	/* below build */
	struct RomHsmState query;	/* below build; the others are below it */
	struct RomHsmState reset;
	struct RomHsmState sync;
	struct RomHsmState apply;
};

_Static_assert(sizeof(struct WlanTxHoldMachine) == 76, "held frames' machine");

/* What wlan/txhold.c keeps from 0x59384 (the peers' table, the flags of the window). */
struct WlanTxHold {
	uint32_t held;			/* 1: a peer is being announced */
	uint32_t drainAll;		/* 1: the list is sent whoever the peer is */
	uint32_t psActive;		/* wlanPsConvertTable(): power save is on */
	uint32_t waiting;		/* 1: the list was filled by macMgmtVerifyHeader() */
	uint32_t active;		/* 1: the query state saw event 6 */
	uint8_t peers[WLAN_TX_HOLD_PEERS][6];	/* the peers announced */
	uint32_t peerCount;
	uint32_t flushPending;		/* 1: the held frames wait for the next window */
	uint8_t groupBit;		/* bit 4 of the descriptor's byte 3 (macMgmtCheckDescriptor()) */
	uint8_t reserved5[1];
	uint16_t interval;		/* the ATIM window (macMgmtPrepareDescriptor(), IBSS only) */
	uint32_t sending;		/* 1: an ATIM frame is being sent */
	uint8_t reserved12[4];
	uint8_t phase;			/* 0: nothing held, 1: unicast frames held, 2: group frames held too */
	uint8_t windowCount;		/* calls of macMgmtClearHeader() since the window opened */
	uint8_t windowDivisor;		/* 255: no window; set by wlanPsHandleBuffer() */
	uint8_t reserved15[1];
};

_Static_assert(sizeof(struct WlanTxHold) == 80, "held frames' state");

/* The ATIM frame: a static buffer (SQRAM 0xc0032294) whose data and frame follow it. */
struct WlanAtimBuffer {
	OsaBuffer buffer;
	union {
		uint8_t data[228];
		struct {
			uint8_t reserved0[96];
			WlanWirelessFrame *frameAddress;	/* TX_DATA_FRAME() */
		};
	};
	WlanWirelessFrame frame;	/* 0xc0032394 */
};

extern struct WlanAtimBuffer wlanAtimBuffer;
extern uint8_t *wlanAtimData;			/* SQRAM 0xc0011d74: wlanAtimBuffer.data */
extern uint32_t wlanIbssPsActive;		/* 0x593d4 */

extern struct WlanTxHoldMachine wlanTxHoldMachine;	/* 0x595f0 */

int macMgmtAcquireHeader(struct WlanTxHoldList *list, struct WlanTxHoldList *other);	/* 0x3b7d6 */
int macMgmtReleaseHeader(struct WlanTxHoldItem *item);				/* 0x3b81a */
int macMgmtSetupHeader(struct WlanTxHoldList *list, OsaBuffer *frame, uint8_t mode);	/* 0x3b85c */
int macMgmtClearHeader(void);							/* 0x3b8a2 */
int macMgmtConvertHeader(void);							/* 0x3b8fc */
int macMgmtVerifyHeader(void);							/* 0x3b904 */
int macMgmtSelectHeader(const void *address);					/* 0x3b90c */
void macMgmtAdjustHeader(void);							/* 0x3b93a */
void macMgmtRecordHeader(void);							/* 0x3b944 */
void macMgmtFilterHeader(void);							/* 0x3b99e */
int macMgmtHandleDescriptor(OsaBuffer *frame);					/* 0x3b9b2 */
void macMgmtProcessDescriptor(void);						/* 0x3ba74 */
int macMgmtUpdateDescriptor(void);						/* 0x3bae4 */
int wlanIbssTxFrame(OsaBuffer *buffer, int transmit);				/* 0x3baec */
void macMgmtCheckDescriptor(void);						/* 0x3bc28 */
void macMgmtValidateDescriptor(uint8_t enabled);				/* 0x3bc52 */
void macMgmtPrepareDescriptor(uint16_t interval);				/* 0x3bc5a */
unsigned int macMgmtDispatchDescriptor(void);					/* 0x3bc6e */
int macMgmtParseDescriptor(void);						/* 0x3bc80 */
void macMgmtResolveDescriptor(void);						/* 0x3bfa0 */

int wlanPsClearTable(uintptr_t context, unsigned int event);			/* 0x3c03c */
int wlanPsConvertTable(int enabled);						/* 0x3c054 */
void wlanPsVerifyTable(void);							/* 0x3c07c */
int wlanPsSelectTable(void);							/* 0x3c09c */
int wlanPsAdjustTable(uintptr_t context);					/* 0x3c0aa */
void wlanPsRecordTable(void);							/* 0x3c0ae */
int wlanPsFilterTable(uintptr_t context);					/* 0x3c108 */
void wlanPsHandleBuffer(uint8_t divisor);					/* 0x3c124 */
void wlanPsProcessBuffer(struct WlanTxHoldList *list, struct WlanTxHoldItem *item);	/* 0x3c12c */
int wlanPsUpdateBuffer(struct WlanTxHoldItem **item);				/* 0x3c18a */
int wlanPsConfigureBuffer(void);						/* 0x3c1dc */
uint32_t wlanLinkCheckPeriod(void);						/* 0x3dd46 */
unsigned int wlanPsCheckHeader(void);						/* 0x3dd4c */

/* Not written yet. */
int macTxGetContext(void);							/* 0xaa74 */
void macTxSetContext(int context);						/* 0xaa7a */
unsigned int wlanPsProcessFrame(struct WlanConnection *connection, unsigned int subtype,
				int broadcast);					/* 0x3d3f0 */
extern uint32_t wlanPsWakeCycle;		/* 0x562b4 */

#endif /* ROBIN_TXHOLD_H */
