/*
 * The host's handshake: host command 0x00a0 runs a challenge and response with the host
 * (handshakeCommand), and a mode it passes is unlocked: its word is set to HANDSHAKE_UNLOCKED
 * and its unlock handler called; a failed one is locked again. Mode 0x0b lets the host's
 * requests through, mode 0x0f leaves a session key in handshakeKey.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "aes.h"
#include "authkeys.h"
#include "chip.h"
#include "ernie.h"
#include "handshake.h"
#include "kernel.h"
#include "log.h"
#include "snvs.h"
#include "state.h"
#include "sysinfo.h"
#include "util.h"

#define HANDSHAKE_MODES		2

/* a mode: its number, its word, and what unlocking and locking it do */
struct HandshakeMode {
	unsigned char	mode;
	unsigned int	*unlocked;
	int		(*unlock)(void);
	int		(*lock)(void);
};

struct HandshakeCrypto {
	unsigned char mode;
	const unsigned char *firstKey;
	const unsigned char *secondKey;
	const unsigned char *sessionKey;
	const unsigned char *firstCheck;
	const unsigned char *secondCheck;
};

/* the exchange's packet: the challenge and response go out and come back in it */
struct HandshakePacket {
	unsigned char	magic;			/* 0x30 */
	unsigned char	step;			/* 1-5 */
	unsigned char	error;
	unsigned char	mode;
	unsigned char	header[4];
	unsigned char	challenge[8];
	unsigned char	response[8];
	unsigned char	key[16];
};

/* the exchange's state */
struct Handshake {
	unsigned char	state;			/* 0 none, 1-3 the steps, 0xff failed */
	unsigned char	mode;			/* the mode asked for, 0xff none */
	unsigned char	challenge[8];
	unsigned char	response[8];
	unsigned char	session[16];
};

unsigned char handshakeKey[16];	/* mode 0x0f's session key */

static __near struct HandshakePacket handshakePacket;
static __near struct Handshake handshake;
static __near unsigned int handshakeUnlocked[HANDSHAKE_MODES];


static int handshakeUnlock11(void);
static int handshakeLockHandler11(void);
static int handshakeUnlock15(void);
static int handshakeLockHandler15(void);

static const unsigned char mode11Check[16];
static const unsigned char mode11Key1[16];
static const unsigned char mode15Check1[16];
static const unsigned char mode15Check2[16];
static const unsigned char mode15Key1[16];
static const unsigned char mode15Key2[16];
static const unsigned char mode15Key3[16];

/* (a mode not found reads past the table, as Sony's does) */
static const struct HandshakeCrypto handshakeCrypto[HANDSHAKE_MODES] = {
	{ 0x0b, mode11Key1, (const unsigned char *)randomKey, NULL, mode11Check, NULL },
	{ 0x0f, mode15Key1, mode15Key2, mode15Key3, mode15Check1, mode15Check2 },
};

static const struct HandshakeMode handshakeModes[HANDSHAKE_MODES] = {
	{ 0x0b, (unsigned int *)&handshakeUnlocked[0], handshakeUnlock11, handshakeLockHandler11 },
	{ 0x0f, (unsigned int *)&handshakeUnlocked[1], handshakeUnlock15, handshakeLockHandler15 },
};

/* the packet header the serial side sends (unused here), then the IV and the keys */
static const unsigned char handshakeHeader[4] = {
	0x01, 0x00, 0x00, 0x00
};

const unsigned char handshakeIv[16] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char mode11Check[16] = {
	0xcf, 0x2e, 0x93, 0xe9, 0xf9, 0x4e, 0x28, 0xcc, 0xa4, 0x80, 0x26, 0x13, 0x4c, 0x7c, 0x77, 0xce
};
static const unsigned char mode11Key1[16] = {
	0xbb, 0x64, 0x47, 0x21, 0xcb, 0x4c, 0x55, 0x07, 0x2e, 0x83, 0x17, 0x7b, 0xeb, 0x3b, 0xbe, 0xe9
};
/* mode 0x0b's second key, also the random generator's key */
const unsigned long randomKey[4] = {
	0xe06e6bdcUL, 0x0edf57f4UL, 0x5e1cad7bUL, 0x7f0238a3UL
};
static const unsigned char mode15Check1[16] = {
	0xc8, 0x6b, 0x51, 0xfb, 0x01, 0x9a, 0x20, 0x7f, 0x32, 0x11, 0x8e, 0x55, 0x46, 0x2d, 0x50, 0x08
};
static const unsigned char mode15Check2[16] = {
	0xb0, 0x11, 0x03, 0xb0, 0x62, 0x38, 0x32, 0xd6, 0x25, 0x40, 0xb5, 0x63, 0x33, 0xd6, 0xe1, 0x1d
};
static const unsigned char mode15Key1[16] = {
	0x50, 0xe4, 0xc3, 0xa7, 0x72, 0x64, 0x16, 0x7c, 0x40, 0x9c, 0x72, 0xa9, 0xb5, 0x7a, 0x86, 0x09
};
static const unsigned char mode15Key2[16] = {
	0x9e, 0x34, 0x08, 0x7c, 0x48, 0x98, 0x5b, 0x4b, 0x35, 0x1a, 0x63, 0x57, 0x2d, 0x9b, 0x48, 0x1b
};
static const unsigned char mode15Key3[16] = {
	0xeb, 0xe3, 0x46, 0x0d, 0x84, 0xa4, 0x17, 0x54, 0xac, 0x44, 0x13, 0x68, 0xcf, 0x02, 0x00, 0xd8
};

