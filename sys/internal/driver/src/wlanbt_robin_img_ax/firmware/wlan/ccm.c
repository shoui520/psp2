/*
 * The CCM engine of the MAC register file (see robin/ccm.h) and AES-CMAC on
 * top of the AES block function. The filename is inferred; no source
 * filename survives. Four ROM-patch slots (0x55c10..0x55c1c) let a patch
 * take over AES-CMAC, the AES block and the two CCM operations.
 */
#include "robin/ccm.h"
#include "robin/esa.h"
#include "robin/mef.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/wep.h"

static ROBIN_PATCH_SLOT(mrvlAesCmacOverride);		/* 0x00055c10 */
static ROBIN_PATCH_SLOT(wlanAesBlockOverride);		/* 0x00055c14 */
static ROBIN_PATCH_SLOT(wlanCcmDecryptOverride);	/* 0x00055c18 */
static ROBIN_PATCH_SLOT(wlanCcmEncryptOverride);	/* 0x00055c1c */

/* Rb of AES-CMAC (RFC 4493): the constant the subkey derivation xors in. */
static uint8_t const_Rb[16] __attribute__((section(".itcm.rw"))) = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x87,
};								/* 0x00054ec4 */

uint32_t wlanCcmAadLength __attribute__((section(".itcm.rw")));		/* 0x00054ed4 */
uint32_t wlanCcmAadLengthQos __attribute__((section(".itcm.rw")));	/* 0x00054ed8 */
/* Space in front of a received frame that holds the offload headers. */
uint32_t wlanOffloadHeadroom __attribute__((section(".itcm.rw")));	/* 0x00054edc */
uint32_t wlanCcmIvLow __attribute__((section(".itcm.rw")));		/* 0x00054ee0 */
uint32_t wlanCcmIvHigh __attribute__((section(".itcm.rw")));		/* 0x00054ee4 */
void (*wlanCcmHookPrepare)(uint8_t *data, const uint32_t *key, uint32_t length,
			   const WlanWirelessFrame *frame) __attribute__((section(".itcm.rw")));	/* 0x00054ee8 */
void (*wlanCcmHookStart)(void) __attribute__((section(".itcm.rw")));	/* 0x00054eec */
uint32_t wlanCcmPollLoops __attribute__((section(".itcm.rw")));		/* 0x00054ef0 */

/* The default prepare hook: nothing to do. */
void wlanCcmPrepare(uint8_t *data, const uint32_t *key, uint32_t length,
		    const WlanWirelessFrame *frame)
{
}

/* The default start hook. */
void wlanCcmStart(void)
{
	CCM_MODE(0) |= CCM_MODE_HOLD;
}

/* Set while a frame is moving through the engine's input. */
void wlanCcmInputHold(void)
{
	CCM_MODE(0) |= CCM_MODE_INPUT_HOLD;
}

void wlanCcmInputRelease(void)
{
	CCM_MODE(0) &= ~CCM_MODE_INPUT_HOLD;
}

/* Waits until a bit of the engine's status is set; returns the status. */
uint32_t wlanCcmWaitSet(uint32_t mask, uint32_t block)
{
	uint32_t status;

	do {
		uint32_t i;

		for (i = 0; i < wlanCcmPollLoops; i++)
			;
		status = CCM_STATUS(block);
	} while ((status & mask) == 0);
	return status;
}

/* Waits until the bits of the engine's status are clear; returns the status. */
uint32_t wlanCcmWaitClear(uint32_t mask, uint32_t block)
{
	uint32_t status;

	do {
		uint32_t i;

		for (i = 0; i < wlanCcmPollLoops; i++)
			;
		status = CCM_STATUS(block);
	} while ((status & mask) != 0);
	return status;
}

/* Fixes the engine's parameters and hooks at start-up (called from wlanInit()). */
void wlanCcmSetup(void)
{
	wlanCcmHooksInit();
	wlanCcmAadLength = 22;
	wlanCcmAadLengthQos = 24;
	wlanOffloadHeadroom = 4;
	wlanCcmIvLow = 4;
	wlanCcmIvHigh = 4;
}

static __attribute__((noinline)) void ccmCopy16(volatile uint32_t *destination, const uint32_t *source)
{
	destination[0] = source[0];
	destination[1] = source[1];
	destination[2] = source[2];
	destination[3] = source[3];
}

static __attribute__((noinline)) void ccmCopy6(uint8_t *destination, const uint8_t *source)
{
	uint16_t *to = (uint16_t *)destination;
	const uint16_t *from = (const uint16_t *)source;

	to[0] = from[0];
	to[1] = from[1];
	to[2] = from[2];
}

/*
 * B0 of CCM for CCMP (flags 0x59: AAD present, M = 8, L = 2), the nonce of
 * the priority, the transmitter address and the packet number from the CCMP
 * header at frame + headroom + 32 (the received frame's offload headroom;
 * 0 when encrypting), and the length of the data, as 802.11 lays it out.
 */
