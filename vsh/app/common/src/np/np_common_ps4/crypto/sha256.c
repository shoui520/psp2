/*
 * SceNpCommonPs4: SHA-256 (the block function is NEON code, crypto/sha256_block.S).
 */

#include <arm_neon.h>
#include "crypto.h"

#define NP_COMMON_PS4_SHA256_ERROR_INVALID_ARGUMENT	-2142763776	/* 0x80415000 */

SceUInt32 g_npCommonPs4Sha256Init[8] = {		/* 0x8102abd8 */
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

/* the round constants as the NEON code reads them: those of rounds 0-7 and 56-63 first (they are loaded
   into registers ahead of the rounds), then those of rounds 8-55 */
const SceUInt32 g_npCommonPs4Sha256K[64] = {		/* 0x8102abf8 */
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3
};

#define ROTATE_RIGHT(value, bits)	(((value) >> (bits)) | ((value) << (32 - (bits))))

/* PLACEHOLDER: the compression function of SHA-256 in plain C. Sony's is hand-written NEON code
   interleaved with core-register code (recorded in DEFERRED.md); this one computes the same state. */
const void *npCommonPs4Sha256Transform(const void *data, SceUInt32 blocks, SceUInt32 *state)
{
	const SceUInt8 *src;
	SceUInt32 w[64];
	SceUInt32 a, b, c, d, e, f, g, h;
	SceUInt32 t1, t2;
	SceUInt32 i;

	src = (const SceUInt8 *)data;
	while (blocks != 0) {
		for (i = 0; i < 16; i++, src += 4)
			w[i] = ((SceUInt32)src[0] << 24) | ((SceUInt32)src[1] << 16) | ((SceUInt32)src[2] << 8) | src[3];
		for (; i < 64; i++) {
			t1 = ROTATE_RIGHT(w[i - 15], 7) ^ ROTATE_RIGHT(w[i - 15], 18) ^ (w[i - 15] >> 3);
			t2 = ROTATE_RIGHT(w[i - 2], 17) ^ ROTATE_RIGHT(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + t1 + w[i - 7] + t2;
		}
		a = state[0]; b = state[1]; c = state[2]; d = state[3];
		e = state[4]; f = state[5]; g = state[6]; h = state[7];
		for (i = 0; i < 64; i++) {
			t1 = h + (ROTATE_RIGHT(e, 6) ^ ROTATE_RIGHT(e, 11) ^ ROTATE_RIGHT(e, 25)) + ((e & f) ^ (~e & g))
				+ g_npCommonPs4Sha256K[i < 8 ? i : (i < 56 ? i + 8 : i - 48)] + w[i];
			t2 = (ROTATE_RIGHT(a, 2) ^ ROTATE_RIGHT(a, 13) ^ ROTATE_RIGHT(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d;
		state[4] += e; state[5] += f; state[6] += g; state[7] += h;
		blocks--;
	}
	return src;
}

/*SORT-START*/
__attribute__((noinline)) int npCommonPs4Sha256Init(struct NpCommonPs4Sha256Context *context)
{
	int i;

	if (context == NULL)
		return NP_COMMON_PS4_SHA256_ERROR_INVALID_ARGUMENT;
	for (i = 0; i != 8; ++i)
		context->state[i] = g_npCommonPs4Sha256Init[i];
	context->buffer[63] = 0;
	context->length = 0;
	return 0;
}

__attribute__((noinline)) int npCommonPs4Sha256Update(struct NpCommonPs4Sha256Context *context, const void *data, SceSize size)
{
	const SceUInt8 *src;
	SceSize length;
	SceSize used;
	SceSize space;
	SceSize i;
	SceUInt8 *dst;

	src = (const SceUInt8 *)data;
	length = size;
	if (context == NULL || data == NULL)
		return NP_COMMON_PS4_SHA256_ERROR_INVALID_ARGUMENT;
	used = context->buffer[63] & 0x3F;
	context->length += size;
	if (used == 0)
		goto blocks;
	space = 64 - used;
	if (length >= space) {
		length -= space;
		dst = &context->buffer[used];
		i = 0;
		do {
			dst[i] = src[i];
			++i;
		} while (i < space);
		src += space;
		npCommonPs4Sha256Transform(context->buffer, 1, context->state);
blocks:
		used = length >> 6;
		if (used != 0) {
			length &= 0x3F;
			src = (const SceUInt8 *)npCommonPs4Sha256Transform(src, used, context->state);
			used = 0;
		}
	}
	if (length != 0) {
		for (i = 0; i != length; ++i)
			context->buffer[used + i] = src[i];
		used += i;
	}
	context->buffer[63] = used;
	return 0;
}

__attribute__((noinline)) int npCommonPs4Sha256Final(struct NpCommonPs4Sha256Context *context, void *digest)
{
	SceUInt8 block[132];
	SceUInt32 used;
	SceUInt32 blocks;
	SceUInt32 words;
	SceUInt8 *state;
	int i;
	int j;
	int8x8_t *zero;

	if (context == NULL || digest == NULL)
		return NP_COMMON_PS4_SHA256_ERROR_INVALID_ARGUMENT;
	used = context->buffer[63];
	state = (SceUInt8 *)context + 112;
	if (used <= 0x3F) {
		context->buffer[63] = used + 0x80;
		for (i = 0; i != 32; i += 4)
			*(SceUInt32 *)(state + i) = *(const SceUInt32 *)((const SceUInt8 *)context + i);
		for (j = 0; j != 128; j += 4)
			*(SceUInt32 *)&block[j] = *(const SceUInt32 *)((const SceUInt8 *)context + 48 + j);
		blocks = (used + 72) >> 6;
		*(SceUInt32 *)&block[used] = 128;
		words = used >> 2;
		zero = (int8x8_t *)((SceUIntPtr)&block[used + 4] & 0xFFFFFFFC);
		while (words < 16 * blocks - 3) {
			++words;
			*(SceUInt32 *)zero = 0;
			zero = (int8x8_t *)((SceUInt8 *)zero + 4);
		}
		*zero = vrev64_s8((int8x8_t)vshl_n_s64((int64x1_t)context->length, 3));
		npCommonPs4Sha256Transform(block, blocks, (SceUInt32 *)state);
	}
	for (i = 0; i != 32; i += 4)
		*(SceUInt32 *)((SceUInt8 *)digest + i) = __builtin_bswap32(*(const SceUInt32 *)(state + i));
	return 0;
}

int npCommonPs4Sha256HashKey(const void *data, SceSize size, void *digest)
{
	struct NpCommonPs4Sha256Context context;
	int result;

	npCommonPs4Sha256Init(&context);
	result = npCommonPs4Sha256Update(&context, data, size);
	if (result >= 0)
		return npCommonPs4Sha256Final(&context, digest);
	return result;
}
/*SORT-END*/
