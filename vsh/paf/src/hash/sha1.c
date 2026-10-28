#include "sha1.h"

#define SHA1CircularShift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

int32_t SHA1Reset(SHA1Context *context)
{
	if (!context)
		return shaNull;
	context->Length_Low = 0;
	context->Length_High = 0;
	context->Message_Block_Index = 0;
	context->Intermediate_Hash[0] = 0x67452301;
	context->Intermediate_Hash[1] = 0xefcdab89;
	context->Intermediate_Hash[2] = 0x98badcfe;
	context->Intermediate_Hash[3] = 0x10325476;
	context->Intermediate_Hash[4] = 0xc3d2e1f0;
	context->Computed = 0;
	context->Corrupted = 0;
	return shaSuccess;
}

void SHA1ProcessMessageBlock(SHA1Context *context)
{
	const uint32_t K[] = { 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6 };
	uint32_t W[80], A, B, C, D, E, temp;
	int t;

	for (t = 0; t < 16; t++) {
		W[t] = (uint32_t)context->Message_Block[t * 4] << 24;
		W[t] |= (uint32_t)context->Message_Block[t * 4 + 1] << 16;
		W[t] |= (uint32_t)context->Message_Block[t * 4 + 2] << 8;
		W[t] |= context->Message_Block[t * 4 + 3];
	}
	for (t = 16; t < 80; t++)
		W[t] = SHA1CircularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
	A = context->Intermediate_Hash[0];
	B = context->Intermediate_Hash[1];
	C = context->Intermediate_Hash[2];
	D = context->Intermediate_Hash[3];
	E = context->Intermediate_Hash[4];
	for (t = 0; t < 20; t++) {
		temp = SHA1CircularShift(5, A) + ((B & C) | (~B & D)) + E + W[t] + K[0];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}
	for (t = 20; t < 40; t++) {
		temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}
	for (t = 40; t < 60; t++) {
		temp = SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		E = D;
		D = C;
		C = SHA1CircularShift(30, B);
		B = A;
		A = temp;
	}
	for (t = 60; t < 80; t++) {
		temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
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

void SHA1PadMessage(SHA1Context *context)
{
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
	context->Message_Block[56] = context->Length_High >> 24;
	context->Message_Block[57] = context->Length_High >> 16;
	context->Message_Block[58] = context->Length_High >> 8;
	context->Message_Block[59] = context->Length_High;
	context->Message_Block[60] = context->Length_Low >> 24;
	context->Message_Block[61] = context->Length_Low >> 16;
	context->Message_Block[62] = context->Length_Low >> 8;
	context->Message_Block[63] = context->Length_Low;
	SHA1ProcessMessageBlock(context);
}

int32_t SHA1Result(SHA1Context *context, uint8_t message_digest[SHA1HashSize])
{
	int i;

	if (!context || !message_digest)
		return shaNull;
	if (context->Corrupted)
		return context->Corrupted;
	if (!context->Computed) {
		SHA1PadMessage(context);
		for (i = 0; i < 64; i++)
			context->Message_Block[i] = 0;
		context->Length_Low = 0;
		context->Length_High = 0;
		context->Computed = 1;
	}
	for (i = 0; i < SHA1HashSize; i++)
		message_digest[i] = context->Intermediate_Hash[i >> 2] >> (8 * (3 - (i & 3)));
	return shaSuccess;
}

int32_t SHA1Input(SHA1Context *context, const uint8_t *message_array, size_t length)
{
	if (!length)
		return shaSuccess;
	if (!context || !message_array)
		return shaNull;
	if (context->Computed) {
		context->Corrupted = shaStateError;
		return shaStateError;
	}
	if (context->Corrupted)
		return context->Corrupted;
	while (length-- && !context->Corrupted) {
		context->Message_Block[context->Message_Block_Index++] = *message_array;
		context->Length_Low += 8;
		if (!context->Length_Low) {
			context->Length_High++;
			if (!context->Length_High)
				context->Corrupted = 1;
		}
		if (context->Message_Block_Index == 64)
			SHA1ProcessMessageBlock(context);
		message_array++;
	}
	return shaSuccess;
}