static __attribute__((noinline)) void ccmBlock0(uint8_t *block, const WlanWirelessFrame *frame, uint32_t length, uint32_t priority,
						uint8_t headroom)
{
	const uint8_t *iv = (const uint8_t *)frame + headroom + 32;
	uint32_t pnHigh;
	uint16_t pnLow;

	block[0] = 0x59;
	block[1] = priority;
	ccmCopy6(block + 2, frame->address2);
	pnHigh = *(const uint32_t *)(iv + 4);
	*(uint32_t *)(block + 8) = (pnHigh << 24) + ((pnHigh & 0xff00) << 8) +
				   ((pnHigh & 0xff0000) >> 8) + (pnHigh >> 24);
	pnLow = *(const uint16_t *)iv;
	*(uint16_t *)(block + 12) = (uint16_t)((pnLow << 8) | (pnLow >> 8));
	*(uint16_t *)(block + 14) = (uint16_t)((length << 8) | (length >> 8));
}

/*
 * The first AAD block: the length of the AAD, the frame control with the
 * bits CCMP masks out, and the first two addresses.
 */
static __attribute__((noinline)) void ccmAad1(uint8_t *block, const WlanWirelessFrame *frame, uint8_t aadLength)
{
	unsigned int mask;

	block[0] = 0;
	block[1] = aadLength;
	if (((frame->frameControl[0] >> 2) & 3) == 2)
		mask = 0xc78f;
	else
		mask = 0xc7ff;
	/* the frame control, as one halfword (the header is aligned here) */
	*(uint16_t *)(block + 2) = *(const uint16_t *)frame->frameControl & mask;
	ccmCopy6(block + 4, frame->address1);
	ccmCopy6(block + 10, frame->address2);
}

/* The second AAD block: the third address, the sequence control and the fourth address or the QoS control. */
static __attribute__((noinline)) void ccmAad2(uint8_t *block, const WlanWirelessFrame *frame, uint32_t qos)
{
	ccmCopy6(block, frame->address3);
	block[6] = frame->sequenceControl[0] & 0xf;
	block[7] = 0;
	*(uint32_t *)(block + 8) = qos;
	*(uint32_t *)(block + 12) = 0;
	if ((frame->frameControl[1] & 1) && (frame->frameControl[1] & 2)) {
		ccmCopy6(block + 8, frame->address4);
		*(uint16_t *)(block + 14) = (uint16_t)qos;
	}
}

/*
 * Decrypts the data of a CCMP frame in place: the engine reads the
 * ciphertext after the 8-byte header at `data` and writes the plaintext
 * over the header. Returns 1 (the failure shows in the control register).
 */
int wlanCcmDecrypt(const WlanWirelessFrame *frame, uint8_t *data, uint32_t length,
		   const uint32_t *key, uint32_t priority, uint32_t block)
{
	uint8_t scratch[16];
	uint32_t qos = 0;
	int result = 1;
	uint8_t type = frame->frameControl[0];
	uint8_t flags = frame->frameControl[1];
	uint8_t aadLength;

	if (wlanCcmDecryptOverride(frame, data, length, key, priority, &result))
		return result;
	CRYPTO_MODE = 0;
	ccmCopy16(CCM_KEY(block), key);
	CCM_MODE(block) = (length << 16) + CCM_MODE_CCMP_DECRYPT;
	wlanCcmHookStart();
	if (((type >> 2) & 3) == 2 && (type >> 4) == 8) {
		aadLength = wlanCcmAadLengthQos;
		qos = priority;
	} else {
		aadLength = wlanCcmAadLength;
	}
	if ((flags & 1) && (flags & 2))
		aadLength += 6;
	wlanCcmHookPrepare(data, key, length, frame);
	ccmBlock0(scratch, frame, length, priority, wlanOffloadHeadroom);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK0(block), (const uint32_t *)scratch);
	ccmAad1(scratch, frame, aadLength);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK1(block), (const uint32_t *)scratch);
	ccmAad2(scratch, frame, qos);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK2(block), (const uint32_t *)scratch);
	CCM_SOURCE(block) = (uint32_t)(uintptr_t)data + 8;
	CCM_DESTINATION(block) = (uint32_t)(uintptr_t)data;
	CRYPTO_MODE |= CRYPTO_MODE_CCM_ENGINE;
	CCM_CONTROL(block) |= CCM_CONTROL_START;
	return result;
}

/*
 * Encrypts the data of a CCMP frame in place: the engine reads at `source`
 * and writes at frame + 40, after the CCMP header.
 */
