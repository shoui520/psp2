/*
 * Transmit rings of the MAC and the table of frames in flight. See
 * robin/txring.h. The filename is inferred.
 */
#include <stddef.h>
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txring.h"
#include "robin/wlan.h"

#define RING_READ(n)	((volatile uint32_t *)(SOC_MAC_REGISTER_BASE + (n)))

/* Hardware read index (A) and write index (B) registers of each ring. */
volatile uint32_t *wlanTxRingReadIndex[WLAN_TX_RING_COUNT] __attribute__((section(".dtcm.rw"))) = {
	RING_READ(0x044), RING_READ(0x04c), RING_READ(0x054), RING_READ(0x05c),
	RING_READ(0x064), RING_READ(0x06c), RING_READ(0x074), RING_READ(0x07c),
	RING_READ(0x034), RING_READ(0x03c), RING_READ(0x01c), RING_READ(0x024),
	RING_READ(0x02c), RING_READ(0x014),
};
volatile uint32_t *wlanTxRingWriteIndex[WLAN_TX_RING_COUNT] __attribute__((section(".dtcm.rw"))) = {
	RING_READ(0x040), RING_READ(0x048), RING_READ(0x050), RING_READ(0x058),
	RING_READ(0x060), RING_READ(0x068), RING_READ(0x070), RING_READ(0x078),
	RING_READ(0x030), RING_READ(0x038), RING_READ(0x018), RING_READ(0x020),
	RING_READ(0x028), RING_READ(0x010),
};

uint8_t wlanTxRingPending[28] __attribute__((section(".sqram1.bss")));
uint8_t wlanTxRingPublished[28] __attribute__((section(".sqram1.bss")));
uint8_t wlanTxRingFilled[28] __attribute__((section(".sqram1.bss")));
/* Entries published at once. */
uint32_t wlanTxRingPublishLimit __attribute__((section(".sqram1.bss")));
/* Policy bits cleared when a ring is filled (one per ring) and set or cleared when a frame is pushed. */
uint32_t wlanTxRingMaskFill __attribute__((section(".sqram1.bss")));
uint32_t wlanTxRingMaskPush __attribute__((section(".sqram1.bss")));
/* Running total of what wlanFrameSetPowerSave() returned. */
uint32_t wlanTxPowerSaveCount __attribute__((section(".itcm.bss")));
/* Owner of each window number; 1 marks a window that publishes entries itself. */
uintptr_t wlanWindowOwners[20] __attribute__((section(".dtcm.bss")));

#define REQUEST_BUFFER(request)	((OsaBuffer *)((uintptr_t)__aeabi_uread4((request)->buffer) - 28))
#define REQUEST_FLAGS(request)	__aeabi_uread4((request)->flags)

/* Sets the power-management bit of the frame in a request; returns the bit plus one. */
int wlanFrameSetPowerSave(const struct WlanMacTxRequest *request, int powerSave)
{
	uint8_t *frame = (uint8_t *)(uintptr_t)__aeabi_uread4(request->frame) + 2;
	int value = (frame[1] & 0xef) | ((16 * powerSave) & 0x1f);

	frame[1] = value;
	return ((value << 27) >> 31) + 1;
}

/*
 * Puts a request on the ring behind the filled entries, to be published
 * later. Returns 0 when the ring is full.
 */
int wlanTxRingFill(struct WlanMacTxRequest *request, unsigned int ring)
{
	unsigned int posture = sysIrqDisable();
	uint32_t readIndex;

	wlanTxPowerSaveCount += wlanFrameSetPowerSave(request, macTxGetPowerSave());
	wlanMacSetPolicyBit(&wlanTxRingMaskFill, ring);
	readIndex = *wlanTxRingReadIndex[ring];
	if (wlanTxRingPending[ring] == 0) {
		uint32_t index = *wlanTxRingWriteIndex[ring];

		wlanTxRingFilled[ring] = index;
		wlanTxRingPublished[ring] = index;
	}
	if (wlanMacWriteChecked(readIndex, wlanTxRingFilled[ring], ring, (uint32_t)request) != 0) {
		wlanTxRingFilled[ring] += 4;
		++wlanTxRingPending[ring];
		sysIrqRestore(posture);
		return 1;
	}
	sysIrqRestore(posture);
	return 0;
}

