/*
 * Frames the firmware encrypts and decrypts itself, one at a time through
 * the CCM engine, and the rings for the frames that wait (see
 * robin/swcrypto.h). The filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include "robin/apps.h"
#include "robin/ccm.h"
#include "robin/conn.h"
#include "robin/defrag.h"
#include "robin/mactx.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/swcrypto.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wapi.h"
#include "robin/wep.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"

#define SOC_EVENT_CONTROL_CRYPTO_MASK	0x04004300u

uint32_t wlanCryptoTxActive __attribute__((section(".dtcm.rw")));		/* 0x04000f30 */
uint32_t wlanCryptoRxActive __attribute__((section(".dtcm.rw")));		/* 0x04000f34 */
uint8_t *wlanCryptoTxHeader __attribute__((section(".dtcm.rw")));		/* 0x04000f38 */
cipher_key_t *wlanCryptoRxKey __attribute__((section(".dtcm.rw")));		/* 0x04000f3c */
OsaBuffer *wlanCryptoTxBuffer __attribute__((section(".dtcm.rw")));		/* 0x04000f40 */
uint32_t wlanCryptoCounters[4] __attribute__((section(".dtcm.rw")));		/* 0x04000f44 */
struct WlanCryptoTxRing wlanCryptoTxRing __attribute__((section(".dtcm.bss")));	/* 0x04001070 */
struct WlanCryptoRxRing wlanCryptoRxRing __attribute__((section(".dtcm.bss")));	/* 0x040010ec */
struct WlanCryptoScratch wlanCryptoScratch __attribute__((section(".dtcm.bss")));	/* 0x04008900 */

static inline int txNext(int index)
{
	return index == WLAN_CRYPTO_TX_RING_SIZE - 1 ? 0 : index + 1;
}

static inline int rxNext(int index)
{
	return index == WLAN_CRYPTO_RX_RING_SIZE - 1 ? 0 : index + 1;
}

/* Holds the event paths while the engine or a host command works. */
void socEventsHold(void)
{
	SOC_EVENT_CONTROL |= SOC_EVENT_CONTROL_CRYPTO_MASK;
}

unsigned int socEventsRelease(void)
{
	unsigned int control = SOC_EVENT_CONTROL & ~SOC_EVENT_CONTROL_CRYPTO_MASK;

	SOC_EVENT_CONTROL = control;
	return control;
}

unsigned int wlanCryptoCount(unsigned int index)
{
	return wlanCryptoCounters[index];
}

/* Marks the engine busy and holds the event paths; returns nonzero. */
int wlanCryptoAcquire(unsigned int index)
{
	unsigned int busy = wlanCryptoCounters[index];

	if (busy == 0) {
		socEventsHold();
		wlanCryptoCounters[index] = 1;
		return 1;
	}
	return busy;
}

/* Empties both rings. */
void wlanCryptoRingsInit(struct WlanCryptoTxRing *tx, struct WlanCryptoRxRing *rx)
{
	tx->tail = -1;
	tx->head = 0;
	rx->tail = -1;
	rx->head = 0;
}

/* Takes the oldest held frame out of the ring; NULL if there is none. */
struct WlanCryptoHeldFrame *wlanCryptoRxPop(struct WlanCryptoRxRing *ring)
{
	int head = ring->head;
	struct WlanCryptoHeldFrame *entry;

	if (rxNext(ring->tail) == head)
		return NULL;
	entry = &ring->entries[head];
	ring->head = rxNext(head);
	return entry;
}

/* Queues a buffer to send; 0 if the ring is full. */
int wlanCryptoTxPush(OsaBuffer *buffer, struct WlanCryptoTxRing *ring)
{
	int tail = ring->tail;

	if (txNext(txNext(tail)) == ring->head)
		return 0;
	tail = txNext(tail);
	ring->tail = tail;
	ring->entries[tail] = buffer;
	return 1;
}

OsaBuffer *wlanCryptoTxPop(struct WlanCryptoTxRing *ring)
{
	int head = ring->head;
	OsaBuffer *buffer;

	if (txNext(ring->tail) == head)
		return NULL;
	buffer = ring->entries[head];
	ring->head = txNext(head);
	return buffer;
}

/* Completes every queued buffer. */
void wlanCryptoTxDrain(struct WlanCryptoTxRing *ring)
{
	OsaBuffer *buffer;

	while ((buffer = wlanCryptoTxPop(ring)) != NULL)
		wlanCompletePacket(buffer, 0);
}

