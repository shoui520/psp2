/*
 * WAPI for the frames the firmware itself encrypts and decrypts (see
 * robin/wapi.h). The filename is inferred; no source filename survives.
 */
#include "robin/ccm.h"
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/swcrypto.h"
#include "robin/wapi.h"
#include "robin/wep.h"
#include "robin/wlanrx.h"

struct WlanWapiCounters wlanWapiCounters __attribute__((section(".dtcm.rw")));	/* 0x0400105c */

/* The key data of a cipher key (the part after the 12-byte header). */
#define KEY_WAPI(key)	((cipher_key_wapi_t *)&(key)->ckd.wapi)

int wlanAddressIsBroadcast(const uint8_t *address)
{
	uint32_t broadcast[2];

	broadcast[1] = 0xffff;
	broadcast[0] = 0xffffffff;
	return memcmp(address, broadcast, 6) == 0;
}

/*
 * Encrypts the data of a frame the firmware sends: the frame is at `frame`
 * (the length first), the header at frame + 2 and the data after the WAPI
 * header, at frame + 50. The engine is started and does the rest.
 */
void wlanWapiEncryptFrame(int qos, unsigned int priority, uint8_t *frame, unsigned int length,
			  cipher_key_wapi_t *key)
{
	uint8_t aad[48];
	uint8_t packetNumber[16];
	uint8_t *data = frame + 50;
	unsigned int dataLength = (uint16_t)(length - WLAN_WAPI_OVERHEAD);
	unsigned int i;

	wlanWapiBuildAad(qos, priority, dataLength, aad, frame, frame[32]);
	for (i = 0; i < 16; i++)
		packetNumber[15 - i] = frame[2 + 32 + i];
	wlanWapiLoadEngine(0, qos, packetNumber, aad, data, dataLength, key->dataKey, key->micKey,
			   data, 0);
}

/*
 * Decrypts a received frame: the packet number is at frame + 38, the data
 * after it, and the engine writes the plaintext over the WAPI header. A
 * length of the frame counts 96 bytes more than its data and header.
 */
int wlanWapiDecryptFrame(unsigned int priority, uint8_t *frame, unsigned int length,
			 cipher_key_wapi_t *key)
{
	uint8_t aad[48];
	uint8_t packetNumber[16];
	uint8_t *data = frame + 54;
	unsigned int dataLength = (uint16_t)(length - 130);
	int qos = 0;
	unsigned int i;

	for (i = 0; i < 16; i++)
		packetNumber[15 - i] = frame[i + 38];
	if (((frame[2] >> 2) & 3) == 2)
		qos = (frame[2] >> 4) == 8;
	wlanWapiBuildAad(qos, priority, dataLength, aad, frame, frame[36]);
	wlanWapiLoadEngine(1, qos, packetNumber, aad, data, dataLength, key->dataKey, key->micKey,
			   data - 18, 0);
	return 1;
}

/* Sends the frame through the engine: the key index, the next packet number, then the encryption. */
int wlanWapiSend(uint8_t *frame, int unused, unsigned int priority, cipher_key_t *key)
{
	int qos = 0;
	uint32_t posture;
	unsigned int type;

	wlanCryptoTxHeader = frame;
	wlanCryptoAcquire(0);
	wlanCryptoTxActive = 1;
	frame[3] |= 0x40;
	type = frame[2];
	if (((type >> 2) & 3) == 2)
		qos = (type >> 4) == 8;
	frame[32] = key->ckd.wapi.keyIndex;
	frame[33] = 0;
	wlanWapiNextPacketNumber(KEY_WAPI(key), frame + 34);
	posture = sysIrqDisable();
	wlanWapiEncryptFrame(qos, priority & 7, frame, (frame[1] << 8) | frame[0], KEY_WAPI(key));
	return sysIrqRestore(posture);
}

/*
 * Passes a received WAPI frame to the engine if its key index and its
 * packet number are good. A frame with the index of the key this one
 * replaced (pLastKey) is decrypted with that key; one with the current index
 * drops that key. Returns 1 if the engine has it.
 */
int wlanWapiReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key)
{
	struct WlanConnection *connection = buffer->context;
	uint8_t *frame = (uint8_t *)__aeabi_uread4(status->frame);
	cipher_key_wapi_t *keyData = KEY_WAPI(key);
	struct WlanKeyBuffer *lastKey;
	unsigned int priority;

	if (!(frame[3] & 0x40) || ((frame[1] << 8) | frame[0]) <= 0x82)
		return 0;
	lastKey = (struct WlanKeyBuffer *)__aeabi_uread4(&keyData->pLastKey);
	if (frame[2 + 34] == keyData->keyIndex) {
		if (lastKey != NULL) {
			wlanKeyBufferFree(lastKey);
			__aeabi_uwrite4(0, &keyData->pLastKey);
		}
	} else {
		if (lastKey == NULL || lastKey->key.ckd.wapi.keyIndex != frame[2 + 34]) {
			wlanWapiCounters.unknownKeys++;
			return 0;
		}
		keyData = &lastKey->key.ckd.wapi;
		wlanWapiCounters.lastKeyFrames++;
	}
	if (!wlanWapiCheckPacketNumber(connection->mode != 2, keyData, frame + 2 + 36)) {
		wlanWapiCounters.replays++;
		return 0;
	}
	buffer->length -= WLAN_WAPI_OVERHEAD;
	priority = status->qosControl[0] & 7;
	wlanCryptoScratch.buffer = buffer;
	__aeabi_memcpy(&wlanCryptoScratch.status, status, sizeof(wlanCryptoScratch.status));
	wlanCryptoAcquire(0);
	wlanCryptoRxActive = 1;
	wlanCryptoRxKey = key;
	return wlanWapiDecryptFrame(priority, frame, (frame[1] << 8) | frame[0], keyData);
}

