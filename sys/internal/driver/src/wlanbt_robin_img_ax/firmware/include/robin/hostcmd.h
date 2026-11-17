/*
 * The host's commands (0x18a0e..0x1ae88); see wlan/hostcmd.c.
 */
#ifndef ROBIN_HOSTCMD_H
#define ROBIN_HOSTCMD_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

int hostCmdProcess(void);
int hostCmdRespond(struct WlanConnection *connection, int (*release)(OsaBuffer *buffer));
void hostCmdUnlock(void);
void *macTxNotifyBufferBase(unsigned int kind);
void macTxNotifyPrepareNothing(void);
int hostCmdIsNodeSet(const uint8_t *command);
int hostCmd0003Caps(void);							/* 0x1b820 */
void hostCmd0003CapsSet(uint32_t caps);						/* 0x1b82a */
unsigned int hostCmd0020Set(unsigned int secondary, unsigned int antenna);	/* 0x262c0, wlan/hostcmdext.c */
uint16_t hostCmdNodeGet(void);
uint16_t hostCmdNode(const uint8_t *body);
unsigned int hostCmdCurrentCode(void);

/*
 * HostCmd 0x78 (Marvell's HostCmd_CMD_802_11_CRYPTO): runs one cipher over the command's own
 * data, to test the firmware's ciphers (sqram/crypto.c). The body's 16-bit fields are little
 * endian and unaligned. RC4, AES and the AES key wrap share one layout; AES-CCM and WAPI
 * (SMS4) have their own.
 */
#define HOSTCMD_CRYPTO_DECRYPT		0
#define HOSTCMD_CRYPTO_ENCRYPT		1

enum HostCmdCryptoAlgorithm {
	HOSTCMD_CRYPTO_RC4 = 1,
	HOSTCMD_CRYPTO_AES,
	HOSTCMD_CRYPTO_AES_KEY_WRAP,
	HOSTCMD_CRYPTO_AES_CCM,
	HOSTCMD_CRYPTO_WAPI,
	HOSTCMD_CRYPTO_ALGORITHMS
};

/* The data, as a TLV. */
struct HostCmdCryptoData {
	uint16_t type;
	uint16_t length;
	uint8_t data[];
} __attribute__((packed));

struct HostCmdCrypto {
	uint16_t encdec;
	uint16_t algorithm;
	uint16_t keyIvLength;		/* RC4: the IV; key wrap: the initial value (8) */
	uint8_t keyIv[32];
	uint16_t keyLength;
	uint8_t key[32];
	struct HostCmdCryptoData data;
} __attribute__((packed));

_Static_assert(__builtin_offsetof(struct HostCmdCrypto, data) == 0x48, "crypto command");

struct HostCmdCryptoAesCcm {
	uint16_t encdec;
	uint16_t algorithm;
	uint16_t keyLength;
	uint8_t key[32];
	uint16_t nonceLength;		/* 7 .. 13 */
	uint8_t nonce[14];
	uint16_t aadLength;		/* up to 30 */
	uint8_t aad[32];
	struct HostCmdCryptoData data;	/* up to 40 bytes */
} __attribute__((packed));

_Static_assert(__builtin_offsetof(struct HostCmdCryptoAesCcm, data) == 0x58, "AES-CCM command");

struct HostCmdCryptoWapi {
	uint16_t encdec;
	uint16_t algorithm;
	uint16_t keyLength;		/* 32: the data key, then the MIC key */
	uint8_t key[32];
	uint16_t packetNumberLength;	/* 16 */
	uint8_t packetNumber[16];
	uint16_t aadLength;		/* 32, or 48 for QoS data */
	uint8_t aad[48];
	uint16_t dataLength;
} __attribute__((packed));

_Static_assert(__builtin_offsetof(struct HostCmdCryptoWapi, dataLength) == 0x6a, "WAPI command");

/* What hostCmd0078Parse() hands the handler: the direction and the command's body. */
struct HostCmdCryptoRequest {
	uint8_t encdec;
	uint8_t *body;
};

typedef void (*HostCmdCryptoHandler)(struct HostCmdCryptoRequest *request, int *failed);

struct HostCmdCryptoEntry {
	uint32_t encdec;
	HostCmdCryptoHandler handler;
};

extern struct HostCmdCryptoEntry hostCmd0078Handlers[2][HOSTCMD_CRYPTO_ALGORITHMS];	/* SQRAM 0xc0011f34 */

/* Bits of hostCmd0078Parse()'s status: what in the command is not allowed. */
#define HOSTCMD_CRYPTO_BAD_ALGORITHM	0x0002u
#define HOSTCMD_CRYPTO_BAD_KEY		0x0004u
#define HOSTCMD_CRYPTO_BAD_DIRECTION	0x0040u
#define HOSTCMD_CRYPTO_BAD_IV		0x0100u
#define HOSTCMD_CRYPTO_BAD_RC4_LENGTH	0x0200u
#define HOSTCMD_CRYPTO_BAD_DATA		0x0400u
#define HOSTCMD_CRYPTO_BAD_NONCE	0x0800u
#define HOSTCMD_CRYPTO_BAD_AAD		0x1000u

/* The crypto test's hooks (sqram/hooks.c, SQRAM 0xc000e21c..0xc000e274). */
int cryptoTestEnter(void);
void cryptoTestLeave(int lock);
void hostCmd0078HooksInit(void);

int hostCmd0078Parse(uint32_t *status, uint8_t *body, struct HostCmdCryptoRequest *request,
		     int8_t *lengthChange);						/* SQRAM 0xc00078d6 */

int hostCmdConnectionChanged(struct WlanConnection *connection);	/* 0x18c0a */

#endif /* ROBIN_HOSTCMD_H */
