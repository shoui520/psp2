/*
 * The serial link's authenticated modes, as the updater has them: mode 1's command 0x0110 runs
 * a challenge and response with the other end (serialAuthCommand), and a mode it passes is
 * unlocked: its word is set to SERIAL_AUTH_UNLOCKED and its unlock handler called; a failed
 * one is locked again. Modes 1 and 0x0e unlock NVS areas for serial1.c. The updater logs
 * nothing.
 */
#include <stddef.h>
#include <string.h>

#include "aes.h"
#include "authkeys.h"
#include "ernie.h"
#include "serialauth.h"
#include "state.h"
#include "util.h"

#define SERIAL_AUTH_MODES	3

/* An unknown mode falls past the reference updater's table into these ROM pointers. */
#if ERNIE_HW == 0x00415000 && ERNIE_FW == 0x01000306
static const unsigned char *serialInvalidPointer(unsigned long value)
{
	union {
		unsigned long value;
		const unsigned char *pointer;
	} address;

	address.value = value;
	return address.pointer;
}

#define SERIAL_FIRST_KEY(index) ((index) == 0xff ? serialInvalidPointer(0x52a117UL) : firstKeys[index])
#define SERIAL_SECOND_KEY(index) ((index) == 0xff ? serialInvalidPointer(0xe6c1c3UL) : secondKeys[index])
#define SERIAL_CHECK(index) ((index) == 0xff ? serialInvalidPointer(0xfc1150UL) : checks[index])
#else
#define SERIAL_FIRST_KEY(index) (firstKeys[index])
#define SERIAL_SECOND_KEY(index) (secondKeys[index])
#define SERIAL_CHECK(index) (checks[index])
#endif

/* a mode: its number, its word, and what unlocking and locking it do */
struct SerialAuthMode {
	unsigned char	mode;
	unsigned int	*unlocked;
	unsigned char	(*unlock)(void);
	unsigned char	(*lock)(void);
};

/* the exchange's packet: the challenge and response go out and come back in it */
struct SerialAuthPacket {
	unsigned char	magic;			/* 0x30 */
	unsigned char	step;			/* 1-3, 0xff when done */
	unsigned char	error;
	unsigned char	mode;
	unsigned char	header[4];
	unsigned char	challenge[8];
	unsigned char	response[8];
	unsigned char	key[16];
};

/* the exchange's state */
static struct {
	unsigned char	state;			/* 0 none, 1-3 the steps, 0xff failed */
	unsigned char	mode;			/* the mode asked for, 0xff none */
	unsigned char	challenge[8];
	unsigned char	nonce[8];
} serialAuth;

static unsigned int serialAuthUnlocked[SERIAL_AUTH_MODES];

static __near struct SerialAuthPacket serialAuthPacket;
static __near unsigned long serialAuthRoundKeys[44];	/* serialAuthCommand's AES-128 key
							   schedule */


static unsigned char serialAuthUnlock0(void);
static unsigned char serialAuthLock0(void);
static unsigned char serialAuthUnlock1(void);
static unsigned char serialAuthLock1(void);
static unsigned char serialAuthUnlock14(void);
static unsigned char serialAuthLock14(void);
int serialAuthGetRoundKeys(unsigned long **w);

static const struct SerialAuthMode __far serialAuthModes[SERIAL_AUTH_MODES] = {
	{ 0x00, &serialAuthUnlocked[0], serialAuthUnlock0, serialAuthLock0 },
	{ 0x01, &serialAuthUnlocked[1], serialAuthUnlock1, serialAuthLock1 },
	{ 0x0e, &serialAuthUnlocked[2], serialAuthUnlock14, serialAuthLock14 },
};

unsigned char serialAuthInit(void)
{
	memset(&serialAuthPacket, 0, sizeof(serialAuthPacket));
	memset(&serialAuth, 0, sizeof(serialAuth));
	serialAuth.state = 0;
	serialAuth.mode = 0xff;
	serialAuthUnlocked[0] = SERIAL_AUTH_LOCKED;
	serialAuthUnlocked[1] = SERIAL_AUTH_LOCKED;
	serialAuthUnlocked[2] = SERIAL_AUTH_LOCKED;
	return 0;
}

/* state 0 unlocks the mode, anything else locks it; the handler's result */
unsigned char serialAuthSetMode(unsigned char state, unsigned char mode)
{
	unsigned char i;
	unsigned char result;

	result = 0;
	i = SERIAL_AUTH_MODES;
	while (i-- != 0) {
		if (serialAuthModes[i].mode != mode)
			continue;
		if (state == 0) {
			*serialAuthModes[i].unlocked = SERIAL_AUTH_UNLOCKED;
			serialAuth.state = 0;
			serialAuth.mode = 0xff;
			if (serialAuthModes[i].unlock != NULL)
				result = serialAuthModes[i].unlock();
		} else {
			*serialAuthModes[i].unlocked = SERIAL_AUTH_LOCKED;
			if (serialAuthModes[i].lock != NULL)
				result = serialAuthModes[i].lock();
		}
		break;
	}
	memset(&serialAuthPacket, 0, sizeof(serialAuthPacket));
	return result;
}

