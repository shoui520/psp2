/*
 * The host's handshake, as the updater has it: host command 0x00a0 runs a challenge and
 * response with the host (handshakeCommand), and mode 0x0b, the only one, is unlocked by it
 * or by the application starting the updater (handshakeSetUnlocked): its word is set to
 * HANDSHAKE_UNLOCKED. A failed exchange locks it again.
 */
#include <stddef.h>
#include <string.h>

#include "state.h"
#include "updater.h"

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

/*
 * PLACEHOLDER: cryptographic routine, deferred (the challenge and response of host command
 * 0x00a0, 0x3b26e-0x3b5a8: the 0x28-byte packet is taken into handshakePacket; in state 0
 * the mode asked for is taken and a nonce made with randomGenerate into handshake.challenge,
 * step 2 sent back; in state 1 the host's step 2 is AES-CBC decrypted under a key of the
 * table at 0x35e4 (its schedule in handshakeRoundKeys, from handshakeGetRoundKeys) with the
 * IV at 0x33ea, its nonce compared, and the reply AES-CBC encrypted under the next key,
 * step 3; each stage traced with handshakeDump ("hello", "challenge", "challenge_d",
 * "response"). The packet goes back to data; a failed or finished exchange calls
 * handshakeSetMode). Returns 0.
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
