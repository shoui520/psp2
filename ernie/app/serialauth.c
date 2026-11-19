/*
 * The serial link's authenticated modes: mode 1's command 0x0110 runs a challenge and response
 * with the other end (serialAuthCommand), and a mode it passes is unlocked: its word is set to
 * SERIAL_AUTH_UNLOCKED and its unlock handler called; a failed one is locked again. Modes 1 and
 * 0x0e unlock NVS areas for serial1.c.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "aes.h"
#include "authkeys.h"
#include "board.h"
#include "chip.h"
#include "ernie.h"
#include "kernel.h"
#include "log.h"
#include "serialauth.h"
#include "state.h"
#include "sysinfo.h"
#include "util.h"

#if FW_SINCE(0x00090804)
#define SERIAL_AUTH_MODES	3
#else
#define SERIAL_AUTH_MODES	2	/* 0x00090206 has no mode 0x0e */
#endif

/* a mode's keys: the first and second AES keys of the exchange, and the check its packet carries
   (a mode not found reads past the table, as Sony's does) */
struct SerialAuthKeys {
	unsigned char		mode;
	const unsigned char	*first;
	const unsigned char	*second;
	const unsigned char	*check;
};

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


#if FW_SINCE(0x00090804)
static unsigned char serialAuthUnlock0(void);
static unsigned char serialAuthLock0(void);
static unsigned char serialAuthUnlock1(void);
static unsigned char serialAuthLock1(void);
static unsigned char serialAuthUnlock14(void);
static unsigned char serialAuthLock14(void);
#endif


static const struct SerialAuthKeys serialAuthKeys[SERIAL_AUTH_MODES] = {
#if !FW_SINCE(0x00090009)	/* the round keys */
	{ 0x00, (const unsigned char *)serialKey0First, (const unsigned char *)serialKey0Second,
	  serialKey0Check },
	{ 0x01, (const unsigned char *)serialKey1First, (const unsigned char *)serialKey1Second,
	  serialKey1Check },
#else
	{ 0x00, serialKey0First, serialKey0Second, serialKey0Check },
	{ 0x01, serialKey1First, serialKey1Second, serialKey1Check },
#endif
#if FW_SINCE(0x00090804)
	{ 0x0e, serialKey2First, serialKey2Second, serialKey2Check },
#endif
};

static const struct SerialAuthMode serialAuthModes[SERIAL_AUTH_MODES] = {
#if FW_SINCE(0x00090804)
	{ 0x00, &serialAuthUnlocked[0], serialAuthUnlock0, serialAuthLock0 },
	{ 0x01, &serialAuthUnlocked[1], serialAuthUnlock1, serialAuthLock1 },
	{ 0x0e, &serialAuthUnlocked[2], serialAuthUnlock14, serialAuthLock14 },
#else
	/* 0x00090206: mode 0 runs the state's own handlers, mode 1 none */
	{ 0x00, &serialAuthUnlocked[0], stateSerialUnlock, (unsigned char (*)(void))stateSerialLock },
	{ 0x01, &serialAuthUnlocked[1], NULL, NULL },
#endif
};

