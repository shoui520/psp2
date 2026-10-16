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

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

#define WLAN_TX_RING_COUNT	14
#define WLAN_TX_RING_SHADOWED	7	/* rings below this fill before publishing */

#define MAC_TX_RING_ENTRY(ring, index) \
	REG32(SOC_MAC_REGISTER_BASE | ((unsigned int)(ring) << 8) | (uint8_t)(index))

/* Transmit request the MAC reads (76 bytes); only the fields the ring code touches are named. */
struct WlanMacTxRequest {
	uint8_t reserved0[4];
	uint8_t frame[4];	/* unaligned pointer to the frame (length first) */
	uint8_t reserved8[20];
	uint8_t flags[4];	/* unaligned word; bit 13 marks a frame sent without publishing */
	uint8_t type;		/* bits 0-2 */
	uint8_t reserved33;
	uint16_t length;
	uint8_t reserved36[32];
	uint8_t buffer[4];	/* unaligned pointer to the buffer data (28 bytes past the buffer) */
	uint8_t reserved72[4];
};

_Static_assert(sizeof(struct WlanMacTxRequest) == 76, "transmit request");

extern volatile uint32_t *wlanTxRingReadIndex[WLAN_TX_RING_COUNT];
extern volatile uint32_t *wlanTxRingWriteIndex[WLAN_TX_RING_COUNT];
extern uint8_t wlanTxRingPending[28];
extern uint8_t wlanTxRingPublished[28];
extern uint8_t wlanTxRingFilled[28];
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
int wlanTxSubmit(OsaBuffer *buffer);
unsigned int wlanTxInFlightIndex(OsaBuffer *buffer);
int wlanTxInFlightAdd(OsaBuffer *buffer);
int wlanTxInFlightRemove(OsaBuffer *buffer);
int wlanBufferIsPooled(const OsaBuffer *buffer);

#endif /* ROBIN_TXRING_H */
