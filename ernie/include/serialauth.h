/*
 * The serial link's authenticated modes (app/serialauth.c): command 0x0110's challenge and
 * response, and the words of the modes it unlocks.
 */
#ifndef ERNIE_SERIALAUTH_H
#define ERNIE_SERIALAUTH_H

#define SERIAL_AUTH_UNLOCKED	0xa5a5
#define SERIAL_AUTH_LOCKED	0x5a5a

extern unsigned char serialAuthInit(void);
extern unsigned char serialAuthSetMode(unsigned char state, unsigned char mode);
extern unsigned char serialAuthCommand(unsigned char *data);
extern unsigned char serialAuthState(void);
extern unsigned char serialAuthUnlocked0(void);
extern unsigned char serialAuthUnlocked1(void);
extern unsigned char serialAuthUnlocked14(void);
extern void sub_224FD(void);

#endif
