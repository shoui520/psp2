/* DMA descriptor rings of the MAC: ring contexts kept in SQRAM. */
#ifndef ROBIN_DMARING_H
#define ROBIN_DMARING_H

#include <stdint.h>
#include "robin/wlanrx.h"

/* One descriptor: a control word and a buffer pointer. */
struct WlanDmaDescriptor {
	uint32_t control;	/* bit 31: armed, bit 20: completed by the hardware */
	uint32_t buffer;
};

#define WLAN_DMA_DESCRIPTOR_ARMED	0x80000000u
#define WLAN_DMA_DESCRIPTOR_DONE	0x00100000u

/*
 * Software state of one ring (36 bytes). The pointer mirrors the hardware
 * pointer: the descriptor index in bits 3-11 and a phase bit 12 that flips
 * each time the ring wraps.
 */
struct WlanDmaRing {
	uint32_t reserved0[2];
	uint32_t controlNext;		/* control word armed into every descriptor but the last */
	uint32_t controlLast;		/* control word armed into the last descriptor */
	uint32_t size;			/* descriptors in the ring */
	struct WlanDmaDescriptor *descriptors;
	uint32_t index;			/* index of the oldest descriptor not yet reclaimed */
	uint32_t pointer;
	volatile uint32_t hardwarePointer;	/* copy of the hardware pointer, read from the MAC */
};

_Static_assert(sizeof(struct WlanDmaRing) == 36, "DMA ring");

#define WLAN_DMA_RING_COUNT	12

/* Receive rings: the data ring and three more that carry firmware and event frames. */
enum {
	WLAN_DMA_RING_RX_DATA = 3,
	WLAN_DMA_RING_RX_A = 4,
	WLAN_DMA_RING_RX_B = 5,
	WLAN_DMA_RING_RX_C = 6,
};

/* Marks a frame on ring B as ready, and as consumed once it has been read. */
#define WLAN_DMA_FRAME_READY	0xbeef
#define WLAN_DMA_FRAME_USED	0xabba

extern struct WlanDmaRing wlanDmaRings[WLAN_DMA_RING_COUNT];
extern uint8_t wlanDmaRingPending[128];

uint32_t wlanDmaRingReadBuffer(unsigned int ring);
uint32_t wlanDmaRingRearm(unsigned int ring);
int wlanDmaRingHasPending(unsigned int ring);
void wlanDmaRingClearPending(void);
void wlanDmaRingMarkPending(uint32_t from, uint32_t to, unsigned int ring);
uint32_t wlanDmaRingAdvance(unsigned int ring, uint32_t unused, uint32_t count);
uint32_t wlanDmaRingReclaim(unsigned int ring);
int wlanDmaRing3HasRoom(void);
uint32_t wlanDmaRingFindCompleted(unsigned int ring, uint16_t *pointer);
void wlanRxCopyFrame(const uint8_t *raw, struct WlanRxStatus *status);
int wlanDmaRingsHaveRoom(void);
int wlanDmaRingPeekReady(uint8_t **raw, uint8_t **header, uint8_t **body, uint16_t *bodyLength);
int wlanDmaRingTakeB(struct WlanRxStatus *status);
int wlanDmaRingTakeA(struct WlanRxStatus *status);
int wlanDmaRingTakeC(struct WlanRxStatus *status);

#endif /* ROBIN_DMARING_H */