unsigned char serialAuthInit(void)
{
	memset(&serialAuthPacket, 0, sizeof(serialAuthPacket));
	memset(&serialAuth, 0, sizeof(serialAuth));
	serialAuth.state = 0;
	serialAuth.mode = 0xff;
	serialAuthUnlocked[0] = SERIAL_AUTH_LOCKED;
	serialAuthUnlocked[1] = SERIAL_AUTH_LOCKED;
#if FW_SINCE(0x00090804)
	serialAuthUnlocked[2] = SERIAL_AUTH_LOCKED;
#endif
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

#if FW_SINCE(0x00090804)
#define SERIAL_AUTH_KEY	serialAuthKeys[index]
#else
#define SERIAL_AUTH_KEY	serialAuthKeys[serialAuth.mode]	/* 0x00090206: the modes are 0 and 1 */
#endif

#if !FW_SINCE(0x00090009)
/*
 * Process serial mode 1's authenticated exchange. 0x00080504 keeps each mode's keys expanded
 * (authkeys.c), so it takes no pool block and expands nothing.
 */
unsigned char serialAuthCommand(unsigned char *data)
{
	struct AesKey key;
	/* The original passes these stack bytes without initializing them first. */
	unsigned char nonce[16];
	unsigned char seed[4];

	memcpy(&serialAuthPacket, data, sizeof(serialAuthPacket));
	switch (serialAuth.state) {
	case 0:
		serialAuth.mode = serialAuthPacket.mode;
		if (serialAuth.mode != 0 && serialAuth.mode != 1) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		} else {
			serialAuth.state = 1;
			serialAuthPacket.error = 0;
		}
		/* fall through */
	case 1:
		randomGenerate(seed, nonce);
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
		memcpy(serialAuth.challenge, serialAuthPacket.challenge, 8);
		serialAuthPacket.magic = 0x30;
		serialAuthPacket.step = 2;
		serialAuthPacket.mode = serialAuth.mode;
		memcpy(serialAuthPacket.header, serialHeader, 4);
		memcpy(serialAuthPacket.challenge, serialAuth.nonce, 8);
		memcpy(serialAuthPacket.response, serialAuth.challenge, 8);
		memcpy(serialAuthPacket.key, SERIAL_AUTH_KEY.check, 16);
		key.w = (unsigned long *)SERIAL_AUTH_KEY.first;
		key.nk = 4;
		key.nb = 4;
		aesCbcEncrypt(serialAuthPacket.challenge, serialAuthPacket.challenge,
			      (struct AesKey __near *)&key, serialIv, 0x20);
		memcpy(data, &serialAuthPacket, sizeof(serialAuthPacket));
		break;
	case 3:
		if (serialAuthPacket.magic != 0x30 || serialAuthPacket.step != 3 ||
		    serialAuthPacket.error != 0 || serialAuthPacket.mode != serialAuth.mode) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		}
		key.w = (unsigned long *)SERIAL_AUTH_KEY.second;
		key.nk = 4;
		key.nb = 4;
		aesCbcDecrypt(serialAuthPacket.challenge, (unsigned char __near *)(data + 8),
			      (struct AesKey __near *)&key, serialIv, 0x20);
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
	if (serialAuth.state == 0xff) {
		serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return 0x10;
	} else if (serialAuth.state == 0) {
		serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return 0;
	}
	return 0;
}
#else
/* Process serial mode 1's authenticated exchange. */
unsigned char serialAuthCommand(unsigned char *data)
{
	struct AesKey key;
#if RANDOM_SEEDED
	/* The original passes these stack bytes without initializing them first. */
	unsigned char nonce[16];
	unsigned char seed[4];
#else
	unsigned long seed;
#endif
	VP block;
	int poolResult;
#if FW_SINCE(0x00090804)
	unsigned char index;
	unsigned char result = 0;
#endif

	memcpy(&serialAuthPacket, data, sizeof(serialAuthPacket));
	switch (serialAuth.state) {
	case 0:
		serialAuth.mode = serialAuthPacket.mode;
#if FW_SINCE(0x00090804)
		if (serialAuth.mode != 0 && serialAuth.mode != 1 && serialAuth.mode != 0x0e) {
#else
		if (serialAuth.mode != 0 && serialAuth.mode != 1) {
#endif
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		} else {
			serialAuth.state = 1;
			serialAuthPacket.error = 0;
		}
		/* fall through */
	case 1:
#if RANDOM_SEEDED
		randomGenerate(seed, nonce);
		memcpy(serialAuth.nonce, nonce, 8);
#else
		getIdRecordsLength();
		seed = TCR10;
		randomGenerate(seed, serialAuth.nonce, 8);
#endif
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
#if FW_SINCE(0x00090804)
		index = SERIAL_AUTH_MODES;
		while (index-- != 0) {
			if (serialAuthKeys[index].mode == serialAuth.mode)
				break;
		}
#endif
		memcpy(serialAuth.challenge, serialAuthPacket.challenge, 8);
		serialAuthPacket.magic = 0x30;
		serialAuthPacket.step = 2;
		serialAuthPacket.mode = serialAuth.mode;
		memcpy(serialAuthPacket.header, serialHeader, 4);
		memcpy(serialAuthPacket.challenge, serialAuth.nonce, 8);
		memcpy(serialAuthPacket.response, serialAuth.challenge, 8);
		memcpy(serialAuthPacket.key, SERIAL_AUTH_KEY.check, 16);
		poolResult = pget_mpf(2, &block);
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)SERIAL_AUTH_KEY.first,
					(struct AesKey __near *)&key);
			aesCbcEncrypt(serialAuthPacket.challenge, serialAuthPacket.challenge,
				      (struct AesKey __near *)&key, serialIv, 0x20);
		} else {
			serialAuth.state = 0xff;
		}
		rel_mpf(2, block);
		memcpy(data, &serialAuthPacket, sizeof(serialAuthPacket));
		break;
	case 3:
		if (serialAuthPacket.magic != 0x30 || serialAuthPacket.step != 3 ||
		    serialAuthPacket.error != 0 || serialAuthPacket.mode != serialAuth.mode) {
			serialAuth.state = 0xff;
			serialAuthPacket.error = 1;
		}
#if FW_SINCE(0x00090804)
		index = SERIAL_AUTH_MODES;
		while (index-- != 0) {
			if (serialAuthKeys[index].mode == serialAuth.mode)
				break;
		}
#endif
		poolResult = pget_mpf(2, &block);
		if (poolResult == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			key.nk = 4;
			key.nb = 4;
			aesKeyExpansion((const unsigned long *)SERIAL_AUTH_KEY.second,
					(struct AesKey __near *)&key);
			aesCbcDecrypt(serialAuthPacket.challenge, (unsigned char __near *)(data + 8),
				      (struct AesKey __near *)&key, serialIv, 0x20);
		} else {
			serialAuth.state = 0xff;
		}
		rel_mpf(2, block);
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
#if FW_SINCE(0x00090804)
	if (serialAuth.state == 0xff) {
		result = serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return result;
	} else if (serialAuth.state == 0) {
		result = serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return result;
	}
	return result;
#else
	/* 0x00090206 drops the handler's result: 0x10 when the exchange failed */
	if (serialAuth.state == 0xff) {
		serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return 0x10;
	} else if (serialAuth.state == 0) {
		serialAuthSetMode(serialAuth.state, serialAuth.mode);
		return 0;
	}
	return 0;
#endif
}
#endif

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

#if FW_SINCE(0x00090804)
unsigned char serialAuthUnlocked14(void)
{
	return serialAuthUnlocked[2] == SERIAL_AUTH_UNLOCKED;
}
#endif

#if !BOARD_USS1001	/* not in the slim's */
void serialAuthInput(void)
{
}
#endif

#if FW_SINCE(0x00090804)
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
	logEvent(0x0f);
	return 0;
}

static unsigned char serialAuthUnlock1(void)
{
	logEvent(0x11);
	return 0;
}

static unsigned char serialAuthLock1(void)
{
	logEvent(0x10);
	return 0;
}

static unsigned char serialAuthUnlock14(void)
{
	logEvent(0x13);
	return 0;
}

static unsigned char serialAuthLock14(void)
{
	logEvent(0x12);
	return 0;
}
#endif
