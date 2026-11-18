/*
 * Transmit rings of the MAC and the table of frames in flight. See
 * robin/txring.h. The filename is inferred.
 */
#include <stddef.h>
#include "robin/amp.h"
#include "robin/apps.h"
#include "robin/ieee80211.h"
#include "robin/mac.h"
#include "robin/mef.h"
#include "robin/pspoll.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txhold.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"

/* Not converted yet; provisional name. */
void macMgmtConvertPacket(struct WlanConnection *connection);	/* 0x3a32a */

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

uint32_t wlanTxRingPending[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.bss")));
uint32_t wlanTxRingPublished[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.bss")));
uint32_t wlanTxRingFilled[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.bss")));
/* Entries published at once. */
uint32_t wlanTxRingPublishLimit __attribute__((section(".sqram1.bss")));
/* Policy bits cleared when a ring is filled (one per ring) and set or cleared when a frame is pushed. */
uint32_t wlanTxRingMaskFill __attribute__((section(".sqram1.bss")));
uint32_t wlanTxRingMaskPush __attribute__((section(".sqram1.bss")));
/* Running total of what wlanFrameSetPowerSave() returned. */
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
__attribute__((noinline)) int wlanTxRingPublish(unsigned int ring)
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
__attribute__((noinline)) void wlanTxRingComplete(struct WlanMacTxRequest *request)
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

#define WLAN_TX_IN_FLIGHT_SLOTS		44
#define WLAN_TX_IN_FLIGHT_POOLED_SLOTS	40	/* the rest are for buffers of no pool */
#define WLAN_TX_IN_FLIGHT_CHECK_MS	10	/* between two looks for frames the MAC kept too long */
#define WLAN_TX_IN_FLIGHT_STALE_MS	330

static struct WlanTxInFlight wlanTxInFlight[WLAN_TX_IN_FLIGHT_SLOTS] __attribute__((section(".dtcm.bss")));
static uint8_t wlanTxInFlightCount __attribute__((section(".itcm.bss")));		/* 0x54f2c */
static uint16_t wlanTxInFlightStale __attribute__((section(".itcm.bss")));		/* 0x54f2e: frames found past the limit */
static uint16_t wlanTxInFlightClashes __attribute__((section(".itcm.bss")));		/* 0x54f30 */
static uint16_t wlanTxInFlightMisses __attribute__((section(".itcm.bss")));		/* 0x54f32 */
static uint32_t wlanTxInFlightIdle __attribute__((section(".itcm.bss")));		/* 0x54f34: the MAC went idle with frames out */
static uint32_t wlanTxInFlightChecked __attribute__((section(".itcm.bss")));		/* 0x54f38: when the table was last looked at */

extern struct OsaBufferPool wlanBlockPool1;
extern struct OsaBufferPool wlanReplyFramePool;
extern struct OsaBufferPool ampTransmitPool;

/* False for the one statically allocated frame, the ATIM frame. */
int wlanBufferIsPooled(const OsaBuffer *buffer)
{
	return (const void *)buffer != &wlanAtimBuffer.frame;
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

/* How many frames the table holds. */
__attribute__((noinline)) unsigned int wlanTxInFlightFrames(void)
{
	return wlanTxInFlightCount;
}

/* The table is empty. */
void wlanTxInFlightClear(void)
{
	__aeabi_memclr4(wlanTxInFlight, sizeof(wlanTxInFlight));
	wlanTxInFlightCount = 0;
}

/* The first frame control byte of the frame the transmit data points to. */
static inline uint8_t wlanTxFrameControl(const uint8_t *data)
{
	return ((const uint8_t *)(uintptr_t)TX_DATA_FRAME(data))[2];
}

/* The frame is a probe response (management, subtype 5). */
static inline int wlanTxFrameIsProbeResponse(const uint8_t *data)
{
	uint8_t control = wlanTxFrameControl(data);

	return (control & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGMT &&
	       (control >> IEEE80211_FC0_SUBTYPE_SHIFT) == 5;
}

/* Every frame in the table is given up: counted, told to the owners and freed. */
void wlanTxInFlightPurge(void)
{
	unsigned int posture = sysIrqDisable();
	unsigned int i;

	for (i = 0; i < WLAN_TX_IN_FLIGHT_SLOTS; i++) {
		OsaBuffer *buffer = wlanTxInFlight[i].buffer;
		const uint8_t *data;
		int release = 1;

		if (buffer == NULL)
			continue;
		data = (const uint8_t *)buffer + buffer->dataOffset;
		if (ieee80211IsData(wlanTxFrameControl(data)))
			wlanTxCountDropped(buffer);
		if (TX_DATA_TAG(data) == 48)
			wlanReplyFrameFree(buffer);
		if (wlanTxFrameIsProbeResponse(data) && ((struct WlanConnection *)buffer->context)->interfaceType == 2) {
			macMgmtConvertPacket(buffer->context);
			release = 0;
		}
		wlanApPsTxDone(buffer->context, data);
		if (wlanTxFrameIsProbeResponse(data) && ((struct WlanConnection *)buffer->context)->interfaceType == 4) {
			((struct WlanConnection *)buffer->context)->transmitBusy = 0;
			release = 0;
		}
		if ((i < WLAN_TX_IN_FLIGHT_POOLED_SLOTS || wlanBufferIsPooled(buffer)) && release) {
			if (((struct WlanConnection *)buffer->context)->interfaceType == 4 &&
			    buffer->pool == &ampTransmitPool)
				mdevIoctl(ampPalDevice, 0x53, (uintptr_t)buffer);
			osa_BufferFree(buffer);
		}
	}
	wlanTxInFlightClear();
	sysIrqRestore(posture);
}

/*
 * Looks, every 10 ms, for frames the MAC has kept longer than 330 ms and asks the transmit
 * task to flush them; when the MAC went idle with frames still out the notify task is told.
 */
int wlanTxInFlightWatchdog(void)
{
	unsigned int posture = sysIrqDisable();
	ULONG now;

	if (sysEventEngineIsActive() != 0 || MAC_REG(0x10) == MAC_REG(0x14)) {
		int remaining = wlanTxInFlightCount;

		sysGetTime(&now);
		if (remaining != 0 && now - wlanTxInFlightChecked >= WLAN_TX_IN_FLIGHT_CHECK_MS) {
			unsigned int stale = 0;
			unsigned int i = 0;

			wlanTxInFlightChecked = now;
			do {
				if (remaining <= 0)
					break;
				if (wlanTxInFlight[i].buffer != NULL) {
					--remaining;
					if (now - wlanTxInFlight[i].time > WLAN_TX_IN_FLIGHT_STALE_MS) {
						++stale;
						++wlanTxInFlightStale;
					}
				}
				i++;
			} while (i < WLAN_TX_IN_FLIGHT_SLOTS);
			if (stale != 0)
				macTxPostEvent(MAC_TX_EVENT_FLUSH);
		}
		return sysIrqRestore(posture);
	}
	sysIrqRestore(posture);
	++wlanTxInFlightIdle;
	return osaEventFlagsSet(macTxNotifyEventFlags, 1, 7);
}

/* A frame of the connection is in the table. */
int wlanTxInFlightHasConnection(const struct WlanConnection *connection)
{
	int found = 0;
	unsigned int i = 0;

	do {
		OsaBuffer *buffer = wlanTxInFlight[i].buffer;

		if (buffer != NULL && buffer->context == connection)
			found = 1;
		i++;
	} while (i < WLAN_TX_IN_FLIGHT_SLOTS && found == 0);
	return found;
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
	wlanTxRequestBuild(connection, data);
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

/* Forgets the filled entries of the shadowed rings. */
void wlanTxRingClearCounters(void)
{
	unsigned int posture = sysIrqDisable();
	unsigned int ring;

	for (ring = 0; ring < WLAN_TX_RING_SHADOWED; ring++) {
		wlanTxRingPending[ring] = 0;
		wlanTxRingPublished[ring] = 0;
		wlanTxRingFilled[ring] = 0;
	}
	sysIrqRestore(posture);
}

/* Whether any shadowed ring has entries not yet published. */
int wlanTxRingsPending(void)
{
	unsigned int posture = sysIrqDisable();
	int ring;

	for (ring = 0; ring < WLAN_TX_RING_SHADOWED; ring++) {
		if (wlanTxRingPending[ring] != 0) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

/* Whether the MAC has read every entry that was published (the write and read indexes agree). */
int wlanTxRingsDrained(void)
{
	return wlanHalfwordsEqual((const uint16_t *const *)wlanTxRingWriteIndex,
				  (const uint16_t *const *)wlanTxRingReadIndex, WLAN_TX_RING_SHADOWED);
}

/* Whether frames are in flight that are not on a shadowed ring (the MAC has them). */
int wlanTxFramesOutsideRings(void)
{
	uint8_t pending = 0;
	int ring;

	for (ring = 0; ring < WLAN_TX_RING_SHADOWED; ring++)
		pending = wlanTxRingPending[ring] + pending;
	return (int)(wlanTxInFlightFrames() - pending) > 0;
}

/* Puts the request of a management frame on its ring (the highest priority for a beacon). */
int wlanTxQueueMgmtFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	unsigned int ring;

	if (TX_DATA_FRAME(data) == 0)
		return 0;
	wlanTxRequestSetup(connection, data);
	if ((((const uint8_t *)__aeabi_uread4(TX_DATA_REQUEST(data)->frame))[2] >> 4) == 9)
		ring = 1;
	else
		ring = TX_DATA_RING(data);
	return wlanTxRingFill(TX_DATA_REQUEST(data), ring);
}

/* Builds the request of a data frame and puts it on its ring. */
int wlanTxQueueFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

	if (TX_DATA_FRAME(data) == 0)
		return 0;
	wlanTxRequestBuild(connection, data);
	return wlanTxRingFill(TX_DATA_REQUEST(data), TX_DATA_RING(data));
}

/*
 * The rings in the order they publish in, how many entries of each go out
 * per round (reloaded when a round finds none) and what is left of this
 * round's. The ring 4 is not counted.
 */
uint8_t wlanTxRingOrder[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.rw"))) = { 4, 5, 6, 3, 2, 1, 0 };	/* 0xc0011c08 */
uint8_t wlanTxRingShare[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.rw"))) = { 0xff, 0x0a, 0x0a, 1, 1, 1, 1 };	/* 0xc0011c0f */
uint8_t wlanTxRingBudget[WLAN_TX_RING_SHADOWED] __attribute__((section(".sqram1.rw"))) = { 0xff, 0x0a, 0x0a, 1, 1, 1, 1 };	/* 0xc0011c16 */

/* Publishes the entries of the first ring in the order that has some and a share left; 0 if none. */
int wlanTxRingPublishNext(void)
{
	int published = 0;
	int rounds = 0;
	unsigned int posture = sysIrqDisable();

	do {
		int i = 0;

		while (published == 0) {
			unsigned int ring = wlanTxRingOrder[i];

			if (wlanTxRingPending[ring] != 0 && (wlanTxRingBudget[ring] != 0 || ring == 4)) {
				wlanTxRingPublish(ring);
				if (ring != 4) {
					wlanTxRingBudget[ring]--;
					wlanTxDataRing = ring;
				}
				published = 1;
			}
			if (++i >= WLAN_TX_RING_SHADOWED) {
				if (published == 0)
					__aeabi_memcpy(wlanTxRingBudget, wlanTxRingShare, WLAN_TX_RING_SHADOWED);
				break;
			}
		}
		rounds++;
	} while (rounds < 2 && published == 0);
	sysIrqRestore(posture);
	return published;
}

/* Hands every ring's filled entries to the MAC at once. */
void wlanTxRingsFlush(void)
{
	unsigned int posture = sysIrqDisable();
	int i = 0;

	do {
		unsigned int ring = wlanTxRingOrder[i];

		if (wlanTxRingPending[ring] != 0) {
			if (wlanWindowOwner(ring, wlanWindowOwners) == 1) {
				while (wlanTxRingPending[ring] != 0)
					wlanTxRingPublish(ring);
			} else {
				wlanTxRingPending[ring] = 0;
				*wlanTxRingWriteIndex[ring] = wlanTxRingFilled[ring] & 0x1ff;
			}
		}
		i++;
	} while (i < WLAN_TX_RING_SHADOWED);
	sysIrqRestore(posture);
}
