/*
 * Helpers (app/util.c, updater/util.c): bytes as hex text and back, and the random generator the handshakes'
 * nonces come from.
 */
#ifndef ERNIE_UTIL_H
#define ERNIE_UTIL_H

#define HEX_INVALID	0xff	/* hexDigitValue: not a hex digit; hexDecode: the text holds one */

extern __near unsigned long randomCounter;
extern __near unsigned char randomBlock[16];

extern void hexEncodeByte(char *out, unsigned char byte);
extern void hexEncode(char *out, const unsigned char *in, unsigned int length);
extern unsigned char hexDigitValue(unsigned char c);
extern unsigned char hexDecode(unsigned char *out, const char *in, unsigned int length);
#ifdef ERNIE_UPDATER
extern void randomGenerate(const unsigned char __near *seed, unsigned char *nonce);
#else
extern void randomGenerate(unsigned long seed, unsigned char *out, unsigned char length);
#endif

#endif