/* Hands the filled entries (up to the limit) over to the hardware. */
int wlanTxRingPublish(unsigned int ring)
{
	unsigned int count = wlanTxRingPending[ring];
	unsigned int limit;
	volatile uint32_t *writeIndex;
	unsigned int i;

	if (count == 0)
		return 0;
	limit = wlanTxRingPublishLimit;
	if (count < limit)
		limit = count;
	wlanTxRingPending[ring] -= limit;
	writeIndex = wlanTxRingWriteIndex[ring];
	if (wlanWindowOwner(ring, wlanWindowOwners) == 1) {
		for (i = 0; i < limit; ++i) {
			unsigned int published = wlanTxRingPublished[ring];
			const struct WlanMacTxRequest *request =
				(const struct WlanMacTxRequest *)MAC_TX_RING_ENTRY(ring, published);
			uint32_t value;

			if (REQUEST_FLAGS(request) & 0x2000) {
				wlanTxRingPublished[ring] = published + 4;
				value = (published + 4) & 0x1ff;
			} else {
				unsigned int length = request->length;

				wlanTxRingPublished[ring] = published + 4;
				value = ((published + 4) & 0x1ff) | ((((4 - length) & 3) + length + 4) << 16);
			}
			*writeIndex = value;
		}
	} else {
		unsigned int published = wlanTxRingPublished[ring] + 4 * limit;

		wlanTxRingPublished[ring] = published;
		*writeIndex = published & 0x1ff;
	}
	return 1;
}

/*
 * Puts a request on the ring right away, unless the write index is too close
 * behind the read index. A set bit 7 of ring selects policy bit (ring & 7) of
 * the mask, a clear one resets it. Returns 0 when the ring is full.
 */
int wlanTxRingPush(const struct WlanMacTxRequest *request, unsigned int ring, volatile uint32_t *readIndex,
		   volatile uint32_t *writeIndex, uint32_t *mask)
{
	uint32_t read = *readIndex;
	uint32_t write = *writeIndex;
	uint32_t value;
	uint8_t from = read;
	uint8_t to = write;

	if (ring & 0x80) {
		ring &= 7;
		if (!(*mask & (1u << ring))) {
			value = *mask | (1u << ring);
			*mask = value;
			MAC_090_POLICY_MASK = value;
		}
	} else if (ring < 8 && (*mask & (1u << ring))) {
		value = *mask & ~(1u << ring);
		*mask = value;
		MAC_090_POLICY_MASK = value;
	}
	if ((from < to && from + 248u <= to) || (from > to && from <= to + 8u))
		return 0;
	MAC_TX_RING_ENTRY(ring, write) = (uint32_t)request;
	*writeIndex = (write + 4) & 0x1ff;
	return 1;
}

/* Puts a request on the ring and publishes it with the padded frame length. */
uint32_t wlanTxRingPushPublished(const struct WlanMacTxRequest *request, unsigned int ring,
				 volatile uint32_t *writeIndex)
{
	uint32_t write = *writeIndex;
	unsigned int length = request->length;
	uint32_t value;

	MAC_TX_RING_ENTRY(ring, write) = (uint32_t)request;
	value = ((write + 4) & 0x1ff) | ((((4 - length) & 3) + length + 4) << 16);
	*writeIndex = value;
	return value;
}

/* Completes the frame behind a request. */
void wlanTxRingComplete(struct WlanMacTxRequest *request)
{
	OsaBuffer *buffer = REQUEST_BUFFER(request);

	if (buffer != NULL && wlanTxInFlightRemove(buffer))
		wlanCompletePacket(buffer, 0);
}

/* Sends the frame behind a request again on the given ring. */
void wlanTxRingRetarget(struct WlanMacTxRequest *request, unsigned int ring)
{
	uint8_t *data = (uint8_t *)(uintptr_t)__aeabi_uread4(request->buffer);
	OsaBuffer *buffer;

	if (data == NULL)
		return;
	buffer = (OsaBuffer *)(data - 28);
	if (wlanTxInFlightRemove(buffer) == 0)
		return;
	data[108] = ring;
	wlanTxResubmit(buffer);
}

/*
 * Drops what the hardware has not read yet: the frames of the owner (every
 * frame when owner is 0) are completed, the others go back on the ring.
 */
void wlanTxRingFlush(unsigned int ring, uintptr_t owner)
{
	unsigned int posture = sysIrqDisable();
	uint32_t index = *wlanTxRingReadIndex[ring];
	uint32_t end = (uint16_t)*wlanTxRingWriteIndex[ring];

	while (index != end) {
		struct WlanMacTxRequest *request = (struct WlanMacTxRequest *)MAC_TX_RING_ENTRY(ring, index);
		OsaBuffer *buffer = REQUEST_BUFFER(request);

		if (owner != 0 && (uintptr_t)buffer->context != owner)
			wlanTxRingRetarget(request, ring);
		else
			wlanTxRingComplete(request);
		index = (index + 4) & 0x1ff;
	}
	*wlanTxRingReadIndex[ring] = index;
	sysIrqRestore(posture);
}

