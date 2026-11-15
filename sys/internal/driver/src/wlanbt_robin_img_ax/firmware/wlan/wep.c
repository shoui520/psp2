/*
 * The WEP/CCMP engine (0x4fb0..0x5614). Each request takes the engine
 * (wlanCryptoOps.enter), loads its key, the header addresses and the buffers,
 * starts it, waits for the busy bit to clear and resets the input path. The
 * filename is inferred; no source filename survives.
 */
#include "robin/wep.h"
#include "robin/buffer.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/threadx.h"
#include "robin/btrom.h"

extern struct OsaBufferPool *wlanPacketPool;

WlanCryptoOps wlanCryptoOps __attribute__((section(".itcm.rw")));

static ROBIN_PATCH_SLOT(wlanWepDecryptOverride);
static ROBIN_PATCH_SLOT(wlanWepEncryptOverride);
static ROBIN_PATCH_SLOT(wlanCcmpEncryptOverride);
static ROBIN_PATCH_SLOT(wlanCcmpDecryptOverride);
static ROBIN_PATCH_SLOT(wlanCryptoLoadKeyOverride);
static ROBIN_PATCH_SLOT(wlanCryptoLoadHeaderOverride);

#define WLAN_FC1_TO_DS		0x01
#define WLAN_FC1_FROM_DS	0x02
#define WLAN_FC1_MORE_FRAG	0x04

static inline uint32_t le32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint32_t le16(const uint8_t *p)
{
	return p[0] | p[1] << 8;
}

/*
 * The RC4 seed of a WEP frame: the three IV bytes, then the key the IV's key
 * index selects (5 or 13 bytes).
 */
void wlanWepBuildSeed(const uint8_t *iv, const WlanWepKeyTable *table, uint8_t *seed,
		      uint8_t *isWep104, uint8_t *keyIndex)
{
	unsigned int length;
	unsigned int i;

	seed[0] = iv[0];
	seed[1] = iv[1];
	seed[2] = iv[2];
	*keyIndex = iv[3] >> 6;
	if (table->keys[*keyIndex].isWep104) {
		length = 16;
		*isWep104 = 1;
	} else {
		length = 8;
		*isWep104 = 0;
	}
	for (i = 0; i < length - 3; i++)
		seed[3 + i] = table->keys[*keyIndex].key[i];
}

/* Before a fragment that has more behind it: wait while the pool is untouched. */
void wlanRxWaitForBuffer(int enabled, const WlanWirelessFrame *frame)
{
	if (!enabled || !(frame->frameControl[1] & WLAN_FC1_MORE_FRAG))
		return;
	while (osa_BufferClone(wlanPacketPool) == wlanPacketPool->blockCount)
		tx_thread_sleep(1);
}

void wlanCryptoWait(void)
{
	uint32_t delay = wlanCryptoOps.pollDelay;
	volatile uint32_t i;

	while (CRYPTO_STATUS & CRYPTO_STATUS_BUSY)
		for (i = 0; i < delay; i++)
			;
}

void wlanCryptoLoadKey(const uint32_t *config, const uint8_t *key)
{
	if (wlanCryptoLoadKeyOverride(config, key))
		return;
	CRYPTO_KEY(0) = le32(key);
	CRYPTO_KEY(1) = le32(key + 4);
	CRYPTO_KEY(2) = le32(key + 8);
	CRYPTO_KEY(3) = le32(key + 12);
	CRYPTO_CONFIG(0) = config[0];
	CRYPTO_CONFIG(1) = config[1];
	CRYPTO_MODE = 0;
}

void wlanCryptoKick(void)
{
	SOC_CONTROL &= ~SOC_CONTROL_CRYPTO_RESET;
	SOC_CONTROL |= SOC_CONTROL_CRYPTO_RESET;
	wlanCryptoOps.afterReset();
}

/*
 * The CCMP nonce and AAD addresses: the destination (address 1, or 3 to the
 * DS), the transmitter (address 2) and the source (address 2, 3 from the DS,
 * or 4 between two).
 */
