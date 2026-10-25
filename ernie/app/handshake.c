/*
 * The host's handshake: host command 0x00a0 runs a challenge and response with the host
 * (handshakeCommand), and a mode it passes is unlocked: its word is set to HANDSHAKE_UNLOCKED
 * and its unlock handler called; a failed one is locked again. Mode 0x0b lets the host's
 * requests through, mode 0x0f leaves a session key in handshakeKey.
 */
#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "handshake.h"
#include "state.h"

#define HANDSHAKE_MODES		2

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

static __near unsigned char handshakeKey[16];	/* mode 0x0f's session key */

static __near struct HandshakePacket handshakePacket;
static __near struct Handshake handshake;
static __near unsigned int handshakeUnlocked[HANDSHAKE_MODES];

extern void sub_2DAE1(int);

static int handshakeUnlock11(void);
static int handshakeLockHandler11(void);
static int handshakeUnlock15(void);
static int handshakeLockHandler15(void);

static const struct HandshakeMode handshakeModes[HANDSHAKE_MODES] = {
	{ 0x0b, (unsigned int *)&handshakeUnlocked[0], handshakeUnlock11, handshakeLockHandler11 },
	{ 0x0f, (unsigned int *)&handshakeUnlocked[1], handshakeUnlock15, handshakeLockHandler15 },
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

/*
 * PLACEHOLDER: cryptographic routine, deferred (the challenge and response of host command
 * 0x00a0: the 0x28-byte packet is taken into handshakePacket; in state 0 the mode asked for
 * (0x0b or 0x0f) is taken, sub_2D6EB called and a nonce made with randomGenerate into
 * handshake.challenge, step 2 sent back (mode 0x0f first clears its key and calls
 * sub_2ED26); in state 1 the host's step 2 is AES-CBC decrypted under the mode's first key
 * (the table at 0x1053c) with the IV at 0x10588, its nonce and the mode's check value
 * compared, and the reply AES-CBC encrypted under the second key, step 3; in state 3 (mode
 * 0x0f) the challenges are encrypted under the third key into the session key, copied to
 * handshakeKey, and the host's step 4 decrypted under it and compared, step 5. The packet
 * goes back to data; a failed or finished exchange calls handshakeSetMode). Returns 0.
 */
unsigned char handshakeCommand(unsigned char *data)
{
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
	sub_2DAE1(0x16);
	return 0;
}

int handshakeUnlocked15(void)
{
	return handshakeUnlocked[1] == HANDSHAKE_UNLOCKED;
}

int handshakeLock15(void)
{
	memset_n(handshakeKey, 0, sizeof(handshakeKey));
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
