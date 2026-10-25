/*
 * Helpers: bytes as hex text, two digits each, most significant first, and back; the random
 * generator the handshakes' nonces come from.
 */
#include "charge.h"
#include "util.h"

static const char hexDigits[] = "0123456789ABCDEF";

__near unsigned long randomCounter;		/* counts the generator's blocks */
__near unsigned char randomBlock[16];		/* the generator's last block */

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

/*
 * PLACEHOLDER: cryptographic routine, deferred (the handshakes' nonce generator: for each 16
 * bytes of out, a block of the seed, sub_2D6E4's word, 2 bytes of the settings block
 * (chargeReadTempVolt, configGet) and randomCounter, XORed with randomBlock, is encrypted with AES under
 * the key at 0x105b8 (Keys.wiki's SharedKey_B_B; the schedule at 0xfaa4 when memory pool 2 has
 * no block) into randomBlock, which goes to out; randomCounter counts the blocks).
 */
void randomGenerate(unsigned long seed, unsigned char *out, unsigned char length)
{
}