/* Moves the entries filled but not published to another ring. */
void wlanTxRingMoveFilled(unsigned int ring, unsigned int target)
{
	unsigned int posture = sysIrqDisable();

	if (wlanTxRingPending[ring] != 0) {
		while (wlanTxRingPublished[ring] != wlanTxRingFilled[ring]) {
			wlanTxRingFill((struct WlanMacTxRequest *)
				       MAC_TX_RING_ENTRY(ring, wlanTxRingPublished[ring]), target);
			wlanTxRingPublished[ring] += 4;
			--wlanTxRingPending[ring];
		}
		if (wlanTxRingPending[ring] != 0)
			for (;;)
				;
		wlanTxRingFilled[ring] = *wlanTxRingWriteIndex[ring];
		wlanTxRingPublished[ring] = *wlanTxRingWriteIndex[ring];
	}
	sysIrqRestore(posture);
}

/*
 * Goes through the entries filled but not published: those of the owner are
 * completed (all of them when owner is 0), the rest are put back.
 */
void wlanTxRingRequeueFilled(unsigned int ring, uintptr_t owner)
{
	unsigned int posture = sysIrqDisable();

	if (wlanTxRingPending[ring] != 0) {
		while (wlanTxRingPublished[ring] != wlanTxRingFilled[ring]) {
			struct WlanMacTxRequest *request = (struct WlanMacTxRequest *)
				MAC_TX_RING_ENTRY(ring, wlanTxRingPublished[ring]);
			OsaBuffer *buffer = REQUEST_BUFFER(request);

			if (owner != 0 && (uintptr_t)buffer->context != owner)
				wlanTxRingFill(request, ring);
			else
				wlanTxRingComplete(request);
			wlanTxRingPublished[ring] += 4;
			--wlanTxRingPending[ring];
		}
		if (owner == 0 && wlanTxRingPending[ring] != 0)
			for (;;)
				;
		wlanTxRingFilled[ring] = *wlanTxRingWriteIndex[ring];
		wlanTxRingPublished[ring] = *wlanTxRingWriteIndex[ring];
	}
	sysIrqRestore(posture);
}

/*
 * Takes all entries filled but not published off the ring and puts each on
 * the other ring when its frame is of the given type and addressed to the
 * address, and back on this one otherwise.
 */
void wlanTxRingRouteFilled(unsigned int ring, unsigned int other, unsigned int type, const void *address)
{
	unsigned int posture = sysIrqDisable();

	if (wlanTxRingPending[ring] != 0) {
		int tail = wlanTxRingFilled[ring];
		int head = wlanTxRingPublished[ring];
		int pending = wlanTxRingPending[ring];
		uint32_t index = *wlanTxRingWriteIndex[ring];

		wlanTxRingFilled[ring] = index;
		wlanTxRingPublished[ring] = index;
		wlanTxRingPending[ring] = 0;
		while (head != tail) {
			struct WlanMacTxRequest *request = (struct WlanMacTxRequest *)MAC_TX_RING_ENTRY(ring, head);
			unsigned int target = ring;

			if ((request->type & 7) == type &&
			    memcmp((uint8_t *)(uintptr_t)__aeabi_uread4(request->frame) + 6, address, 6) == 0)
				target = other;
			wlanTxRingFill(request, target);
			head += 4;
			--pending;
		}
		if (pending != 0)
			for (;;)
				;
	}
	sysIrqRestore(posture);
}

/* Table of the frames being sent, by pool and block number. */
struct WlanTxInFlight {
	OsaBuffer *buffer;
	uint32_t time;
};

static struct WlanTxInFlight wlanTxInFlight[41] __attribute__((section(".dtcm.bss")));
static uint8_t wlanTxInFlightCount __attribute__((section(".itcm.bss")));
static uint16_t wlanTxInFlightClashes __attribute__((section(".itcm.bss")));
static uint16_t wlanTxInFlightMisses __attribute__((section(".itcm.bss")));

extern OsaBuffer wlanStaticBuffer;
extern struct OsaBufferPool wlanBlockPool28;
extern struct OsaBufferPool wlanBlockPool7;
extern struct OsaBufferPool wlanBlockPool1;
extern struct OsaBufferPool wlanReplyFramePool;
extern struct OsaBufferPool ampTransmitPool;

/* False for the one statically allocated buffer. */
int wlanBufferIsPooled(const OsaBuffer *buffer)
{
	return buffer != &wlanStaticBuffer;
}

/*
 * Index of a buffer in the table: where its pool starts plus its block
 * number; 39 for a buffer of no known pool and 40 for the static buffer.
 */
unsigned int wlanTxInFlightIndex(OsaBuffer *buffer)
{
	struct OsaBufferPool *pool;
	unsigned int base;
	unsigned int block;

	if (!wlanBufferIsPooled(buffer))
		return 40;
	pool = buffer->pool;
	block = buffer->blockIndex;
	if (pool == &wlanBlockPool28)
		base = 0;
	else if (pool == &wlanBlockPool7)
		base = 28;
	else if (pool == &wlanReplyFramePool)
		base = 35;
	else if (pool == &wlanBlockPool1)
		base = 38;
	else if (pool == &ampTransmitPool)
		base = 39;
	else
		return 39;
	if (block == 255)
		return 39;
	return (uint8_t)(base + block);
}

