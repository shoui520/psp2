/*
 * The host's handshake, as the updater has it: host command 0x00a0 runs a challenge and
 * response with the host (handshakeCommand), and mode 0x0b, the only one, is unlocked by it
 * or by the application starting the updater (handshakeSetUnlocked): its word is set to
 * HANDSHAKE_UNLOCKED. A failed exchange locks it again.
 */
#include <stddef.h>
#include <string.h>

#include "aes.h"
#include "authkeys.h"
#include "state.h"
#include "updater.h"
#include "util.h"

#define HANDSHAKE_MODES		1

/* a mode: its number, its word, and what unlocking and locking it do */
struct HandshakeMode {
	unsigned char	mode;
	unsigned int	*unlocked;
	int		(*unlock)(void);
	int		(*lock)(void);
};

/* the exchange's packet: the challenge and response go out and come back in it */
struct HandshakePacket {
	unsigned char	magic;			/* 0x30 */
	unsigned char	step;			/* 1-3 */
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
};

int handshakeUnlock11(void);
int handshakeLockHandler11(void);
void handshakeDump(const char *name, unsigned char *data);
int handshakeGetRoundKeys(unsigned long __far * __far *w);

__near struct HandshakePacket handshakePacket;
__near struct Handshake handshake;
__near unsigned int handshakeUnlocked;
__near unsigned long handshakeRoundKeys[44];	/* the exchange's AES-128 key schedule */

/* in the keys object, authkeys.c */
extern const unsigned char mode11Check[16];
extern const unsigned char mode11Key1[16];

/* the factory handshake key, expanded (the updater keeps it here, before the modes' tables) */
const unsigned long factoryRoundKeys[60] = {
	FACTORY_ROUND_KEYS
};

/* a mode's keys: the challenge's, the response's, and the check of the challenge's key */
struct HandshakeCrypto {
	unsigned char mode;
	const unsigned char *firstKey;
	const unsigned char *secondKey;
	const unsigned char *firstCheck;
};

static const struct HandshakeCrypto handshakeCrypto[HANDSHAKE_MODES] = {
	{ 0x0b, mode11Key1, (const unsigned char *)randomKey, mode11Check },
};

static const struct HandshakeMode handshakeModes[HANDSHAKE_MODES] = {
	{ 0x0b, (unsigned int *)&handshakeUnlocked, handshakeUnlock11, handshakeLockHandler11 },
};

int handshakeInit(void)
{
	memset_n(&handshakePacket, 0, sizeof(handshakePacket));
	memset_n(&handshake, 0, sizeof(handshake));
	handshake.state = 0;
	handshake.mode = 0xff;
	handshakeUnlocked = HANDSHAKE_LOCKED;
	return 0;
}

/* state 0 unlocks the mode, anything else locks it; the handler's result */
unsigned char handshakeSetMode(unsigned char state, unsigned char mode)
{
	unsigned char result;

	result = 0;
	if (handshakeModes[0].mode == mode) {
		if (state == 0) {
			*handshakeModes[0].unlocked = HANDSHAKE_UNLOCKED;
			handshake.state = 0;
			handshake.mode = 0xff;
			if (handshakeModes[0].unlock != NULL)
				result = (unsigned char)handshakeModes[0].unlock();
		} else {
			*handshakeModes[0].unlocked = HANDSHAKE_LOCKED;
			if (handshakeModes[0].lock != NULL)
				result = (unsigned char)handshakeModes[0].lock();
		}
	}
	memset_n(&handshakePacket, 0, sizeof(handshakePacket));
	return result;
}

/* Process the updater's mode 0x0b challenge and response. */
unsigned char handshakeCommand(unsigned char *data)
{
	struct AesKey key;
	unsigned long *roundKeys;
	int keyResult;
	/* The original passes these stack bytes without initializing them first. */
	unsigned char nonce[16];
	unsigned char seed[4];

	memcpy(&handshakePacket, data, sizeof(handshakePacket));
	switch (handshake.state) {
	case 0:
		handshake.mode = 0x0b;
		randomGenerate(seed, nonce);
		memcpy(handshake.challenge, nonce, 8);
		handshakeDump("reqhello", data);
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
		handshakeDump("hello", data);
		break;
	case 2:
		handshakeDump("challenge", data);
		handshakePacket.error = 0;
		if (handshakePacket.magic != 0x30 || handshakePacket.step != 2 ||
		    handshakePacket.error != 0 || handshakePacket.mode != handshake.mode) {
			handshake.state = 0xff;
			handshakePacket.error = 1;
		}
		keyResult = handshakeGetRoundKeys(&roundKeys);
		if (keyResult == 0) {
			key.w = (unsigned long *)(unsigned long __near *)roundKeys;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeCrypto[0].firstKey, (struct AesKey __near *)&key);
			aesCbcDecrypt(handshakePacket.challenge, (unsigned char __near *)(data + 8),
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		} else {
			handshake.state = 0xff;
		}
		memcpy(handshakePacket.challenge, (unsigned char __near *)(data + 8), 0x20);
		handshakeDump("challenge_d", data);
		if (memcmp(handshakePacket.response, handshake.challenge, 8) != 0 &&
		    handshakePacket.error == 0) {
			handshake.state = 0xff;
			handshakePacket.error = 2;
		}
		if (memcmp(handshakePacket.key, mode11Check, 16) != 0 &&
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
		memset(handshakePacket.key, 0, 16);
		if (keyResult == 0) {
			key.w = (unsigned long *)(unsigned long __near *)roundKeys;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)handshakeCrypto[0].secondKey, (struct AesKey __near *)&key);
			aesCbcEncrypt(handshakePacket.challenge, handshakePacket.challenge,
				      (struct AesKey __near *)&key, handshakeIv, 0x20);
		} else {
			handshake.state = 0xff;
		}
		memcpy(data, &handshakePacket, sizeof(handshakePacket));
		handshakeDump("response", data);
		if (handshakePacket.error == 0)
			handshake.state = 0;
		break;
	default:
		break;
	}
	if (handshake.state == 0xff) {
		handshakeSetMode(handshake.state, handshake.mode);
		return 0;
	}
	if (handshake.state == 0) {
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
	return handshakeUnlocked == HANDSHAKE_UNLOCKED;
}

int handshakeUnlock11(void)
{
	return 0;
}

int handshakeLockHandler11(void)
{
	return 0;
}

/* a stage of the exchange, for a trace not built */
void handshakeDump(const char *name, unsigned char *data)
{
}

/* a host request refused while mode 0x0b is locked: transition 0x32; called by nothing */
unsigned char handshakeRefuse(void)
{
	struct StateEvent event;

	event.transition = 0x32;
	return (unsigned char)updStateEvent(&event);
}

int handshakeGetRoundKeys(unsigned long __far * __far *w)
{
	*w = (unsigned long __far *)handshakeRoundKeys;
	return 0;
}

/* the application starting the updater unlocks mode 0x0b */
void handshakeSetUnlocked(unsigned int unlocked)
{
	handshakeUnlocked = unlocked;
}
