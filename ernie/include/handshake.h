/*
 * The host's handshake (app/handshake.c): host command 0x00a0's challenge and response, and
 * the words of the modes it unlocks. Until mode 0x0b is unlocked, the host's requests other
 * than the base commands are refused (handshakeRefuse).
 */
#ifndef ERNIE_HANDSHAKE_H
#define ERNIE_HANDSHAKE_H

#define HANDSHAKE_UNLOCKED	0xa5a5
#define HANDSHAKE_LOCKED	0x5a5a

extern int handshakeInit(unsigned char keepKey);
extern unsigned char handshakeSetMode(unsigned char state, unsigned char mode);
extern unsigned char handshakeCommand(unsigned char *data);
extern unsigned char handshakeState(void);
extern unsigned char handshakeUnlocked11(void);
extern int handshakeLock11(void);
extern int handshakeUnlocked15(void);
extern int handshakeLock15(void);
extern unsigned char handshakeRefuse(void);

#endif
