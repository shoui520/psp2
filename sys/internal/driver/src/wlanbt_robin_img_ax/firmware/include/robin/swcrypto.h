/*
 * Frames the firmware encrypts and decrypts itself (CCMP with the CCM
 * engine, WAPI with its own block; robin/ccm.h, robin/wapi.h). The engine
 * takes one frame at a time. Frames that arrive while it is busy wait in two
 * rings: the buffers to send (29) and the received frames with the status
 * and key they came with (84). When the engine finishes, sysServiceWlanPackets()
 * delivers the frame and starts the next one. The names are inferred from use.
 */
#ifndef ROBIN_SWCRYPTO_H
#define ROBIN_SWCRYPTO_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/wlanrx.h"

/* The original calls these; they are not folded into their callers. */
#define WLAN_CRYPTO_CALL		__attribute__((noinline))

#define WLAN_CRYPTO_TX_RING_SIZE	29
#define WLAN_CRYPTO_RX_RING_SIZE	84

/* Counters in wlanCryptoCounters. */
enum {
	WLAN_CRYPTO_BUSY,		/* the engine has a frame */
	WLAN_CRYPTO_FAILED_PEER,	/* decryptions that failed for a frame with a connection */
	WLAN_CRYPTO_FRAMES,		/* frames the engine finished receiving */
	WLAN_CRYPTO_FAILED,		/* decryptions that failed */
};

/* A ring of frames to send; tail is the last entry written, head the next to read. */
struct WlanCryptoTxRing {
	int tail;
	int head;
	OsaBuffer *entries[WLAN_CRYPTO_TX_RING_SIZE];
};

_Static_assert(sizeof(struct WlanCryptoTxRing) == 124, "send ring");

/* Where the frame the engine is receiving waits: its buffer and the hardware status it came with. */
struct WlanCryptoScratch {
	OsaBuffer *buffer;
	uint32_t reserved;
	struct WlanRxStatus status;
};

_Static_assert(sizeof(struct WlanCryptoScratch) == 76, "receive scratch");

/*
 * A received frame that waits for the engine: the status bytes 4..27 it came
 * with (the receive path needs the QoS control, kept over bytes 12 and 13,
 * and the PHY mode bits, over byte 19) and the pointer to its header.
 */
struct WlanCryptoHeldFrame {
	OsaBuffer *buffer;
	cipher_key_t *key;
	uint8_t status[24];
	uint32_t frame;
};

_Static_assert(sizeof(struct WlanCryptoHeldFrame) == 36, "held frame");

struct WlanCryptoRxRing {
	int tail;
	int head;
	struct WlanCryptoScratch *scratch;
	struct WlanCryptoHeldFrame entries[WLAN_CRYPTO_RX_RING_SIZE];
};

/* The frame being sent and received (0x04000f30..0x04000f50). */
extern uint32_t wlanCryptoTxActive;
extern uint32_t wlanCryptoRxActive;
extern uint8_t *wlanCryptoTxHeader;
extern cipher_key_t *wlanCryptoRxKey;
extern OsaBuffer *wlanCryptoTxBuffer;
extern uint32_t wlanCryptoCounters[4];
extern struct WlanCryptoTxRing wlanCryptoTxRing;
extern struct WlanCryptoRxRing wlanCryptoRxRing;
extern struct WlanCryptoScratch wlanCryptoScratch;

unsigned int wlanCryptoCount(unsigned int index);			/* 0x1ebe8 */
WLAN_CRYPTO_CALL int wlanCryptoAcquire(unsigned int index);				/* 0x1ebf0 */
void wlanCryptoRingsInit(struct WlanCryptoTxRing *tx, struct WlanCryptoRxRing *rx);	/* 0x1ec14 */
WLAN_CRYPTO_CALL struct WlanCryptoHeldFrame *wlanCryptoRxPop(struct WlanCryptoRxRing *ring);	/* 0x1ec20 */
WLAN_CRYPTO_CALL int wlanCryptoTxPush(OsaBuffer *buffer, struct WlanCryptoTxRing *ring);	/* 0x1ec4e */
WLAN_CRYPTO_CALL OsaBuffer *wlanCryptoTxPop(struct WlanCryptoTxRing *ring);		/* 0x1ec86 */
void wlanCryptoTxDrain(struct WlanCryptoTxRing *ring);			/* 0x1ecae */
void wlanCryptoRxDrain(struct WlanCryptoRxRing *ring);			/* 0x1ecc6 */
void wlanCryptoFlushConnection(struct WlanConnection *connection);	/* 0x1ecde */
void wlanCryptoRetargetFrames(unsigned int from, uint8_t to, unsigned int tid);	/* 0x1ee16 */
void wlanCryptoMoveFrames(unsigned int from, unsigned int to, unsigned int tid,
			  const void *address);				/* 0x1ee52 */
int wlanCryptoSend(OsaBuffer *buffer);					/* 0x1eea6 */
void wlanCryptoReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key);	/* 0x1ef0c */
int wlanCryptoRxPush(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key,
		     struct WlanCryptoRxRing *ring);			/* 0x1f188 */
WLAN_CRYPTO_CALL struct WlanCryptoHeldFrame *wlanCryptoRxPeek(struct WlanCryptoRxRing *ring);	/* 0x1f222 */
void wlanCryptoReleaseBuffer(OsaBuffer *buffer);			/* 0x1f240 */
int wlanCryptoAdjust(struct WlanCryptoTxRing *tx, struct WlanCryptoRxRing *rx, OsaBuffer **active);	/* 0x1f254 */
int wlanCryptoSendTable(OsaBuffer *buffer);				/* 0x4221a */
int wlanCryptoReceiveTable(OsaBuffer **buffer, struct WlanRxStatus *status, cipher_key_t *key);	/* 0x4226e */
int wlanCcmpSend(uint8_t *frame, int unused, unsigned int priority, cipher_key_t *key, unsigned int keyId);	/* 0x4218c */
int wlanCcmpReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key);	/* 0x422b0 */

/* Holds the SoC's event paths while the crypto engine or a host command works (bits 8, 9, 14 and 26 of SOC_EVENT_CONTROL). */
WLAN_CRYPTO_CALL void socEventsHold(void);						/* 0x26c40 */
WLAN_CRYPTO_CALL unsigned int socEventsRelease(void);					/* 0x26c4c */

/* Not converted. */
int macMgmtPostRelease(void);						/* 0x34852 */
int macMgmtPostReceived(void);						/* 0x3483e */
int mdevWlanFormatState(OsaBuffer *buffer, struct WlanRxStatus *status);	/* 0x13d2 */
int mdevWlanSyncState(OsaBuffer *buffer, struct WlanRxStatus *status);	/* 0xba4 */

#endif /* ROBIN_SWCRYPTO_H */