/* Releases every held frame. */
void wlanCryptoRxDrain(struct WlanCryptoRxRing *ring)
{
	struct WlanCryptoHeldFrame *entry;

	while ((entry = wlanCryptoRxPop(ring)) != NULL)
		wlanCryptoReleaseBuffer(entry->buffer);
}

/* Takes the frames of a connection out of the engine and both rings. */
void wlanCryptoFlushConnection(struct WlanConnection *connection)
{
	struct WlanCryptoTxRing *tx = &wlanCryptoTxRing;
	struct WlanCryptoRxRing *rx = &wlanCryptoRxRing;
	uint32_t posture = sysIrqDisable();
	int removed = 0;
	int read;
	int write;
	int last;

	if (wlanCryptoTxActive == 1 && wlanCryptoTxBuffer->context == connection)
		wlanCryptoTxBuffer->context = NULL;
	read = tx->head;
	write = read;
	last = read;
	while (txNext(tx->tail) != read) {
		OsaBuffer *buffer = tx->entries[read];

		if (buffer->context == connection) {
			removed++;
			wlanCompletePacket(buffer, 0);
			tx->entries[read] = NULL;
		} else {
			tx->entries[write] = buffer;
			last = write;
			write = txNext(write);
		}
		read = txNext(read);
	}
	if (removed != 0) {
		if (tx->head == write)
			tx->head = read;
		else
			tx->tail = last;
	}
	if (wlanCryptoRxActive == 1 && wlanCryptoScratch.buffer->context == connection)
		wlanCryptoScratch.buffer->context = NULL;
	read = rx->head;
	write = 0;
	last = read;
	write = read;
	removed = 0;
	while (rxNext(rx->tail) != read) {
		struct WlanCryptoHeldFrame *entry = &rx->entries[read];
		OsaBuffer *buffer = entry->buffer;

		if (buffer->context == connection) {
			removed++;
			wlanCryptoReleaseBuffer(buffer);
			__aeabi_memclr4(entry, sizeof(*entry));
		} else {
			struct WlanCryptoHeldFrame *target = &rx->entries[write];

			target->buffer = buffer;
			target->key = entry->key;
			__aeabi_memcpy4(target->status, entry->status, 28);
			last = write;
			write = rxNext(write);
		}
		read = rxNext(read);
	}
	if (removed != 0) {
		if (rx->head == write)
			rx->head = read;
		else
			rx->tail = last;
	}
	sysIrqRestore(posture);
}

/* Sends the queued frames of a TID to another ring. */
void wlanCryptoRetargetFrames(unsigned int from, uint8_t to, unsigned int tid)
{
	struct WlanCryptoTxRing *ring = &wlanCryptoTxRing;
	int read;

	for (read = ring->head; txNext(ring->tail) != read; read = txNext(read)) {
		OsaBuffer *buffer = ring->entries[read];
		uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

		if ((data[0] & 7) == tid)
			TX_DATA_RING(data) = to;
	}
}

/* The same for the frame in the engine, if it is for the peer, and for the queue. */
void wlanCryptoMoveFrames(unsigned int from, unsigned int to, unsigned int tid, const void *address)
{
	OsaBuffer *buffer = wlanCryptoTxBuffer;
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;

	if (wlanCryptoTxActive == 1 && (data[0] & 7) == tid &&
	    memcmp((const uint8_t *)*(uint32_t *)(data + 96) + 6, address, 6) == 0)
		TX_DATA_RING(data) = (uint8_t)to;
	if (txNext(wlanCryptoTxRing.tail) != wlanCryptoTxRing.head)
		wlanCryptoRetargetFrames(from, (uint8_t)to, tid);
}

/* True if a frame waits for the engine while it is idle (the firmware hangs on that). */
static int wlanCryptoWaitingWhileIdle(void)
{
	struct WlanCryptoTxRing *tx = &wlanCryptoTxRing;

	if (wlanCryptoRxPeek(&wlanCryptoRxRing) != NULL)
		return 1;
	return txNext(tx->tail) != tx->head && tx->entries[tx->head] != NULL;
}

/* Sends a frame that needs the engine: at once, or after the ones waiting. */
int wlanCryptoSend(OsaBuffer *buffer)
{
	uint32_t posture = sysIrqDisable();

	if (wlanCryptoCounters[WLAN_CRYPTO_BUSY] != 0) {
		if (wlanCryptoTxPush(buffer, &wlanCryptoTxRing) == 0)
			for (;;)
				;
		return sysIrqRestore(posture);
	}
	sysIrqRestore(posture);
	wlanCryptoTxBuffer = buffer;
	if (wlanCryptoWaitingWhileIdle())
		for (;;)
			;
	return wlanCryptoSendTable(buffer);
}