void wlanCryptoLoadHeader(const WlanWirelessFrame *frame)
{
	const uint8_t *destination;
	const uint8_t *transmitter;
	const uint8_t *source;
	uint8_t flags;

	if (wlanCryptoLoadHeaderOverride(frame))
		return;
	flags = frame->frameControl[1];
	destination = (flags & WLAN_FC1_TO_DS) ? frame->address3 : frame->address1;
	transmitter = frame->address2;
	if (!(flags & WLAN_FC1_FROM_DS))
		source = transmitter;
	else if (flags & WLAN_FC1_TO_DS)
		source = frame->address4;
	else
		source = frame->address3;
	CRYPTO_SOURCE_LOW = le32(source);
	CRYPTO_SOURCE_HIGH = le16(source + 4);
	CRYPTO_DESTINATION_LOW = le32(destination);
	CRYPTO_DESTINATION_HIGH = le16(destination + 4);
	CRYPTO_TRANSMITTER_LOW = le32(transmitter);
	CRYPTO_TRANSMITTER_HIGH = le16(transmitter + 4);
}

static inline void wlanCryptoClearStatus(void)
{
	(void)CRYPTO_CLEAR1;
	(void)CRYPTO_CLEAR0;
	(void)CRYPTO_CLEAR2;
}

void wlanCcmpEncrypt(const WlanWirelessFrame *frame, uint8_t *output, uint32_t pnHigh,
		     uint16_t pnLow, uint32_t priority, const uint32_t *config, const uint8_t *key)
{
	uint32_t token;

	if (wlanCcmpEncryptOverride(frame, output, pnHigh, pnLow, priority, config, key))
		return;
	token = wlanCryptoOps.enter();
	wlanCryptoLoadKey(config, key);
	wlanCryptoWait();
	CRYPTO_MODE = 0;
	wlanCryptoClearStatus();
	CRYPTO_INPUT = (uint32_t)frame->body + 8;
	CRYPTO_LENGTH = le16(frame->bodyLength);
	CRYPTO_OUTPUT = (uint32_t)output;
	wlanCryptoLoadHeader(frame);
	CRYPTO_MIC_ADDRESS = priority;
	if (!(frame->frameControl[1] & WLAN_FC1_MORE_FRAG))
		CRYPTO_MODE = CRYPTO_MODE_LAST_FRAGMENT;
	CRYPTO_PN_LOW = pnHigh << 16 | pnLow;
	CRYPTO_PN_HIGH = pnHigh >> 16;
	CRYPTO_HEADER_LENGTH = 0x20;
	CRYPTO_MODE |= (frame->sequenceControl[0] & 0xf) ?
		       CRYPTO_MODE_FRAGMENT | CRYPTO_MODE_CCMP_ENCRYPT :
		       CRYPTO_MODE_FIRST_FRAGMENT | CRYPTO_MODE_CCMP_ENCRYPT;
	wlanCryptoOps.start();
	wlanCryptoWait();
	if (!(frame->frameControl[1] & WLAN_FC1_MORE_FRAG))
		wlanCryptoKick();
	wlanCryptoOps.leave(token);
}

/*
 * Decrypts in place. A fragment after the first continues from the state the
 * previous one left; one with more behind it saves the state for the next.
 * Returns the engine's error bits, 0 when the MIC checked.
 */
int wlanCcmpDecrypt(const WlanWirelessFrame *frame, uint8_t *data, uint32_t length,
		    uint32_t priority, const uint32_t *config, const uint8_t *key,
		    uint32_t *state0, uint32_t *state1, uint32_t *state2, uint32_t *state3,
		    uint32_t *state4)
{
	uint32_t token;
	uint32_t mode;
	int result = 0;

	if (wlanCcmpDecryptOverride(frame, data, length, priority, config, key, state0, state1,
				    state2, state3, state4, &result))
		return result;
	token = wlanCryptoOps.enter();
	wlanCryptoLoadKey(config, key);
	wlanCryptoWait();
	CRYPTO_MODE = 0;
	wlanCryptoClearStatus();
	CRYPTO_INPUT = (uint32_t)data;
	CRYPTO_LENGTH = length;
	CRYPTO_OUTPUT = (uint32_t)data;
	wlanCryptoLoadHeader(frame);
	CRYPTO_MIC_ADDRESS = priority;
	mode = (frame->frameControl[1] & WLAN_FC1_MORE_FRAG) ? 0 : CRYPTO_MODE_LAST_FRAGMENT;
	if (frame->sequenceControl[0] & 0xf) {
		CRYPTO_STATE_IN(0) = *state0;
		CRYPTO_STATE_IN(1) = *state1;
		CRYPTO_STATE_IN(2) = *state2;
		CRYPTO_STATE_IN(3) = *state3;
		CRYPTO_STATE_IN(4) = *state4;
		mode |= CRYPTO_MODE_FRAGMENT | CRYPTO_MODE_CCMP_DECRYPT;
	} else {
		mode |= CRYPTO_MODE_FIRST_FRAGMENT | CRYPTO_MODE_CCMP_DECRYPT;
	}
	CRYPTO_MODE = mode;
	wlanCryptoOps.start();
	wlanCryptoWait();
	result = CRYPTO_STATUS & CRYPTO_STATUS_CCMP_ERRORS;
	if (frame->frameControl[1] & WLAN_FC1_MORE_FRAG) {
		*state0 = CRYPTO_STATE_OUT0;
		*state1 = CRYPTO_STATE_OUT1;
		*state2 = CRYPTO_STATE_OUT2;
		*state3 = CRYPTO_STATE_OUT3;
		*state4 = CRYPTO_STATE_OUT4;
	}
	wlanCryptoKick();
	wlanCryptoOps.leave(token);
	return result;
}