/* Process the updater's serial authenticated exchange. */
unsigned char serialAuthCommand(unsigned char *data)
{
	static const unsigned char * const firstKeys[] = { serialKey0First, serialKey1First, serialKey2First };
	static const unsigned char * const secondKeys[] = { serialKey0Second, serialKey1Second, serialKey2Second };
	static const unsigned char * const checks[] = { serialKey0Check, serialKey1Check, serialKey2Check };
	struct AesKey key;
	/* The original passes these stack bytes without initializing them first. */
	unsigned char nonce[16];
	unsigned char seed[4];
	unsigned long *roundKeys;
	int keyResult;
	unsigned char index;
	unsigned char result = 0;

	memcpy(&serialAuthPacket, data, sizeof(serialAuthPacket));
	switch (serialAuth.state) {
	case 0:
		serialAuth.mode = serialAuthPacket.mode;
		if (serialAuth.mode != 0 && serialAuth.mode != 1 && serialAuth.mode != 0x0e) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		} else {
			serialAuth.state = 1;
			serialAuthPacket.error = 0;
		}
		/* fall through */
	case 1:
		randomGenerate((const unsigned char __near *)seed, (unsigned char *)nonce);
		memcpy(serialAuth.nonce, nonce, 8);
		serialAuthPacket.magic = 0x30;
		serialAuthPacket.step = 0;
		serialAuthPacket.mode = serialAuth.mode;
		memset(serialAuthPacket.header, 0, 0x24);
		memcpy(data, &serialAuthPacket, sizeof(serialAuthPacket));
		serialAuth.state = 2;
		break;
	case 2:
		if (serialAuthPacket.magic != 0x30 || serialAuthPacket.step != 1 ||
		    serialAuthPacket.error != 0 || serialAuthPacket.mode != serialAuth.mode) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		} else {
			serialAuth.state = 3;
			serialAuthPacket.error = 0;
		}
		index = SERIAL_AUTH_MODES;
		while (index-- != 0) {
			if (serialAuthModes[index].mode == serialAuth.mode)
				break;
		}
		memcpy(serialAuth.challenge, serialAuthPacket.challenge, 8);
		serialAuthPacket.magic = 0x30;
		serialAuthPacket.step = 2;
		serialAuthPacket.mode = serialAuth.mode;
		memcpy(serialAuthPacket.header, serialHeader, 4);
		memcpy(serialAuthPacket.challenge, serialAuth.nonce, 8);
		memcpy(serialAuthPacket.response, serialAuth.challenge, 8);
		memcpy(serialAuthPacket.key, SERIAL_CHECK(index), 16);
		keyResult = serialAuthGetRoundKeys(&roundKeys);
		if (keyResult == 0) {
			key.w = (unsigned long *)(unsigned long __near *)roundKeys;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)SERIAL_FIRST_KEY(index), (struct AesKey __near *)&key);
			aesCbcEncrypt(serialAuthPacket.challenge, serialAuthPacket.challenge,
				      (struct AesKey __near *)&key, serialIv, 0x20);
		} else {
			serialAuth.state = 0xff;
		}
		memcpy(data, &serialAuthPacket, sizeof(serialAuthPacket));
		break;
	case 3:
		if (serialAuthPacket.magic != 0x30 || serialAuthPacket.step != 3 ||
		    serialAuthPacket.error != 0 || serialAuthPacket.mode != serialAuth.mode) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		}
		index = SERIAL_AUTH_MODES;
		while (index-- != 0) {
			if (serialAuthModes[index].mode == serialAuth.mode)
				break;
		}
		keyResult = serialAuthGetRoundKeys(&roundKeys);
		if (keyResult == 0) {
			key.w = (unsigned long *)(unsigned long __near *)roundKeys;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)SERIAL_SECOND_KEY(index), (struct AesKey __near *)&key);
			aesCbcDecrypt(serialAuthPacket.challenge, (unsigned char __near *)(data + 8),
				      (struct AesKey __near *)&key, serialIv, 0x20);
		} else {
			serialAuth.state = 0xff;
		}
		memcpy(serialAuthPacket.challenge, data + 8, 0x20);
		if (serialAuth.state != 0xff) {
			if (memcmp(serialAuthPacket.challenge, serialAuth.challenge, 8) != 0 ||
			    memcmp(serialAuthPacket.response, serialAuth.nonce, 8) != 0) {
				serialAuth.state = 0xff;
				serialAuthPacket.error = 2;
			} else {
				serialAuth.state = 0;
				serialAuthPacket.error = 0;
			}
		}
		serialAuthPacket.magic = 0x30;
		serialAuthPacket.step = 0xff;
		serialAuthPacket.mode = serialAuth.mode;
		memset(serialAuthPacket.header, 0, 0x24);
		memcpy(data, &serialAuthPacket, sizeof(serialAuthPacket));
		break;
	default:
		break;
	}
	if (serialAuth.state == 0xff || serialAuth.state == 0)
		result = serialAuthSetMode(serialAuth.state, serialAuth.mode);
	return result;
}

unsigned char serialAuthState(void)
{
	return serialAuth.state;
}

unsigned char serialAuthUnlocked0(void)
{
	return serialAuthUnlocked[0] == SERIAL_AUTH_UNLOCKED;
}

unsigned char serialAuthUnlocked1(void)
{
	return serialAuthUnlocked[1] == SERIAL_AUTH_UNLOCKED;
}

unsigned char serialAuthUnlocked14(void)
{
	return serialAuthUnlocked[2] == SERIAL_AUTH_UNLOCKED;
}

/* never called */
void serialAuthNone(void)
{
}

/* serialAuthCommand's key schedule */
int serialAuthGetRoundKeys(unsigned long **w)
{
	*w = (unsigned long *)serialAuthRoundKeys;
	return 0;
}

static unsigned char serialAuthUnlock0(void)
{
	unsigned char result;

	if (stateSerialUnlock())
		result = 0x11;
	else
		result = 0;
	return result;
}

static unsigned char serialAuthLock0(void)
{
	stateSerialLock();
	return 0;
}

static unsigned char serialAuthUnlock1(void)
{
	return 0;
}

static unsigned char serialAuthLock1(void)
{
	return 0;
}

static unsigned char serialAuthUnlock14(void)
{
	return 0;
}

static unsigned char serialAuthLock14(void)
{
	return 0;
}