/* Decrypts a received frame: at once, or after the ones waiting. */
void wlanCryptoReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	uint32_t posture = sysIrqDisable();

	if (wlanCryptoCounters[WLAN_CRYPTO_BUSY] != 0) {
		if (wlanCryptoRxPush(buffer, status, key, &wlanCryptoRxRing) == 0)
			for (;;)
				;
		sysIrqRestore(posture);
		return;
	}
	sysIrqRestore(posture);
	if (wlanCryptoWaitingWhileIdle())
		for (;;)
			;
	if (wlanCryptoReceiveTable(&buffer, status, key) == 0)
		wlanCryptoReleaseBuffer(buffer);
}

/* Holds a received frame with its status and key until the engine is free; 0 if the ring is full. */
int wlanCryptoRxPush(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key,
		     struct WlanCryptoRxRing *ring)
{
	int tail = ring->tail;
	struct WlanCryptoHeldFrame *entry;

	if (rxNext(rxNext(tail)) == ring->head)
		return 0;
	tail = rxNext(tail);
	ring->tail = tail;
	entry = &ring->entries[tail];
	entry->buffer = buffer;
	entry->key = key;
	__aeabi_memcpy(entry->status, (const uint8_t *)status + 4, 24);
	entry->status[8] = status->qosControl[0];
	entry->status[9] = status->qosControl[1];
	entry->status[15] = (entry->status[15] & 0xe7) | (status->phyInfo & 0x18);
	entry->frame = __aeabi_uread4(status->frame);
	return 1;
}

struct WlanCryptoHeldFrame *wlanCryptoRxPeek(struct WlanCryptoRxRing *ring)
{
	if (rxNext(ring->tail) == ring->head)
		return NULL;
	return &ring->entries[ring->head];
}

/* Gives a buffer back: to its pool owner if it has one, else to the packet pool. */
void wlanCryptoReleaseBuffer(OsaBuffer *buffer)
{
	if (buffer->pool != NULL)
		macMgmtPostRelease();
	else
		wlanReleasePacket(buffer);
}

/*
 * Starts the next frame when the engine is idle: the held received frames
 * first (their status is put back into the scratch area and the frame is
 * decrypted), then the next frame to send.
 */
int wlanCryptoAdjust(struct WlanCryptoTxRing *tx, struct WlanCryptoRxRing *rx, OsaBuffer **active)
{
	int result;

	for (;;) {
		struct WlanCryptoHeldFrame *entry;

		result = wlanCryptoCounters[WLAN_CRYPTO_BUSY];
		if (result != 0)
			return result;
		entry = wlanCryptoRxPop(rx);
		if (entry == NULL)
			break;
		if (entry->key != NULL) {
			struct WlanRxStatus *status = &rx->scratch->status;

			__aeabi_memcpy((uint8_t *)status + 4, entry->status, 24);
			status->qosControl[0] = entry->status[8];
			status->qosControl[1] = entry->status[9];
			status->phyInfo = (status->phyInfo & 0xe7) | (entry->status[15] & 0x18);
			__aeabi_uwrite4(__aeabi_uread4(&entry->frame), status->frame);
			if (wlanCryptoReceiveTable(&entry->buffer, status, entry->key) != 0)
				break;
		}
		wlanCryptoReleaseBuffer(entry->buffer);
	}
	result = wlanCryptoCounters[WLAN_CRYPTO_BUSY];
	if (result == 0) {
		OsaBuffer *buffer = wlanCryptoTxPop(tx);

		if (buffer != NULL) {
			*active = buffer;
			return wlanCryptoSendTable(buffer);
		}
		result = 0;
	}
	return result;
}

/* Starts a frame the firmware sends, with the key its descriptor names. */
int wlanCryptoSendTable(OsaBuffer *buffer)
{
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	cipher_key_t *key = *(cipher_key_t **)(data + 112);
	struct WlanConnection *connection = buffer->context;
	uint8_t *frame = (uint8_t *)*(uint32_t *)(data + 96);
	unsigned int type = key->hdr.keyType;

	if (type == KEY_TYPE_AES) {
		uint8_t keyId = 0;

		if (connection->mode == 2 && (frame[6] & 1))
			keyId = wlanConnectionInfo(connection)->grpKeyData.KeyIndex;
		wlanCcmpSend(frame, 0, data[0], key, keyId);
	} else if (type == KEY_TYPE_WAPI) {
		wlanWapiSend(frame, 0, data[0], key);
	}
	return 0;
}

