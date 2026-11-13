/*
 * Helpers (app/util.c, updater/util.c): bytes as hex text and back, and the random generator the handshakes'
 * nonces come from.
 */
#ifndef ERNIE_UTIL_H
#define ERNIE_UTIL_H

#include "ernie.h"

#define HEX_INVALID	0xff	/* hexDigitValue: not a hex digit; hexDecode: the text holds one */

/* before 0x01000107 the application's generator is the updater's: one AES block of the seed
   and the clock */
#if defined(ERNIE_UPDATER) || !FW_SINCE(0x01000107)
#define RANDOM_SEEDED	1
#else
#define RANDOM_SEEDED	0
extern __near unsigned long randomCounter;
extern __near unsigned char randomBlock[16];
#endif

extern void hexEncodeByte(char *out, unsigned char byte);
extern void hexEncode(char *out, const unsigned char *in, unsigned int length);
extern unsigned char hexDigitValue(unsigned char c);
extern unsigned char hexDecode(unsigned char *out, const char *in, unsigned int length);
#if RANDOM_SEEDED && !defined(ERNIE_UPDATER) && !defined(ERNIE_RANDOM_DEFINE)
/* before 0x01000107 the application's callers see it without its parameters and pass the seed
   far (the page in C, B clear); the definition reads only the near address in AX */
extern void randomGenerate();
#elif RANDOM_SEEDED
extern void randomGenerate(const unsigned char __near *seed, unsigned char *nonce);
#else
extern void randomGenerate(unsigned long seed, unsigned char *out, unsigned char length);
#endif

#endif
