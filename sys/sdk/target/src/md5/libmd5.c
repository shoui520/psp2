/*
 * libSceMd5 (the SDK's static library, libmd5.o): MD5 by blocks. The compression is Sony's
 * hand-written core (md5.S), which callers build with this file.
 */
#include <libmd5.h>
#include <stddef.h>
#include <stdint.h>

void _sceMd5Transform(uint32_t state[4], const SceUChar8 block[SCE_MD5_BLOCK_SIZE]);

SceInt32 sceMd5BlockInit(SceMd5Context *pContext)
{
	if (pContext == NULL)
		return SCE_MD5_ERROR_INVALID_POINTER;
	pContext->h[0] = 0x67452301;
	pContext->h[1] = 0xefcdab89;
	pContext->h[2] = 0x98badcfe;
	pContext->h[3] = 0x10325476;
	pContext->ullTotalLen = 0;
	pContext->usRemains = 0;
	pContext->usComputed = 0;
	return 0;
}

SceInt32 sceMd5BlockUpdate(SceMd5Context *pContext, const void *plain, SceUInt32 len)
{
	const SceUChar8 *input = plain;
	uint32_t total, remaining, pending;

	if (pContext == NULL || plain == NULL)
		return SCE_MD5_ERROR_INVALID_POINTER;
	pending = pContext->usRemains;
	if (pending > 63)
		return SCE_MD5_ERROR_INVALID_STATE;
	total = len + pending;
	pContext->ullTotalLen += len;
	if (total > 63) {
		remaining = total;
		for (;;) {
			if (pending != 0) {
				uint32_t consumed = SCE_MD5_BLOCK_SIZE - pending;
				SceUChar8 *to = &pContext->buf[pending];
				SceUChar8 *end = to + consumed;
				const SceUChar8 *from = input;

				if (consumed != 0) {
					do
						*to++ = *from++;
					while (to != end);
				}
				input += consumed;
				pContext->usRemains = 0;
			} else {
				const SceUChar8 *end = input + SCE_MD5_BLOCK_SIZE;
				SceUChar8 *to = pContext->buf;

				do
					*to++ = *input++;
				while (input != end);
			}
			remaining -= SCE_MD5_BLOCK_SIZE;
			_sceMd5Transform(pContext->h, pContext->buf);
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

SceInt32 sceMd5BlockResult(SceMd5Context *pContext, SceUChar8 *digest)
{
	uint32_t state[4];
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	const SceUChar8 *from;
	SceUChar8 *to;
	uint32_t *word;

	if (pContext == NULL || digest == NULL)
		return SCE_MD5_ERROR_INVALID_POINTER;
	if (pContext->usComputed == 0) {
		uint32_t pending = pContext->usRemains;
		uint64_t bitLength;
		uint32_t i;

		for (word = block; word != block + 16; word++)
			*word = 0;
		for (i = 0; i < pending; i++)
			bytes[i] = pContext->buf[i];
		bytes[pending] = 0x80;
		for (from = (const SceUChar8 *)pContext->h, to = (SceUChar8 *)state;
		     to != (SceUChar8 *)(state + 4); )
			*to++ = *from++;
		if (pending > 55) {
			_sceMd5Transform(state, bytes);
			for (word = block; word != block + 16; word++)
				*word = 0;
		}
		bitLength = pContext->ullTotalLen << 3;
		for (i = 0; i < 8; i++)
			bytes[56 + i] = bitLength >> (i * 8);
		_sceMd5Transform(state, bytes);
		for (from = (const SceUChar8 *)state, to = pContext->result;
		     from != (const SceUChar8 *)(state + 4); )
			*to++ = *from++;
		pContext->usComputed = 1;
	}
	for (from = pContext->result, to = digest; to != digest + SCE_MD5_DIGEST_SIZE; )
		*to++ = *from++;
	return 0;
}

SceInt32 sceMd5Digest(const void *plain, SceUInt32 len, unsigned char *digest)
{
	SceMd5Context context;

	if (plain == NULL || digest == NULL)
		return SCE_MD5_ERROR_INVALID_POINTER;
	sceMd5BlockInit(&context);
	sceMd5BlockUpdate(&context, plain, len);
	sceMd5BlockResult(&context, digest);
	return 0;
}
