/*
 * The serial link's authenticated modes: mode 1's command 0x0110 runs a challenge and response
 * with the other end (serialAuthCommand), and a mode it passes is unlocked: its word is set to
 * SERIAL_AUTH_UNLOCKED and its unlock handler called; a failed one is locked again. Modes 1 and
 * 0x0e unlock NVS areas for serial1.c.
 */
#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "serialauth.h"
#include "state.h"

#define SERIAL_AUTH_MODES	3

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

extern void sub_2DAE1(int);

static unsigned char serialAuthUnlock0(void);
static unsigned char serialAuthLock0(void);
static unsigned char serialAuthUnlock1(void);
static unsigned char serialAuthLock1(void);
static unsigned char serialAuthUnlock14(void);
static unsigned char serialAuthLock14(void);

static const struct SerialAuthMode serialAuthModes[SERIAL_AUTH_MODES] = {
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

/*
 * PLACEHOLDER: cryptographic routine, deferred (the challenge and response of command 0x0110:
 * the 0x28-byte packet is taken into serialAuthPacket; in state 0 the mode it asks for (0, 1
 * or 0x0e) is checked and a nonce made with randomGenerate into serialAuth.nonce, step 1 sent
 * back; in step 2 the challenge and nonce are AES-CBC encrypted under the mode's first key
 * (the table at 0xff70) with the IV at 0xfed0 and sent with the header at 0xfecc; in step 3
 * the answer is AES-CBC decrypted under the mode's second key and compared with the challenge
 * and nonce. A failed or finished exchange calls serialAuthSetMode). Returns its result.
 */
unsigned char serialAuthCommand(unsigned char *data)
{
	return 0;
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

void sub_224FD(void)
{
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
	sub_2DAE1(0x0f);
	return 0;
}

static unsigned char serialAuthUnlock1(void)
{
	sub_2DAE1(0x11);
	return 0;
}

static unsigned char serialAuthLock1(void)
{
	sub_2DAE1(0x10);
	return 0;
}

static unsigned char serialAuthUnlock14(void)
{
	sub_2DAE1(0x13);
	return 0;
}

static unsigned char serialAuthLock14(void)
{
	sub_2DAE1(0x12);
	return 0;
}
