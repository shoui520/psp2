/*
 * libSceSha1 (the SDK's static library, libsha1.o): SHA-1 by blocks. The compression is
 * Sony's hand-written core, the same object as SceKernelUtils' (sysmem's sha1_transform.S),
 * which callers build with this file.
 */
#include <libsha1.h>
#include <stddef.h>
#include <stdint.h>

uint32_t *Sha1Transform(uint32_t block[16], uint32_t state[5]);

/*
 * The 3.10 SDK's revision of the block calls (SceLibMarlin links it, SDK 3.100.010): pointers
 * tested context first, the input copied byte by byte, index loops; 3.570.011's follows.
 */
#if defined(SCE_PSP2_SDK_VERSION) && SCE_PSP2_SDK_VERSION < 0x03570000
#define SCE_SHA1_SDK_310	1
#else
#define SCE_SHA1_SDK_310	0
#endif

SceInt32 sceSha1BlockInit(SceSha1Context *pContext)
{
	if (pContext == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	pContext->h[0] = 0x67452301;
	pContext->h[1] = 0xefcdab89;
	pContext->h[2] = 0x98badcfe;
	pContext->h[3] = 0x10325476;
	pContext->h[4] = 0xc3d2e1f0;
	pContext->ullTotalLen = 0;
	pContext->usRemains = 0;
	pContext->usComputed = 0;
	return 0;
}

#if SCE_SHA1_SDK_310
SceInt32 sceSha1BlockUpdate(SceSha1Context *pContext, const void *plain, SceUInt32 len)
{
	const SceUChar8 *input = plain;
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	uint32_t total, remaining, pending, i;
	uint32_t *word;

	if (pContext == NULL || plain == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	pending = pContext->usRemains;
	if (pending > 63)
		return SCE_SHA1_ERROR_INVALID_STATE;
	total = len + pending;
	pContext->ullTotalLen += len;
	if (total > 63) {
		remaining = total;
		for (;;) {
			if (pending != 0) {
				uint32_t consumed = SCE_SHA1_BLOCK_SIZE - pending;
				const SceUChar8 *from = input;
				SceUChar8 *to, *end;

				for (i = 0; i < pending; i++)
					bytes[i] = pContext->buf[i];
				to = &bytes[pending];
				end = to + consumed;
				if (consumed != 0) {
					do
						*to++ = *from++;
					while (to != end);
				}
				pContext->usRemains = 0;
				input += consumed;
			} else {
				do {
					bytes[pending] = input[pending];
					pending++;
				} while (pending != SCE_SHA1_BLOCK_SIZE);
				input += SCE_SHA1_BLOCK_SIZE;
			}
			word = block;
			do {
				*word = __builtin_bswap32(*word);
				word++;
			} while (word != block + 16);
			remaining -= SCE_SHA1_BLOCK_SIZE;
			Sha1Transform(block, pContext->h);
			if (remaining <= 63)
				break;
			pending = pContext->usRemains;
		}
		total &= 63;
	}
	if (total != 0) {
		uint32_t count = total - pContext->usRemains;
		SceUChar8 *to = &pContext->buf[pContext->usRemains];

		if (count != 0) {
			SceUChar8 *end = to + count;

			do
				*to++ = *input++;
			while (to != end);
		}
	}
	pContext->usRemains = total;
	pContext->usComputed = 0;
	return 0;
}
#else
SceInt32 sceSha1BlockUpdate(SceSha1Context *pContext, const void *plain, SceUInt32 len)
{
	const SceUChar8 *input = plain;
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	uint32_t total, remaining, pending, i;

	if (plain == NULL || pContext == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	pending = pContext->usRemains;
	if (pending > 63)
		return SCE_SHA1_ERROR_INVALID_STATE;
	total = len + pending;
	pContext->ullTotalLen += len;
	if (total > 63) {
		remaining = total;
		for (;;) {
			if (pending != 0) {
				uint32_t consumed = SCE_SHA1_BLOCK_SIZE - pending;
				const SceUChar8 *end;
				SceUChar8 *to;
				uint32_t *word;

				for (i = 0; i < pending; i++)
					bytes[i] = pContext->buf[i];
				end = input + consumed;
				to = &bytes[pending];
				if (consumed != 0) {
					do
						*to++ = *input++;
					while (input != end);
				}
				pContext->usRemains = 0;
				word = block;
				do {
					*word = __builtin_bswap32(*word);
					word++;
				} while (word != block + 16);
				remaining -= SCE_SHA1_BLOCK_SIZE;
				Sha1Transform(block, pContext->h);
				if (remaining <= 63)
					break;
			} else {
				do {
					*(uint32_t *)&bytes[pending] =
						__builtin_bswap32(*(const uint32_t *)&input[pending]);
					pending += sizeof(uint32_t);
				} while (pending != SCE_SHA1_BLOCK_SIZE);
				remaining -= SCE_SHA1_BLOCK_SIZE;
				__builtin_prefetch(input + 95);
				__builtin_prefetch(input + 127);
				input += SCE_SHA1_BLOCK_SIZE;
				Sha1Transform(block, pContext->h);
				if (remaining <= 63)
					break;
			}
			pending = pContext->usRemains;
		}
		total &= 63;
	}
	if (total != 0) {
		uint32_t count = total - pContext->usRemains;
		SceUChar8 *to = &pContext->buf[pContext->usRemains];

		if (count != 0) {
			SceUChar8 *end = to + count;

			do
				*to++ = *input++;
			while (to != end);
		}
	}
	pContext->usRemains = total;
	pContext->usComputed = 0;
	return 0;
}
#endif

#if SCE_SHA1_SDK_310
SceInt32 sceSha1BlockResult(SceSha1Context *pContext, SceUChar8 *digest)
{
	uint32_t state[5];
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	uint32_t *word;
	uint32_t i;

	if (pContext == NULL || digest == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	if (pContext->usComputed == 0) {
		uint32_t pending = pContext->usRemains;
		uint64_t bitLength;
		SceUChar8 *to;
		int shift;

		for (word = block; word != block + 16; word++)
			*word = 0;
		for (i = 0; i < pending; i++)
			bytes[i] = pContext->buf[i];
		bytes[pending] = 0x80;
		for (i = 0; i != sizeof(state); i++)
			((SceUChar8 *)state)[i] = ((const SceUChar8 *)pContext->h)[i];
		if (pending > 55) {
			for (word = block; word != block + 16; word++)
				*word = __builtin_bswap32(*word);
			Sha1Transform(block, state);
			for (word = block; word != block + 16; word++)
				*word = 0;
		}
		bitLength = pContext->ullTotalLen << 3;
		to = &bytes[55];
		for (shift = 56; shift != -8; shift -= 8)
			*++to = bitLength >> shift;
		for (word = block; word != block + 16; word++)
			*word = __builtin_bswap32(*word);
		Sha1Transform(block, state);
		for (word = state; word != state + 5; word++)
			*word = __builtin_bswap32(*word);
		for (i = 0; i != sizeof(state); i++)
			pContext->result[i] = ((const SceUChar8 *)state)[i];
		pContext->usComputed = 1;
	}
	for (i = 0; i != SCE_SHA1_DIGEST_SIZE; i++)
		digest[i] = pContext->result[i];
	return 0;
}
#else
SceInt32 sceSha1BlockResult(SceSha1Context *pContext, SceUChar8 *digest)
{
	uint32_t state[5];
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	const SceUChar8 *from;
	SceUChar8 *to;
	uint32_t *word;

	if (digest == NULL || pContext == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	if (pContext->usComputed == 0) {
		uint32_t pending = pContext->usRemains;
		uint64_t bitLength;
		uint32_t i;
		int shift;

		for (word = block; word != block + 16; word++)
			*word = 0;
		for (i = 0; i < pending; i++)
			bytes[i] = pContext->buf[i];
		bytes[pending] = 0x80;
		for (from = (const SceUChar8 *)pContext->h, to = (SceUChar8 *)state;
		     to != (SceUChar8 *)(state + 5); )
			*to++ = *from++;
		if (pending > 55) {
			for (word = block; word != block + 16; word++)
				*word = __builtin_bswap32(*word);
			Sha1Transform(block, state);
			for (word = block; word != block + 16; word++)
				*word = 0;
		}
		bitLength = pContext->ullTotalLen << 3;
		to = &bytes[55];
		for (shift = 56; shift != -8; shift -= 8)
			*++to = bitLength >> shift;
		for (word = block; word != block + 16; word++)
			*word = __builtin_bswap32(*word);
		Sha1Transform(block, state);
		for (word = state; word != state + 5; word++)
			*word = __builtin_bswap32(*word);
		for (from = (const SceUChar8 *)state, to = pContext->result;
		     from != (const SceUChar8 *)(state + 5); )
			*to++ = *from++;
		pContext->usComputed = 1;
	}
	for (from = pContext->result, to = digest; to != digest + SCE_SHA1_DIGEST_SIZE; )
		*to++ = *from++;
	return 0;
}
#endif

SceInt32 sceSha1Digest(const void *plain, SceUInt32 len, SceUChar8 *digest)
{
	SceSha1Context context;

	if (plain == NULL || digest == NULL)
		return SCE_SHA1_ERROR_INVALID_POINTER;
	sceSha1BlockInit(&context);
	sceSha1BlockUpdate(&context, plain, len);
	sceSha1BlockResult(&context, digest);
	return 0;
}