/* Starts decrypting a received frame, by the key's cipher; 0 if the engine does not take it. */
int wlanCryptoReceiveTable(OsaBuffer **buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	unsigned int type = key->hdr.keyType;

	if (type == KEY_TYPE_AES)
		return wlanCcmpReceive(*buffer, status, key);
	if (type == KEY_TYPE_WAPI)
		return wlanWapiReceive(*buffer, status, key);
	return 0;
}

/*
 * Encrypts a frame with CCMP: the CCMP header (packet number, key id) in
 * front of the data, the engine for the data and the MIC. The packet number
 * of the key then counts up.
 */
int wlanCcmpSend(uint8_t *frame, int unused, unsigned int priority, cipher_key_t *key, unsigned int keyId)
{
	uint32_t posture;
	unsigned int result;

	wlanCryptoTxHeader = frame;
	wlanCryptoAcquire(0);
	wlanCryptoTxActive = 1;
	frame[3] |= 0x40;
	*(uint16_t *)(frame + 32) = key->ckd.aes.txIv16;
	frame[34] = 0;
	frame[35] = (keyId << 6) + 0x20;
	*(uint32_t *)(frame + 36) = key->ckd.aes.txIv32;
	posture = sysIrqDisable();
	wlanCcmEncrypt((const WlanWirelessFrame *)frame, (frame[1] << 8) | frame[0],
		       (const uint32_t *)key->ckd.aes.key, frame + 40, priority & 7, 0);
	*(uint16_t *)frame = ((frame[1] << 8) | frame[0]) + 16;
	sysIrqRestore(posture);
	result = (uint16_t)(key->ckd.aes.txIv16 + 1);
	key->ckd.aes.txIv16 = result;
	if (result == 0) {
		result = key->ckd.aes.txIv32 + 1;
		key->ckd.aes.txIv32 = result;
	}
	return result;
}

/* Starts decrypting a CCMP frame; the engine writes the plaintext over the CCMP header. */
int wlanCcmpReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *frame = (uint8_t *)__aeabi_uread4(status->frame);
	uint8_t *iv = frame + 36;
	unsigned int length;
	int dataLength;
	uint16_t low;
	uint32_t high;

	if ((frame[39] & 0x20) == 0)
		return 0;
	length = (uint16_t)(buffer->length - 16);
	buffer->length = length;
	dataLength = length - 96;
	if (dataLength <= 0)
		return 0;
	/* The original passes whatever is on its stack when the connection is not ready. */
	low = 0;
	high = 0;
	if (wlanConnectionIsReady(connection))
		wlanRxReplayCheck(iv, (const struct WlanRxKey *)key, &low, &high);
	wlanCryptoScratch.buffer = buffer;
	__aeabi_memcpy(&wlanCryptoScratch.status, status, sizeof(wlanCryptoScratch.status));
	wlanCryptoAcquire(0);
	wlanCryptoRxActive = 1;
	wlanCryptoRxKey = key;
	wlanCcmDecrypt((const WlanWirelessFrame *)frame, iv, (uint16_t)dataLength,
		       (const uint32_t *)key->ckd.aes.key, status->qosControl[0] & 7, 0);
	wlanRxKeySetReplayCounter((struct WlanRxKey *)key, low, high);
	return 1;
}

/*
 * The engine finished a frame, sent or received. A sent frame goes on its
 * way (the next fragment goes through the engine too); a received one is
 * delivered, or released if the decryption failed (status bit 3). Then the
 * next held frame or frame to send starts.
 */
