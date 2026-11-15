/*
 * The WEP/CCMP engine (0x4fb0..0x5614): the MAC's cipher block at 0x9000b000
 * and the code that loads it with keys, header addresses and buffers. No
 * Marvell source names the registers; the names say what the firmware writes.
 */
#ifndef ROBIN_WEP_H
#define ROBIN_WEP_H

#include <stdint.h>

#include "robin/soc.h"
#include "robin/wlan.h"

#define CRYPTO_BASE			0x9000b000u
#define CRYPTO_REG(offset)		REG32(CRYPTO_BASE + (offset))

#define CRYPTO_KEY(n)			CRYPTO_REG(0x000 + 4u * (n))	/* 128-bit key */
#define CRYPTO_TRANSMITTER_LOW		CRYPTO_REG(0x010)		/* address 2 */
#define CRYPTO_TRANSMITTER_HIGH		CRYPTO_REG(0x014)
#define CRYPTO_HEADER_LENGTH		CRYPTO_REG(0x018)		/* 0x20 for CCMP, 0 for WEP */
#define CRYPTO_PN_LOW			CRYPTO_REG(0x01c)
#define CRYPTO_PN_HIGH			CRYPTO_REG(0x020)
#define CRYPTO_DESTINATION_LOW		CRYPTO_REG(0x024)
#define CRYPTO_DESTINATION_HIGH		CRYPTO_REG(0x028)
#define CRYPTO_SOURCE_LOW		CRYPTO_REG(0x02c)
#define CRYPTO_SOURCE_HIGH		CRYPTO_REG(0x030)
#define CRYPTO_MIC_ADDRESS		CRYPTO_REG(0x034)	/* the TID, written by wlanCcmpEncrypt() and wlanCcmpDecrypt() */
#define CRYPTO_INPUT			CRYPTO_REG(0x038)
#define CRYPTO_LENGTH			CRYPTO_REG(0x03c)
#define CRYPTO_OUTPUT			CRYPTO_REG(0x040)
/* CCMP state carried between the fragments of one MSDU. */
#define CRYPTO_STATE_OUT0		CRYPTO_REG(0x090)
#define CRYPTO_STATE_OUT1		CRYPTO_REG(0x094)
#define CRYPTO_STATE_IN(n)		CRYPTO_REG(0x098 + 4u * (n))
#define CRYPTO_STATE_OUT2		CRYPTO_REG(0x0ac)
#define CRYPTO_STATE_OUT3		CRYPTO_REG(0x0b0)
#define CRYPTO_STATE_OUT4		CRYPTO_REG(0x0b4)
#define CRYPTO_MODE			CRYPTO_REG(0x100)
#define CRYPTO_CONFIG(n)		CRYPTO_REG(0x104 + 4u * (n))
#define CRYPTO_WEP_SEED(n)		CRYPTO_REG(0x118 + 4u * (n))
#define CRYPTO_CLEAR0			CRYPTO_REG(0x128)		/* read to clear */
#define CRYPTO_CLEAR1			CRYPTO_REG(0x12c)
#define CRYPTO_CLEAR2			CRYPTO_REG(0x130)
#define CRYPTO_STATUS			CRYPTO_REG(0x134)

#define CRYPTO_STATUS_BUSY		0x01
#define CRYPTO_STATUS_WEP_ICV_ERROR	0x04
#define CRYPTO_STATUS_CCMP_ERRORS	0x0e

#define CRYPTO_MODE_WEP			0x00001
#define CRYPTO_MODE_WEP_ENCRYPT		0x00004
#define CRYPTO_MODE_LAST_FRAGMENT	0x00008
#define CRYPTO_MODE_CCMP_DECRYPT	0x00043
#define CRYPTO_MODE_CCMP_ENCRYPT	0x00047
#define CRYPTO_MODE_FRAGMENT		0x20000	/* not the first fragment */
#define CRYPTO_MODE_FIRST_FRAGMENT	0x30000
#define CRYPTO_MODE_WEP104_SHIFT	6


