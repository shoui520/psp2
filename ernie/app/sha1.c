/*
 * SHA-1 (FIPS 180-1), from the sample code of RFC 3174 (sha1.c, section 7.2), as Sony changed
 * it for the syscon: the message block and W are the caller's (W a ring of 16 words, the RFC's
 * method 2), the message may be far, the Corrupted state is gone, and SHA1InputBlocks hashes
 * whole KiB blocks. The RFC's names are kept.
 *
 * Copyright (C) The Internet Society (2001).  All Rights Reserved.
 *
 * This document and translations of it may be copied and furnished to others, and derivative
 * works that comment on or otherwise explain it or assist in its implementation may be
 * prepared, copied, published and distributed, in whole or in part, without restriction of
 * any kind, provided that the above copyright notice and this paragraph are included on all
 * such copies and derivative works.  However, this document itself may not be modified in any
 * way, such as by removing the copyright notice or references to the Internet Society or
 * other Internet organizations, except as needed for the purpose of developing Internet
 * standards in which case the procedures for copyrights defined in the Internet Standards
 * process must be followed, or as required to translate it into languages other than
 * English.
 */
#include "ernie.h"
#include "sha1.h"
#ifdef ERNIE_UPDATER
#include "util.h"
#endif

/* Define the SHA1 circular left shift macro */
#define SHA1CircularShift(bits, word) \
	(((word) << (bits)) | ((word) >> (32 - (bits))))

/* Constants defined in SHA-1 */
static const unsigned long K[] = {
	0x5A827999,
	0x6ED9EBA1,
	0x8F1BBCDC,
	0xCA62C1D6
};

/* Local Function Prototypes */
void SHA1PadMessage(SHA1Context __near *context);
void SHA1ProcessMessageBlock(SHA1Context __near *context);
#ifdef ERNIE_UPDATER
void SHA1DebugDump(unsigned long a, unsigned long b, unsigned long c, unsigned long d,
		   unsigned long e);
#endif

/* initializes the context, on the caller's block and W, in preparation for a new digest */
int SHA1Reset(SHA1Context __near *context, unsigned char __near *block, unsigned long __near *w)
{
	if (!context)
		return shaNull;

	context->Intermediate_Hash[0] = 0x67452301;
	context->Intermediate_Hash[1] = 0xEFCDAB89;
	context->Intermediate_Hash[2] = 0x98BADCFE;
	context->Intermediate_Hash[3] = 0x10325476;
	context->Intermediate_Hash[4] = 0xC3D2E1F0;

	context->Length_Low = 0;
	context->Length_High = 0;
	context->Message_Block = block;
	context->Message_Block_Index = 0;
	context->Computed = 0;
	context->W = w;

	return shaSuccess;
}

/* the 160-bit message digest into Message_Digest, the first octet the hash's high byte */
int SHA1Result(SHA1Context __near *context, unsigned char __near *Message_Digest)
{
	unsigned char i;

	if (!context || !Message_Digest)
		return shaNull;

	if (!context->Computed) {
		SHA1PadMessage(context);
		for (i = 0; i < SHA1BlockSize; ++i) {
			/* message may be sensitive, clear it out */
			context->Message_Block[i] = 0;
		}
		context->Length_Low = 0;	/* and clear length */
		context->Length_High = 0;
		context->Computed = 1;
	}

	for (i = 0; i < SHA1HashSize; ++i)
		Message_Digest[i] = (unsigned char)(context->Intermediate_Hash[i >> 2] >>
						     8 * (3 - (i & 0x03)));

	return shaSuccess;
}

/* the next length octets of the message */
int SHA1Input(SHA1Context __near *context, const unsigned char *message_array,
	      unsigned long length)
{
	if (!length)
		return shaSuccess;

	if (!context || !message_array)
		return shaNull;

	if (context->Computed)
		return shaStateError;

	while (length--) {
		context->Message_Block[context->Message_Block_Index++] = *message_array & 0xFF;

		context->Length_Low += 8;
		if (context->Length_Low == 0) {
			context->Length_High++;
			if (context->Length_High == 0) {
				/* Message is too long */
				return shaInputTooLong;
			}
		}

		if (context->Message_Block_Index == SHA1BlockSize)
			SHA1ProcessMessageBlock(context);

		message_array = FAR_ADD(message_array, 1);
	}

	return shaSuccess;
}