int sysServiceWlanPackets(void)
{
	if (wlanCryptoTxActive == 1) {
		OsaBuffer *buffer = wlanCryptoTxBuffer;
		struct WlanConnection *connection = buffer->context;
		uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
		uint8_t type = wlanCryptoTxHeader[2];

		CCM_CONTROL(0) &= ~CCM_CONTROL_INPUT_PENDING;
		wlanCryptoTxActive = 0;
		wlanCryptoCounters[WLAN_CRYPTO_BUSY] = 0;
		if (connection == NULL) {
			wlanCompletePacket(buffer, 0);
		} else if (!(wlanCryptoTxHeader[3] & WLAN_FC1_MORE_FRAGMENTS)) {
			if (connection->mode == 2 && wlanTxHoldForPeer(buffer, connection) != 0) {
				/* held for the peer's power save */
			} else if (((type >> 2) & 3) != 2) {
				macTxSendMgmtFrame(wlanCryptoTxBuffer);
			} else if (wlanTxSendFrame(wlanCryptoTxBuffer) == 0) {
				wlanCompletePacket(wlanCryptoTxBuffer, 0);
			}
		} else {
			cipher_key_t *key = *(cipher_key_t **)(data + 112);
			uint8_t *next = (uint8_t *)(((uintptr_t)wlanCryptoTxHeader +
						     ((wlanCryptoTxHeader[1] << 8) | wlanCryptoTxHeader[0]) +
						     35) & ~3u);

			if (key->hdr.keyType == KEY_TYPE_AES) {
				uint8_t keyId = 0;

				if (connection->mode == 2 && (wlanCryptoTxHeader[6] & 1))
					keyId = wlanConnectionInfo(connection)->grpKeyData.KeyIndex;
				*(uint16_t *)next = ((next[1] << 8) | next[0]) - 16;
				wlanCcmpSend(next, 0, data[0], key, keyId);
				if (wlanCryptoCounters[WLAN_CRYPTO_BUSY] != 0)
					return wlanCryptoCounters[WLAN_CRYPTO_BUSY];
				return socEventsRelease();
			} else if (key->hdr.keyType == KEY_TYPE_WAPI) {
				wlanWapiSend(next, 0, data[0], key);
				if (wlanCryptoCounters[WLAN_CRYPTO_BUSY] != 0)
					return wlanCryptoCounters[WLAN_CRYPTO_BUSY];
				return socEventsRelease();
			}
			return socEventsRelease();
		}
	} else if (wlanCryptoRxActive == 1) {
		uint32_t control = CCM_CONTROL(0);
		OsaBuffer *buffer = wlanCryptoScratch.buffer;
		struct WlanConnection *connection = buffer->context;
		uint8_t *frame;

		CCM_CONTROL(0) &= ~CCM_CONTROL_INPUT_PENDING;
		wlanCryptoRxActive = 0;
		wlanCryptoCounters[WLAN_CRYPTO_BUSY] = 0;
		wlanCryptoCounters[WLAN_CRYPTO_FRAMES]++;
		if (control & CCM_CONTROL_FAILED)
			wlanCryptoCounters[WLAN_CRYPTO_FAILED]++;
		if (connection == NULL) {
			wlanCryptoReleaseBuffer(buffer);
		} else if (control & CCM_CONTROL_FAILED) {
			wlanCryptoCounters[WLAN_CRYPTO_FAILED_PEER]++;
			wlanCryptoReleaseBuffer(buffer);
		} else {
			uint16_t length;

			frame = (uint8_t *)__aeabi_uread4(wlanCryptoScratch.status.frame);
			if ((frame[6] & 1) == 0 && (wlanCryptoRxKey->hdr.keyDirection & KEY_DIRECTION_RX) &&
			    connection->mode != 2)
				keyMgmtSuccessfulDecryptNotify(wlanCryptoRxKey);
			if (wlanCryptoRxKey->hdr.keyType == KEY_TYPE_AES) {
				frame = (uint8_t *)__aeabi_uread4(wlanCryptoScratch.status.frame);
				length = ((frame[1] << 8) | frame[0]) - 16;
				*(uint16_t *)frame = length;
			} else if (wlanCryptoRxKey->hdr.keyType == KEY_TYPE_WAPI) {
				frame = (uint8_t *)__aeabi_uread4(wlanCryptoScratch.status.frame);
				length = ((frame[1] << 8) | frame[0]) - WLAN_WAPI_OVERHEAD;
				*(uint16_t *)frame = length;
			}
			buffer->length -= 64;
			frame = (uint8_t *)__aeabi_uread4(wlanCryptoScratch.status.frame);
			if (((frame[2] >> 2) & 3) == 2) {
				OsaBuffer *assembled = wlanDefragment(buffer,
								      (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset),
								      &wlanDefragReassembly, &wlanDefragContext,
								      &wlanDefragDiscarding);

				if (assembled != NULL) {
					mdevWlanFormatState(assembled, &wlanCryptoScratch.status);
					mdevWlanSyncState(assembled, &wlanCryptoScratch.status);
				}
			} else {
				macMgmtPostReceived();
			}
		}
	}
	wlanCryptoAdjust(&wlanCryptoTxRing, &wlanCryptoRxRing, &wlanCryptoTxBuffer);
	if (wlanCryptoCounters[WLAN_CRYPTO_BUSY] != 0)
		return wlanCryptoCounters[WLAN_CRYPTO_BUSY];
	return socEventsRelease();
}