/* keepKey 0 (a wakeup without the factor's notify bit) also clears mode 0x0f's key */
int handshakeInit(unsigned char keepKey)
{
	memset_n(&handshakePacket, 0, sizeof(handshakePacket));
	memset_n(&handshake, 0, sizeof(handshake));
	handshake.state = 0;
	handshake.mode = 0xff;
	handshakeLock11();
	if (!keepKey)
		handshakeLock15();
	return 0;
}

/* state 0 unlocks the mode, anything else locks it; the handler's result */
unsigned char handshakeSetMode(unsigned char state, unsigned char mode)
{
	unsigned char i;
	unsigned char result;

	result = 0;
	i = HANDSHAKE_MODES;
	while (i-- != 0) {
		if (handshakeModes[i].mode != mode)
			continue;
		if (state == 0) {
			*handshakeModes[i].unlocked = HANDSHAKE_UNLOCKED;
			handshake.state = 0;
			handshake.mode = 0xff;
			if (handshakeModes[i].unlock != NULL)
				result = (unsigned char)handshakeModes[i].unlock();
		} else {
			*handshakeModes[i].unlocked = HANDSHAKE_LOCKED;
			if (handshakeModes[i].lock != NULL)
				result = (unsigned char)handshakeModes[i].lock();
		}
		break;
	}
	memset_n(&handshakePacket, 0, sizeof(handshakePacket));
	return result;
}

