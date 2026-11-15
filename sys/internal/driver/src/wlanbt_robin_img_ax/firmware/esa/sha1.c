/*
 * SHA-1 (Marvell's common/sha1.c, 0x4c410..0x4c76c): the context is cleared with the
 * ROM's memclr and the working words are the context's own Scratch array, as the idle task has
 * no stack for them.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include <stdint.h>
#include "robin/rom.h"
#include "robin/sha1.h"

#define Mrvl_SHA1CircularShift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

static void Mrvl_SHA1PadMessage(Mrvl_SHA1_CTX *context);
static void Mrvl_SHA1ProcessMessageBlock(Mrvl_SHA1_CTX *context);

int Mrvl_SHA1Init(Mrvl_SHA1_CTX *context)
{
	if (context == NULL)
		return shaNull;

	context->Length_Low = 0;
	context->Length_High = 0;
	context->Message_Block_Index = 0;

	context->Intermediate_Hash[0] = 0x67452301;
	context->Intermediate_Hash[1] = 0xEFCDAB89;
	context->Intermediate_Hash[2] = 0x98BADCFE;
	context->Intermediate_Hash[3] = 0x10325476;
	context->Intermediate_Hash[4] = 0xC3D2E1F0;

	context->Computed = 0;
	context->Corrupted = 0;

	return shaSuccess;
}

int Mrvl_SHA1Final(Mrvl_SHA1_CTX *context, uint8_t *digest)
{
	int i;

	if (context == NULL || digest == NULL)
		return shaNull;

	if (context->Corrupted)
		return context->Corrupted;

	if (!context->Computed) {
		Mrvl_SHA1PadMessage(context);
		for (i = 0; i < 64; ++i)
			context->Message_Block[i] = 0;	/* the message may be sensitive */
		context->Length_Low = 0;
		context->Length_High = 0;
		context->Computed = 1;
	}

	for (i = 0; i < A_SHA_DIGEST_LEN; ++i)
		digest[i] = context->Intermediate_Hash[i >> 2] >> 8 * (3 - (i & 0x03));

	__aeabi_memclr4(context, sizeof(Mrvl_SHA1_CTX));

	return shaSuccess;
}

int Mrvl_SHA1Update(Mrvl_SHA1_CTX *context, const uint8_t *message, unsigned int length)
{
	if (!length)
		return shaSuccess;

	if (context == NULL || message == NULL)
		return shaNull;

	if (context->Computed) {
		context->Corrupted = shaStateError;
		return shaStateError;
	}

	if (context->Corrupted)
		return context->Corrupted;

	while (length-- && !context->Corrupted) {
		context->Message_Block[context->Message_Block_Index++] = *message & 0xFF;

		context->Length_Low += 8;
		if (context->Length_Low == 0) {
			context->Length_High++;
			if (context->Length_High == 0)
				context->Corrupted = 1;	/* the message is too long */
		}

		if (context->Message_Block_Index == 64)
			Mrvl_SHA1ProcessMessageBlock(context);

		message++;
	}

	return shaSuccess;
}

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
#define blk(i) (W[i & 15] = rol(W[(i + 13) & 15] ^ W[(i + 8) & 15] ^ W[(i + 2) & 15] ^ W[i & 15], 1))
#define blk0(i) ((i & 0x30) ? blk(i) : W[i])

/* The next 512 bits of the message. */
static void Mrvl_SHA1ProcessMessageBlock(Mrvl_SHA1_CTX *context)
{
	static const uint32_t K[] = {
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};
	int t;
	uint32_t temp;
	uint32_t *W;
	uint32_t A, B, C, D, E;

	W = context->Scratch;

	for (t = 0; t < 16; t++) {
		W[t] = context->Message_Block[t * 4] << 24;
		W[t] |= context->Message_Block[t * 4 + 1] << 16;
		W[t] |= context->Message_Block[t * 4 + 2] << 8;
		W[t] |= context->Message_Block[t * 4 + 3];
	}

	A = context->Intermediate_Hash[0];
	B = context->Intermediate_Hash[1];
	C = context->Intermediate_Hash[2];
	D = context->Intermediate_Hash[3];
	E = context->Intermediate_Hash[4];

	for (t = 0; t < 20; t++) {
		temp = Mrvl_SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + blk0(t) + K[0];
		E = D;
		D = C;
		C = Mrvl_SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 20; t < 40; t++) {
		temp = Mrvl_SHA1CircularShift(5, A) + (B ^ C ^ D) + E + blk(t) + K[1];
		E = D;
		D = C;
		C = Mrvl_SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 40; t < 60; t++) {
		temp = Mrvl_SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + blk(t) + K[2];
		E = D;
		D = C;
		C = Mrvl_SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}

	for (t = 60; t < 80; t++) {
		temp = Mrvl_SHA1CircularShift(5, A) + (B ^ C ^ D) + E + blk(t) + K[3];
		E = D;
		D = C;
		C = Mrvl_SHA1CircularShift(30, B);
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

/* The message is padded to a whole block, the last 64 bits holding its length. */
static void Mrvl_SHA1PadMessage(Mrvl_SHA1_CTX *context)
{
	if (context->Message_Block_Index > 55) {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 64)
			context->Message_Block[context->Message_Block_Index++] = 0;

		Mrvl_SHA1ProcessMessageBlock(context);

		while (context->Message_Block_Index < 56)
			context->Message_Block[context->Message_Block_Index++] = 0;
	} else {
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while (context->Message_Block_Index < 56)
			context->Message_Block[context->Message_Block_Index++] = 0;
	}

	context->Message_Block[56] = context->Length_High >> 24;
	context->Message_Block[57] = context->Length_High >> 16;
	context->Message_Block[58] = context->Length_High >> 8;
	context->Message_Block[59] = context->Length_High;
	context->Message_Block[60] = context->Length_Low >> 24;
	context->Message_Block[61] = context->Length_Low >> 16;
	context->Message_Block[62] = context->Length_Low >> 8;
	context->Message_Block[63] = context->Length_Low;

	Mrvl_SHA1ProcessMessageBlock(context);
}