static inline void wlanWepLoadSeed(const uint8_t *seed, uint8_t isWep104)
{
	CRYPTO_WEP_SEED(0) = le32(seed);
	CRYPTO_WEP_SEED(1) = le32(seed + 4);
	if (isWep104 == 1) {
		CRYPTO_WEP_SEED(2) = le32(seed + 8);
		CRYPTO_WEP_SEED(3) = le32(seed + 12);
	}
}

/* Writes the IV (seed bytes 0-2 and the key index) and the encrypted body behind it. */
void wlanWepEncrypt(uint8_t *output, const uint8_t *input, uint32_t length,
		    const uint8_t *seed, uint8_t isWep104, uint8_t keyIndex)
{
	uint32_t token;

	if (wlanWepEncryptOverride(output, input, length, seed, isWep104, keyIndex))
		return;
	token = wlanCryptoOps.enter();
	wlanCryptoWait();
	(void)CRYPTO_CLEAR1;
	CRYPTO_MODE = 0;
	wlanWepLoadSeed(seed, isWep104);
	output[0] = seed[0];
	output[1] = seed[1];
	output[2] = seed[2];
	output[3] = keyIndex << 6;
	CRYPTO_HEADER_LENGTH = 0;
	CRYPTO_INPUT = (uint32_t)input;
	CRYPTO_OUTPUT = (uint32_t)(output + 4);
	CRYPTO_LENGTH = length;
	CRYPTO_MODE = ((isWep104 << CRYPTO_MODE_WEP104_SHIFT) + CRYPTO_MODE_WEP) |
		      CRYPTO_MODE_WEP_ENCRYPT;
	wlanCryptoOps.start();
	wlanCryptoWait();
	wlanCryptoKick();
	wlanCryptoOps.leave(token);
}

/* Returns 1 when the ICV checked, 0 when it did not. */
int wlanWepDecrypt(uint8_t *output, const uint8_t *input, uint32_t length,
		   const uint8_t *seed, uint8_t isWep104, WlanWirelessFrame *frame)
{
	uint32_t token;
	uint32_t mode;
	int result;

	if (wlanWepDecryptOverride(output, input, length, seed, isWep104, frame, &result))
		return result;
	token = wlanCryptoOps.enter();
	wlanCryptoWait();
	(void)CRYPTO_CLEAR1;
	CRYPTO_MODE = 0;
	wlanWepLoadSeed(seed, isWep104);
	CRYPTO_INPUT = (uint32_t)input;
	CRYPTO_OUTPUT = (uint32_t)output;
	CRYPTO_LENGTH = length;
	mode = (isWep104 << CRYPTO_MODE_WEP104_SHIFT) + CRYPTO_MODE_WEP;
	if (!(frame->frameControl[1] & WLAN_FC1_MORE_FRAG))
		mode |= CRYPTO_MODE_LAST_FRAGMENT;
	CRYPTO_MODE = mode;
	wlanCryptoOps.start();
	wlanCryptoWait();
	result = !(CRYPTO_STATUS & CRYPTO_STATUS_WEP_ICV_ERROR);
	wlanCryptoKick();
	wlanCryptoOps.leave(token);
	return result;
}