/* Process the host's challenge and response and unlock the selected mode. */
unsigned char handshakeCommand(unsigned char *data)
{
	struct AesKey key;
	VP block;
	int poolResult;
	unsigned char modeIndex;
#if RANDOM_SEEDED
	/* The original passes these stack bytes without initializing them first. */
	unsigned char nonce[16];
	unsigned char seed[4];
#else
	unsigned long seed;
#endif

	memcpy(&handshakePacket, data, sizeof(handshakePacket));
	switch (handshake.state) {
	case 0:
		handshake.mode = handshakePacket.mode;
#if RANDOM_SEEDED
		randomGenerate(seed, nonce);
		memcpy(handshake.challenge, nonce, 8);
#else
		getIdRecordsLength();
		seed = TCR10;
		randomGenerate(seed, (unsigned char *)handshake.challenge, 8);
#endif
		if (handshake.mode == 0x0f) {
			handshakeLock15();
			snvsReset();
		}
		/* fall through */
	case 1:
		if (handshakePacket.magic != 0x30 || handshakePacket.step != 0 ||
		    handshakePacket.error != 0 || handshakePacket.mode != handshake.mode) {
			handshake.state = 0xff;
			handshakePacket.error = 1;
		} else {
			handshake.state = 2;
			handshakePacket.error = 0;
		}
		handshakePacket.magic = 0x30;
		handshakePacket.step = 1;
		handshakePacket.mode = handshake.mode;
		memset(handshakePacket.header, 0, 0x24);
		memcpy(handshakePacket.challenge, handshake.challenge, 8);
		memcpy(data, &handshakePacket, sizeof(handshakePacket));
		break;
	case 2:
		handshakePacket.error = 0;
		if (handshakePacket.magic != 0x30 || handshakePacket.step != 2 ||
		    handshakePacket.error != 0 || handshakePacket.mode != handshake.mode) {
			handshake.state = 0xff;
			handshakePacket.error = 1;
		}
		poolResult = pget_mpf(2, &block);
		modeIndex = HANDSHAKE_MODES;
		while (modeIndex-- != 0) {
			if (handshakeCrypto[modeIndex].mode == handshake.mode)
				break;
		}
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeCrypto[modeIndex].firstKey,
					(struct AesKey __near *)&key);
			aesCbcDecrypt(handshakePacket.challenge, (unsigned char __near *)(data + 8),
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		} else {
			handshake.state = 0xff;
		}
		memcpy(handshakePacket.challenge, (unsigned char __near *)(data + 8), 0x20);
		if (memcmp(handshakePacket.response, handshake.challenge, 8) != 0 &&
		    handshakePacket.error == 0) {
			handshake.state = 0xff;
			handshakePacket.error = 2;
		}
		if (memcmp(handshakePacket.key, handshakeCrypto[modeIndex].firstCheck, 0x10) != 0 &&
		    handshakePacket.error == 0) {
			handshake.state = 0xff;
			handshakePacket.error = 3;
		}
		memcpy(handshake.response, handshakePacket.challenge, 8);
		handshakePacket.magic = 0x30;
		handshakePacket.step = 3;
		handshakePacket.mode = handshake.mode;
		memset(handshakePacket.header, 0, 4);
		memcpy(handshakePacket.response, handshake.response, 8);
		memcpy(handshakePacket.challenge, handshake.challenge, 8);
		memset(handshakePacket.key, 0, 0x10);
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeCrypto[modeIndex].secondKey,
					(struct AesKey __near *)&key);
			aesCbcEncrypt(handshakePacket.challenge, handshakePacket.challenge,
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		} else {
			handshake.state = 0xff;
		}
		memcpy(data, &handshakePacket, sizeof(handshakePacket));
		rel_mpf(2, block);
		if (handshakePacket.error == 0) {
			if (handshake.mode == 0x0f)
				handshake.state = 3;
			else
				handshake.state = 0;
		}
		break;
	case 3:
		handshakePacket.error = 0;
		if (handshakePacket.magic != 0x30 || handshakePacket.step != 4 ||
		    handshakePacket.error != 0 || handshakePacket.mode != handshake.mode) {
			handshake.state = 0xff;
			handshakePacket.error = 1;
		}
		memcpy(handshake.session, handshake.challenge, 8);
		memcpy(handshake.session + 8, handshake.response, 8);
		poolResult = pget_mpf(2, &block);
		modeIndex = HANDSHAKE_MODES;
		while (modeIndex-- != 0) {
			if (handshakeCrypto[modeIndex].mode == handshake.mode)
				break;
		}
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeCrypto[modeIndex].sessionKey,
					(struct AesKey __near *)&key);
			aesCbcEncrypt(handshake.session, handshake.session, (struct AesKey __near *)&key,
				      handshakeIv, 0x10);
			memcpy(handshakeKey, handshake.session, 0x10);
		} else {
			handshake.state = 0xff;
		}
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeKey, (struct AesKey __near *)&key);
			aesCbcDecrypt(handshakePacket.challenge, (unsigned char __near *)(data + 8),
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		} else {
			handshake.state = 0xff;
		}
		memcpy(handshakePacket.challenge, (unsigned char __near *)(data + 8), 0x20);
		if (memcmp(handshakePacket.challenge, handshakeCrypto[modeIndex].secondCheck, 0x10) != 0 &&
		    handshakePacket.error == 0) {
			handshake.state = 0xff;
			handshakePacket.error = 3;
		}
		handshakePacket.magic = 0x30;
		handshakePacket.step = 5;
		handshakePacket.mode = handshake.mode;
		memset(handshakePacket.header, 0, 0x24);
		memcpy(handshakePacket.challenge, handshakeCrypto[modeIndex].secondCheck, 0x10);
		if (poolResult == E_OK)
			aesCbcEncrypt(handshakePacket.challenge, handshakePacket.challenge,
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		rel_mpf(2, block);
		memcpy(data, &handshakePacket, sizeof(handshakePacket));
		if (handshakePacket.error == 0)
			handshake.state = 0;
		break;
	default:
		break;
	}
	if (handshake.state == 0xff) {
		handshakeSetMode(handshake.state, handshake.mode);
		return 0;
	} else if (handshake.state == 0) {
		handshakeSetMode(handshake.state, handshake.mode);
		return 0;
	}
	return 0;
}

unsigned char handshakeState(void)
{
	return handshake.state;
}

unsigned char handshakeUnlocked11(void)
{
	return handshakeUnlocked[0] == HANDSHAKE_UNLOCKED;
}

int handshakeLock11(void)
{
	handshakeUnlocked[0] = HANDSHAKE_LOCKED;
	return 0;
}

static int handshakeUnlock11(void)
{
	return 0;
}

static int handshakeLockHandler11(void)
{
	logEvent(0x16);
	return 0;
}

int handshakeUnlocked15(void)
{
	return handshakeUnlocked[1] == HANDSHAKE_UNLOCKED;
}

int handshakeLock15(void)
{
	memset_n((unsigned char __near *)handshakeKey, 0, sizeof(handshakeKey));
	handshakeUnlocked[1] = HANDSHAKE_LOCKED;
	return 0;
}

static int handshakeUnlock15(void)
{
	return 0;
}

static int handshakeLockHandler15(void)
{
	return 0;
}

void handshakeUnused(unsigned long unused)
{
}

/* a host request refused while mode 0x0b is locked: transition 0x36 */
unsigned char handshakeRefuse(void)
{
	return (unsigned char)stateTransition(0x36);
}
