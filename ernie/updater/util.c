/*
 * The updater's helpers: bytes as hex text, two digits each, most significant first, and
 * back; the nonce generator of the serial link's handshake.
 */
#include "util.h"
#include "updater.h"
#include "aes.h"
#include "authkeys.h"
#include "rtc.h"

static const char __far hexDigits[] = "0123456789ABCDEF";

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

/* Mix the seed and the clock into the caller's block, then encrypt it in place. */
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
