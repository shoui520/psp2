/*
 * Transmit rings of the MAC. Each of the 14 rings is a page of 64 four-byte
 * entries in the MAC register file (MAC_REG(ring << 8)); an entry points to
 * a transmit request. Two pointer registers belong to a ring: the hardware
 * read index (A) and the write index (B), both counting bytes with a wrap
 * bit; the upper half of B also carries the length of the last frame.
 * For the first rings, entries are first filled in without telling the
 * hardware and published later: Filled is the next free entry, Published the
 * first one not yet handed over and Pending the number between them.
 */
#ifndef ROBIN_TXRING_H
#define ROBIN_TXRING_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/rom.h"

#define WLAN_TX_RING_COUNT	14
#define WLAN_TX_RING_SHADOWED	7	/* rings below this fill before publishing */

#define MAC_TX_RING_ENTRY(ring, index) \
	REG32(SOC_MAC_REGISTER_BASE | ((unsigned int)(ring) << 8) | (uint8_t)(index))

/* Transmit request the MAC reads (76 bytes); only the fields the ring code touches are named. */
struct WlanMacTxRequest {
	uint8_t mode;		/* 1: put back at the head of its ring for a retry; 10: Block Ack */
	uint8_t reserved1[3];
	uint8_t frame[4];	/* unaligned pointer to the frame (length first) */
	uint16_t hwStatus;	/* hwStatus..hwFlags: cleared before a data frame goes out */
	uint8_t hwRetries;
	uint8_t hwFlags;
	union {
		uint16_t sequence[2];	/* copied into the data at 16 */
		struct {		/* a beacon or probe response template */
			uint8_t cfOffset;	/* where the elements from the CF Parameter Set on start */
			uint8_t timOffset;	/* where the TIM element starts */
			uint8_t templateLength[2];
		};
	};
	uint8_t reserved16[12];
	uint8_t flags[4];	/* unaligned word; bit 13 marks a frame sent without publishing */
	uint8_t type;		/* bits 0-2 */
	uint8_t info;		/* byte 1 of the data */
	uint16_t length;
	uint8_t reserved36[4];
	uint16_t rate;		/* bits 3-9: rate index */
	uint8_t reserved42[2];
	uint8_t rateInfo[4];	/* unaligned word the radio calibration derives from the rate (0x2bc6c) */
	uint8_t reserved48[16];
	uint8_t host[4];	/* unaligned pointer to the host descriptor */
	uint8_t buffer[4];	/* unaligned pointer to the buffer data (28 bytes past the buffer) */
	uint8_t next[4];	/* unaligned word, cleared */
};

/* Bits of WlanMacTxRequest.flags. */
#define MAC_TX_FLAG_RTS			0x00000020u
#define MAC_TX_FLAG_CTS_TO_SELF		0x00000080u
#define MAC_TX_FLAG_NO_ACK		0x00000100u	/* group-addressed or no-ack policy */

_Static_assert(sizeof(struct WlanMacTxRequest) == 76, "transmit request");
_Static_assert(offsetof(struct WlanMacTxRequest, rate) == 40, "request rate");
_Static_assert(offsetof(struct WlanMacTxRequest, host) == 64, "request host descriptor");

/*
 * The buffer data starts with the transmit descriptor; the transmit request
 * is at 20, the pointer to the frame at 96, the ring number at 108, its
 * tag at 109 and the transmissions left at 110.
 */
#define TX_DATA_REQUEST(data)	((struct WlanMacTxRequest *)((data) + 20))
#define TX_DATA_FRAME(data)	__aeabi_uread4((data) + 96)
#define TX_DATA_RING(data)	((data)[108])
#define TX_DATA_TAG(data)	((data)[109])	/* what the transmit-done task does with it */
#define TX_DATA_RETRIES(data)	((data)[110])

/* What the transmit-done task learns of a finished frame (partial). */
struct WlanTxStatus {
	uint8_t *data;			/* buffer data of the frame */
	uint8_t reserved4[12];
	uint32_t result;		/* 0: acknowledged */
	uint8_t rate;			/* rate index the frame was first sent at */
	uint8_t finalRate;		/* rate index of the last transmission */
	uint8_t reserved22;
	uint8_t retryBlocked;		/* the frame must not be sent again */
	uint8_t reserved24;
	uint8_t retries;		/* transmissions after the first */
};

_Static_assert(offsetof(struct WlanTxStatus, result) == 16, "transmit result");
_Static_assert(offsetof(struct WlanTxStatus, retryBlocked) == 23, "retry blocked");
_Static_assert(offsetof(struct WlanTxStatus, retries) == 25, "retries");

extern struct OsaBufferPool wlanBlockPool7;
extern struct OsaBufferPool wlanBlockPool28;

extern volatile uint32_t *wlanTxRingReadIndex[WLAN_TX_RING_COUNT];
extern volatile uint32_t *wlanTxRingWriteIndex[WLAN_TX_RING_COUNT];
extern uint32_t wlanTxRingPending[WLAN_TX_RING_SHADOWED];
extern uint32_t wlanTxRingPublished[WLAN_TX_RING_SHADOWED];
extern uint32_t wlanTxRingFilled[WLAN_TX_RING_SHADOWED];
extern uint32_t wlanTxRingPublishLimit;
extern uint32_t wlanTxRingMaskFill;
extern uint32_t wlanTxRingMaskPush;
extern uint32_t wlanTxPowerSaveCount;
extern uintptr_t wlanWindowOwners[20];

int wlanFrameSetPowerSave(const struct WlanMacTxRequest *request, int powerSave);
int wlanTxRingFill(struct WlanMacTxRequest *request, unsigned int ring);
int wlanTxRingPublish(unsigned int ring);
int wlanTxRingPush(const struct WlanMacTxRequest *request, unsigned int ring, volatile uint32_t *readIndex,
		   volatile uint32_t *writeIndex, uint32_t *mask);
uint32_t wlanTxRingPushPublished(const struct WlanMacTxRequest *request, unsigned int ring,
				 volatile uint32_t *writeIndex);
void wlanTxRingFlush(unsigned int ring, uintptr_t owner);
void wlanTxRingMoveFilled(unsigned int ring, unsigned int target);
void wlanTxRingRequeueFilled(unsigned int ring, uintptr_t owner);
void wlanTxRingRouteFilled(unsigned int ring, unsigned int other, unsigned int type, const void *address);
void wlanTxRingRetarget(struct WlanMacTxRequest *request, unsigned int ring);
void wlanTxRingComplete(struct WlanMacTxRequest *request);
int wlanTxCheckRing(struct WlanConnection *connection, struct WlanMacTxRequest *request, unsigned int ring);
int wlanTxResubmit(OsaBuffer *buffer);

/* Not converted yet; provisional names. */
struct OsaBufferPool;
OsaBuffer *wlanTxBufferAlloc(struct OsaBufferPool *pool, uint32_t size, uint32_t flags,
			     unsigned int queue);				/* 0x188b2 */
int wlanTxHoldFrame(OsaBuffer *buffer, int reason);		/* 0x18924 */
int wlanTxSubmit(OsaBuffer *buffer);
unsigned int wlanTxInFlightIndex(OsaBuffer *buffer);
int wlanTxInFlightAdd(OsaBuffer *buffer);
int wlanTxInFlightRemove(OsaBuffer *buffer);
int wlanBufferIsPooled(const OsaBuffer *buffer);

#endif /* ROBIN_TXRING_H */