/* Set up at start-up (0x31156); zero until then. */
typedef struct WlanCryptoOps {
	uint32_t (*enter)(void);	/* returns the token leave() takes */
	void (*leave)(uint32_t token);
	void (*afterReset)(void);
	void (*start)(void);
	uint32_t pollDelay;		/* loops between status reads */
} WlanCryptoOps;

_Static_assert(sizeof(WlanCryptoOps) == 0x14, "crypto operations");

/* One WEP default key, as the host configured it (15 bytes, packed in fours). */
typedef struct WlanWepKey {
	uint8_t isWep104;
	uint8_t key[13];
	uint8_t reserved;
} WlanWepKey;

/*
 * The default keys as the host configured them (the key TLVs of the 802.11
 * key command, wlan/hostcmdtlv.c): each entry names its own index and type.
 */
typedef struct __attribute__((packed)) WlanWepHostKey {
	uint8_t index;
	uint8_t isWep104;
	uint8_t key[13];
} WlanWepHostKey;

/* The four keys and which of them frames are sent with. */
typedef struct __attribute__((packed)) WlanWepHostKeys {
	WlanWepHostKey key[4];
	uint8_t defaultKey;		/* the index of the key frames are sent with */
} WlanWepHostKeys;

typedef struct __attribute__((packed)) WlanWepHostConfig {
	uint8_t reserved0[8];
	uint16_t keyLength;		/* 5 or 13: the length of the last key set */
	uint8_t flags;			/* bit 0: keys were set */
	uint8_t keyFlags;		/* bit 3: the last key has 104 bits */
	WlanWepHostKeys keys;
} WlanWepHostConfig;

_Static_assert(sizeof(WlanWepHostKey) == 15, "host WEP key");
_Static_assert(offsetof(WlanWepHostConfig, keys.defaultKey) == 72, "host WEP default key");

typedef struct WlanWepKeyTable {
	uint8_t reserved[13];
	WlanWepKey keys[4];
} WlanWepKeyTable;

_Static_assert(sizeof(WlanWepKey) == 15, "WEP key");
_Static_assert(sizeof(WlanWepKeyTable) == 73, "WEP key table");

extern WlanCryptoOps wlanCryptoOps;

void wlanWepHostKeySet(WlanWepHostConfig *config, WlanWepHostKeys *keys, const uint8_t *tlv,
		       int keyType);	/* 0x2aad8 */

void wlanWepBuildSeed(const uint8_t *iv, const WlanWepKeyTable *table, uint8_t *seed,
		      uint8_t *isWep104, uint8_t *keyIndex);
void wlanRxWaitForBuffer(int enabled, const WlanWirelessFrame *frame);
void wlanCryptoWait(void);
void wlanCryptoLoadKey(const uint32_t *config, const uint8_t *key);
void wlanCryptoKick(void);
void wlanCryptoLoadHeader(const WlanWirelessFrame *frame);
void wlanCcmpEncrypt(const WlanWirelessFrame *frame, uint8_t *output, uint32_t pnHigh,
		     uint16_t pnLow, uint32_t priority, const uint32_t *config, const uint8_t *key);
int wlanCcmpDecrypt(const WlanWirelessFrame *frame, uint8_t *data, uint32_t length,
		    uint32_t priority, const uint32_t *config, const uint8_t *key,
		    uint32_t *state0, uint32_t *state1, uint32_t *state2, uint32_t *state3,
		    uint32_t *state4);
void wlanWepEncrypt(uint8_t *output, const uint8_t *input, uint32_t length,
		    const uint8_t *seed, uint8_t isWep104, uint8_t keyIndex);
int wlanWepDecrypt(uint8_t *output, const uint8_t *input, uint32_t length,
		   const uint8_t *seed, uint8_t isWep104, WlanWirelessFrame *frame);

#endif /* ROBIN_WEP_H */