/* processes the next 512 bits of the message stored in the Message_Block array */
void SHA1ProcessMessageBlock(SHA1Context __near *context)
{
	unsigned char t;		/* Loop counter */
	unsigned char s;		/* W's word for t */
	unsigned long __near *W;	/* Word sequence */
	unsigned long temp;		/* Temporary word value */
	unsigned long A, B, C, D, E;	/* Word buffers */

	W = context->W;
	A = context->Intermediate_Hash[0];
	B = context->Intermediate_Hash[1];
	C = context->Intermediate_Hash[2];
	D = context->Intermediate_Hash[3];
	E = context->Intermediate_Hash[4];

	/* Initialize the first 16 words in the array W */
	for (t = 0; t < 16; t++) {
		((unsigned char __near *)&W[t])[3] = context->Message_Block[t * 4];
		((unsigned char __near *)&W[t])[2] = context->Message_Block[t * 4 + 1];
		((unsigned char __near *)&W[t])[1] = context->Message_Block[t * 4 + 2];
		((unsigned char __near *)&W[t])[0] = context->Message_Block[t * 4 + 3];
	}

	for (t = 0; t < 80; t++) {
		s = t & 0x0f;
		if (t >= 16)
			W[s] = SHA1CircularShift(1, W[(s + 13) & 0x0f] ^ W[(s + 8) & 0x0f] ^
						    W[(s + 2) & 0x0f] ^ W[s]);

		temp = SHA1CircularShift(5, A) + E + W[s];
		if (t < 20)
			temp += ((B & C) | ((~B) & D)) + K[0];
		else if (t < 40)
			temp += (B ^ C ^ D) + K[1];
		else if (t < 60)
			temp += ((B & C) | (B & D) | (C & D)) + K[2];
		else
			temp += (B ^ C ^ D) + K[3];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	context->Intermediate_Hash[0] += A;
	context->Intermediate_Hash[1] += B;
	context->Intermediate_Hash[2] += C;
	context->Intermediate_Hash[3] += D;
	context->Intermediate_Hash[4] += E;

	context->Message_Block_Index = 0;
}

#ifdef ERNIE_UPDATER
/* a word as big-endian hex, then a pause */
#define SHA1DumpWord(word) \
	do { \
		(word) = (word) << 24 & 0xff000000 | (word) << 8 & 0x00ff0000 | \
			 (word) >> 8 & 0x0000ff00 | (word) >> 24 & 0x000000ff; \
		hexEncode(hex, (unsigned char *)&(word), 4); \
		for (i = 0; i < 0x2ff; i++) \
			; \
	} while (0)

/*
 * the updater's: the five words of the state as hex lines (crlf and hex, one after the other
 * in the frame), each followed by a pause; nothing sends them (the debug output it was
 * written for is not built), and nothing calls it
 */
void SHA1DebugDump(unsigned long a, unsigned long b, unsigned long c, unsigned long d,
		   unsigned long e)
{
	unsigned int i;
	char hex[9];			/* the word's digits and a space, after crlf */
	char crlf[2] = { '\r', '\n' };

	hex[8] = ' ';
	SHA1DumpWord(a);
	SHA1DumpWord(b);
	SHA1DumpWord(c);
	SHA1DumpWord(d);
	SHA1DumpWord(e);
	for (i = 0; i < 0x2ff; i++)
		;
}
#endif

/*
 * pads the message to a multiple of 512 bits: a 1 bit, zeros, and the message's length in its
 * last 64 bits, processing each block filled
 */
void SHA1PadMessage(SHA1Context __near *context)
{
	/*
	 * Check to see if the current message block is too small to hold the initial padding
	 * bits and length.  If so, we will pad the block, process it, and then continue padding
	 * into a second block.
	 */
	if (context->Message_Block_Index > 55) {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 64)
			context->Message_Block[context->Message_Block_Index++] = 0;

		SHA1ProcessMessageBlock(context);

		while (context->Message_Block_Index < 56)
			context->Message_Block[context->Message_Block_Index++] = 0;
	} else {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 56)
			context->Message_Block[context->Message_Block_Index++] = 0;
	}

	/* Store the message length as the last 8 octets */
	context->Message_Block[56] = (unsigned char)(context->Length_High >> 24);
	context->Message_Block[57] = (unsigned char)(context->Length_High >> 16);
	context->Message_Block[58] = (unsigned char)(context->Length_High >> 8);
	context->Message_Block[59] = (unsigned char)(context->Length_High);
	context->Message_Block[60] = (unsigned char)(context->Length_Low >> 24);
	context->Message_Block[61] = (unsigned char)(context->Length_Low >> 16);
	context->Message_Block[62] = (unsigned char)(context->Length_Low >> 8);
	context->Message_Block[63] = (unsigned char)(context->Length_Low);

	SHA1ProcessMessageBlock(context);
}

/* blocks KiB of data, 63 KiB at a time; the last SHA1Input's result */
int SHA1InputBlocks(SHA1Context __near *context, const unsigned char *data, unsigned long blocks)
{
	unsigned char result;

	while (blocks > 0x3f) {
		result = (unsigned char)SHA1Input(context, data, 0xfc00);
		blocks -= 0x3f;
		data = FAR_ADD(data, 0xfc00);
	}
	if (result == shaSuccess)
		result = (unsigned char)SHA1Input(context, data, blocks << 10);
	return result;
}