/* Enters a buffer into the table with the time. */
int wlanTxInFlightAdd(OsaBuffer *buffer)
{
	struct WlanTxInFlight *slot = &wlanTxInFlight[wlanTxInFlightIndex(buffer)];
	ULONG now;

	if (slot->buffer != NULL)
		return (uint16_t)wlanTxInFlightClashes++ + 1;
	slot->buffer = buffer;
	sysGetTime(&now);
	slot->time = now;
	return (uint8_t)wlanTxInFlightCount++ + 1;
}

/* Takes a buffer out of the table; 0 if it was not there. */
int wlanTxInFlightRemove(OsaBuffer *buffer)
{
	struct WlanTxInFlight *slot = &wlanTxInFlight[wlanTxInFlightIndex(buffer)];

	if (slot->buffer == buffer) {
		slot->buffer = NULL;
		slot->time = 0;
		--wlanTxInFlightCount;
		return 1;
	}
	++wlanTxInFlightMisses;
	return 0;
}

/* Tries to put a request on its ring; for the owner's rings the filled entries are published first. */
int wlanTxCheckRing(struct WlanConnection *connection, struct WlanMacTxRequest *request, unsigned int ring)
{
	unsigned int posture = sysIrqDisable();
	volatile uint32_t *readIndex;
	volatile uint32_t *writeIndex;

	if (connection != NULL && connection->mode != 2)
		wlanTxPowerSaveCount += wlanFrameSetPowerSave(request, macTxGetPowerSave());
	readIndex = wlanTxRingReadIndex[ring];
	writeIndex = wlanTxRingWriteIndex[ring];
	if (ring < WLAN_TX_RING_SHADOWED && wlanTxRingPending[ring] != 0) {
		if (wlanWindowOwner(ring, wlanWindowOwners) == 1) {
			while (wlanTxRingPending[ring] != 0)
				wlanTxRingPublish(ring);
		} else {
			wlanTxRingPending[ring] = 0;
			writeIndex = wlanTxRingWriteIndex[ring];
			*writeIndex = wlanTxRingFilled[ring] & 0x1ff;
		}
	}
	if (wlanWindowOwner(ring, wlanWindowOwners) != 1 || (REQUEST_FLAGS(request) & 0x2000) != 0) {
		if (wlanTxRingPush(request, ring, readIndex, writeIndex, &wlanTxRingMaskPush) == 0) {
			sysIrqRestore(posture);
			return 0;
		}
	} else {
		wlanTxRingPushPublished(request, ring, writeIndex);
	}
	sysIrqRestore(posture);
	return 1;
}

/* Builds the transmit request of a frame; not converted yet. */
int macTxNotifyScheduleTable(struct WlanConnection *connection, void *data);	/* 0x31ae8 */

/*
 * The buffer data starts with the transmit descriptor; the transmit request
 * is at 20, the pointer to the frame at 96 and the ring number at 108.
 */
#define TX_DATA_REQUEST(data)	((struct WlanMacTxRequest *)((data) + 20))
#define TX_DATA_FRAME(data)	__aeabi_uread4((data) + 96)
#define TX_DATA_RING(data)	((data)[108])

/* Sends a frame again, waiting a tick at a time while its ring is full. */
int wlanTxResubmit(OsaBuffer *buffer)
{
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

	wlanTxInFlightAdd(buffer);
	while (wlanTxCheckRing(buffer->context, TX_DATA_REQUEST(data), TX_DATA_RING(data)) == 0) {
		wlanTxInFlightRemove(buffer);
		osaTaskSleep(1);
		wlanTxInFlightAdd(buffer);
	}
	return 1;
}

/* Builds the transmit request of a frame and sends it, with interrupts masked between attempts. */
int wlanTxSubmit(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	unsigned int posture;

	if (connection == NULL) {
		sysIrqDisable();
		for (;;)
			;
	}
	if (TX_DATA_FRAME(data) == 0)
		return 0;
	posture = sysIrqDisable();
	wlanTxInFlightAdd(buffer);
	macTxNotifyScheduleTable(connection, data);
	while (wlanTxCheckRing(connection, TX_DATA_REQUEST(data), TX_DATA_RING(data)) == 0) {
		wlanTxInFlightRemove(buffer);
		sysIrqRestore(posture);
		osaTaskSleep(1);
		posture = sysIrqDisable();
		wlanTxInFlightAdd(buffer);
	}
	sysIrqRestore(posture);
	return 1;
}