/*
 * Loads the engine: the keys, the packet number, the three AAD blocks (a
 * fourth for QoS data), the source and the destination, the mode and the
 * length; then hands the block to the engine and starts it.
 */
int wlanWapiLoadEngine(int decrypt, int qos, const uint8_t *packetNumber, const uint8_t *aad,
		       void *source, unsigned int length, const uint8_t *dataKey,
		       const uint8_t *micKey, void *destination, uint32_t block)
{
	uint32_t mode = 0x440;
	volatile uint8_t *micKeyRegisters = (volatile uint8_t *)CCM_KEY(block);
	volatile uint8_t *dataKeyRegisters = (volatile uint8_t *)CCM_KEY2(block);
	volatile uint8_t *packetNumberRegisters = (volatile uint8_t *)CCM_BLOCK0(block);
	volatile uint8_t *aad1Registers = (volatile uint8_t *)CCM_BLOCK1(block);
	volatile uint8_t *aad2Registers = (volatile uint8_t *)CCM_BLOCK2(block);
	volatile uint8_t *aad3Registers = CCM_BLOCK3(block);
	int i;

	for (i = 0; i < 16; i++) {
		micKeyRegisters[i] = micKey[i];
		dataKeyRegisters[i] = dataKey[i];
		packetNumberRegisters[i] = packetNumber[i];
		aad1Registers[i] = aad[i];
		aad2Registers[i] = aad[i + 16];
		if (qos)
			aad3Registers[i] = aad[i + 32];
	}
	CCM_SOURCE(block) = (uint32_t)(uintptr_t)source;
	CCM_DESTINATION(block) = (uint32_t)(uintptr_t)destination;
	if (qos) {
		mode = 0x4c0;
		wlanWapiCounters.qosFrames++;
	} else {
		wlanWapiCounters.frames++;
	}
	if (decrypt == 1)
		mode |= 8;
	CCM_MODE(block) = (length << 16) | mode;
	CRYPTO_MODE |= CRYPTO_MODE_CCM_ENGINE;
	CCM_CONTROL(block) |= CCM_CONTROL_START;
	return 0;
}

/* A halfword of the AAD, which is packed: read and written a byte at a time. */
struct WlanWapiAadHalfword {
	uint16_t value;
} __attribute__((packed));

#define WAPI_AAD16(p)	(((struct WlanWapiAadHalfword *)(p))->value)

/*
 * The additional data of WAPI's MIC: the frame control with the retry,
 * power management and more data bits and the subtype's low bits cleared and
 * the protected bit set, the addresses, the sequence control without its
 * sequence number, the QoS control (QoS data only), the key index and the
 * length of the data, big endian.
 */
void wlanWapiBuildAad(int qos, unsigned int priority, unsigned int length, uint8_t *aad,
		      const uint8_t *frame, uint8_t keyIndex)
{
	uint16_t lengthBe = (uint16_t)(((length >> 8) | (length << 8)) & 0xffff);

	if (qos)
		__aeabi_memclr(aad, 0x30);
	else
		__aeabi_memclr(aad, 0x20);
	aad[0] = frame[2];
	aad[1] = frame[3];
	WAPI_AAD16(aad) = (WAPI_AAD16(aad) & 0xc78f) | 0x4000;
	__aeabi_memcpy(aad + 2, frame + 6, 12);
	__aeabi_memcpy(aad + 16, frame + 18, 6);
	__aeabi_memcpy(aad + 22, frame + 26, 6);
	aad[14] = frame[24];
	aad[15] = frame[25];
	if (qos) {
		WAPI_AAD16(aad + 28) |= priority;
		WAPI_AAD16(aad + 14) &= 0xf;
		aad[30] = keyIndex;
		aad[31] = 0;
		WAPI_AAD16(aad + 32) = lengthBe;
	} else {
		WAPI_AAD16(aad + 14) &= 0xf;
		aad[28] = keyIndex;
		aad[29] = 0;
		WAPI_AAD16(aad + 30) = lengthBe;
	}
}

/* Adds the key's packet number step to its send packet number, with carries, and copies it out. */
void wlanWapiNextPacketNumber(cipher_key_wapi_t *key, uint8_t *output)
{
	unsigned int carry = key->pnStep;
	int i;

	for (i = 0; i < 16; i++) {
		unsigned int before = key->txPn[i];
		uint8_t after = before + carry;

		key->txPn[i] = after;
		if (after >= before)
			break;
		carry = 1;
	}
	__aeabi_memcpy(output, key->txPn, 16);
}

/*
 * A received packet number must be newer than the last one (the receive
 * packet number is compared from its last byte; an equal one passes). A
 * unicast key (step 2) also needs the first byte's parity to differ from
 * `notStation`'s.
 */
int wlanWapiCheckPacketNumber(int notStation, cipher_key_wapi_t *key, const uint8_t *packetNumber)
{
	int i;

	if (key->pnStep != 2 || ((packetNumber[0] ^ notStation) & 1) == 0) {
		for (i = 15; ; ) {
			unsigned int stored = key->rxPn[i];
			unsigned int received = packetNumber[i];

			if (stored > received)
				break;
			if (stored >= received && --i >= 0)
				continue;
			__aeabi_memcpy(key->rxPn, packetNumber, 16);
			return 1;
		}
	}
	return 0;
}

/* output[i] = input[length - 1 - i]. */
void wlanReverseBytes(uint8_t *output, const uint8_t *input, uint8_t length)
{
	int last = (uint8_t)(length - 1);
	int i;

	for (i = 0; i <= last; i++)
		output[i] = input[last - i];
}