int wlanCcmEncrypt(const WlanWirelessFrame *frame, uint32_t length, const uint32_t *key,
		   uint8_t *source, uint32_t priority, uint32_t block)
{
	uint8_t scratch[16];
	uint8_t type;
	uint8_t flags;
	int result;
	uint32_t qos;
	uint8_t aadLength;

	if (wlanCcmEncryptOverride(frame, length, key, source, priority, &result))
		return result;
	type = frame->frameControl[0];
	flags = frame->frameControl[1];
	CRYPTO_MODE = 0;
	ccmCopy16(CCM_KEY(block), key);
	CCM_MODE(block) = (length << 16) + CCM_MODE_CCMP_ENCRYPT;
	wlanCcmHookStart();
	if (((type >> 2) & 3) == 2 && (type >> 4) == 8) {
		qos = priority;
		aadLength = wlanCcmAadLengthQos;
	} else {
		aadLength = wlanCcmAadLength;
		qos = 0;
	}
	if ((flags & 1) && (flags & 2))
		aadLength += 6;
	ccmBlock0(scratch, frame, length, priority, 0);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK0(block), (const uint32_t *)scratch);
	ccmAad1(scratch, frame, aadLength);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK1(block), (const uint32_t *)scratch);
	ccmAad2(scratch, frame, qos);
	ccmCopy16((volatile uint32_t *)CCM_BLOCK2(block), (const uint32_t *)scratch);
	CCM_SOURCE(block) = (uint32_t)(uintptr_t)source;
	CCM_DESTINATION(block) = wlanCcmIvLow + wlanCcmIvHigh + (uint32_t)(uintptr_t)frame + 32;
	CRYPTO_MODE |= CRYPTO_MODE_CCM_ENGINE;
	CCM_CONTROL(block) |= CCM_CONTROL_START;
	return 0;
}

/* One AES block, through a patch or the AES unit; returns nonzero when the patch handled it. */
int mrvl_aes_128(const uint8_t *key, const uint8_t *input, uint8_t *output)
{
	int result = wlanAesBlockOverride(key, input, output);

	if (result == 0)
		return MRVL_AesEncrypt((uint8_t *)key, 2, (uint8_t *)input, output);
	return result;
}

__attribute__((noinline)) void xor_128(const uint8_t *left, const uint8_t *right, uint8_t *output)
{
	int i;

	for (i = 0; i < 16; i++)
		output[i] = left[i] ^ right[i];
}

/* output = input << 1 over the 128 bits, most significant byte first. */
__attribute__((noinline)) void leftshift_onebit(const uint8_t *input, uint8_t *output)
{
	uint8_t carry = 0;
	int i;

	for (i = 15; i >= 0; i--) {
		output[i] = (input[i] << 1) | carry;
		carry = input[i] >> 7;
	}
}

/* Derives the two AES-CMAC subkeys of `key` (RFC 4493 section 2.3). */
void generate_subkey(const uint8_t *key, uint8_t *subkey1, uint8_t *subkey2)
{
	uint8_t shifted[16];
	uint8_t zero[16] = { 0 };
	uint8_t l[16];

	mrvl_aes_128(key, zero, l);
	if (l[0] & 0x80) {
		leftshift_onebit(l, shifted);
		xor_128(shifted, const_Rb, subkey1);
	} else {
		leftshift_onebit(l, subkey1);
	}
	if (subkey1[0] & 0x80) {
		leftshift_onebit(subkey1, shifted);
		xor_128(shifted, const_Rb, subkey2);
	} else {
		leftshift_onebit(subkey1, subkey2);
	}
}

/* Pads the last block: `length` bytes of input, then 0x80 and zeros. */
__attribute__((noinline)) void padding(const uint8_t *input, uint8_t *output, int length)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (i < length)
			output[i] = input[i];
		else if (i == length)
			output[i] = 0x80;
		else
			output[i] = 0;
	}
}

/* AES-CMAC of the data with a 128-bit key (the integrity check of protected management frames). */
void mrvl_aes_cmac(const uint8_t *key, const uint8_t *data, int length, uint8_t *mac)
{
	uint8_t subkey2[16];
	uint8_t subkey1[16];
	uint8_t padded[16];
	uint8_t last[16];
	uint8_t chain[16] = { 0 };
	uint8_t block[16];
	const uint8_t *tail;
	const uint8_t *subkey;
	int blocks;
	int i;

	if (mrvlAesCmacOverride(key, data, length, mac))
		return;
	generate_subkey(key, subkey1, subkey2);
	blocks = (length + 15) / 16;
	if (blocks != 0 && (length & 15) == 0) {
		tail = data + 16 * blocks - 16;
		subkey = subkey1;
	} else {
		if (blocks == 0)
			blocks = 1;
		padding(data + 16 * blocks - 16, padded, length - 16 * (length / 16));
		subkey = subkey2;
		tail = padded;
	}
	xor_128(tail, subkey, last);
	for (i = 0; i < blocks - 1; i++) {
		xor_128(chain, data + 16 * i, block);
		mrvl_aes_128(key, block, chain);
	}
	xor_128(chain, last, block);
	mrvl_aes_128(key, block, chain);
	for (i = 0; i < 16; i++)
		mac[i] = chain[i];
}
