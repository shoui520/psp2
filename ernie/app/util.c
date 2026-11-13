/*
 * Helpers: bytes as hex text, two digits each, most significant first, and back; the random
 * generator the handshakes' nonces come from.
 */
#define ERNIE_RANDOM_DEFINE
#include "charge.h"
#include "aes.h"
#include "authkeys.h"
#include "config.h"
#include "kernel.h"
#include "rtc.h"
#include "sysinfo.h"
#include "util.h"
#include <string.h>

static const char hexDigits[] = "0123456789ABCDEF";

#if !RANDOM_SEEDED
__near unsigned long randomCounter;		/* counts the generator's blocks */
__near unsigned char randomBlock[16];		/* the generator's last block */
#endif

/* the byte as two digits (never called) */
void hexEncodeByte(char *out, unsigned char byte)
{
	out[0] = hexDigits[byte >> 4];
	out[1] = hexDigits[byte & 0x0f];
}

/* length bytes as 2 * length digits */
void hexEncode(char *out, const unsigned char *in, unsigned int length)
{
	while (length--) {
		*out++ = hexDigits[*in >> 4];
		*out++ = hexDigits[*in & 0x0f];
		in++;
	}
}

/* the digit's value, either case; HEX_INVALID if it is none */
unsigned char hexDigitValue(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return HEX_INVALID;
}

/* 2 * length digits as length bytes; HEX_INVALID at the first that is not a digit */
unsigned char hexDecode(unsigned char *out, const char *in, unsigned int length)
{
	unsigned char high;
	unsigned char low;

	while (length--) {
		high = hexDigitValue(*in++);
		if (high == HEX_INVALID)
			return HEX_INVALID;
		low = hexDigitValue(*in++);
		if (low == HEX_INVALID)
			return HEX_INVALID;
		*out++ = high << 4 | low;
	}
	return 0;
}

#if RANDOM_SEEDED
/* The seed and two clock bytes, encrypted in place with the factory key: the nonce (as the
   updater's). */
void randomGenerate(const unsigned char __near *seed, unsigned char *nonce)
{
	struct AesKey key;

	((unsigned char __near *)nonce)[0] = seed[0];
	((unsigned char __near *)nonce)[1] = seed[1];
	((unsigned char __near *)nonce)[2] = seed[2];
	((unsigned char __near *)nonce)[3] = seed[3];
	((unsigned char __near *)nonce)[4] = getClock().byte[0];
	((unsigned char __near *)nonce)[5] = getClock().byte[1];
	key.w = (unsigned long *)factoryRoundKeys;
	key.nk = 4;
	key.nb = 4;
	aesEncrypt((unsigned char __near *)nonce, (unsigned char __near *)nonce,
		   (struct AesKey __near *)&key);
}
#else
/* Generate successive nonce blocks from the device state and the preceding block. */
void randomGenerate(unsigned long seed, unsigned char *out, unsigned char length)
{
	struct AesKey key;
	unsigned char input[16];
	VP block;
	unsigned long id;
	unsigned char i;
	unsigned char offset;

	memcpy_n(input, &seed, 4);
	offset = 0;
	while (length != 0) {
		id = getIdRecord1();
		memcpy_n(input + 4, &id, 4);
		chargeReadTempVolt((unsigned int *)(input + 8));
		memcpy(input + 10, configGet(), 2);
		memcpy_n(input + 12, &randomCounter, 4);
		for (i = 16; i != 0; ) {
			i--;
			input[i] ^= randomBlock[i];
		}
		key.nk = 4;
		key.nb = 4;
		if (pget_mpf(2, &block) == E_OK) {
			key.w = (unsigned long *)(unsigned long __near *)block;
			aesKeyExpansion(randomKey, (struct AesKey __near *)&key);
		} else {
			key.w = (unsigned long *)factoryRoundKeys;
		}
		aesEncrypt((unsigned char __near *)input, randomBlock,
			   (struct AesKey __near *)&key);
		rel_mpf(2, block);
		randomCounter++;
		memcpy_n((unsigned char __near *)out + offset, randomBlock,
			 length < 16 ? length : 16);
		if (length < 16)
			break;
		offset += length < 16 ? length : 16;
		length -= 16;
	}
}
#endif
