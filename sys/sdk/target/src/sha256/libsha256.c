/*
 * libSceSha256 (the SDK's static library, libsha256.o): SHA-256 by blocks. The compression is
 * Sony's hand-written core (sha256.S), which callers build with this file.
 */
#include <arm_neon.h>
#include <libsha256.h>
#include <scetypes.h>

/*
 * The 3.10 SDK's revision (SceLibMarlin links it, SDK 3.100.010) is libsha1's design with eight
 * state words: the context's usRemains and usComputed, the input gathered into a block of
 * big-endian words, and a hand-written core that compresses one block; 3.570.011's follows.
 */
#if defined(SCE_PSP2_SDK_VERSION) && SCE_PSP2_SDK_VERSION < 0x03570000
#include <stddef.h>
#include <stdint.h>

uint32_t *Sha256Transform(uint32_t block[16], uint32_t state[8]);

SceInt32 sceSha256BlockInit(SceSha256Context *pContext)
{
	if (pContext == NULL)
		return SCE_SHA256_ERROR_INVALID_POINTER;
	pContext->h[0] = 0x6a09e667;
	pContext->h[1] = 0xbb67ae85;
	pContext->h[2] = 0x3c6ef372;
	pContext->h[3] = 0xa54ff53a;
	pContext->h[4] = 0x510e527f;
	pContext->h[5] = 0x9b05688c;
	pContext->h[6] = 0x1f83d9ab;
	pContext->h[7] = 0x5be0cd19;
	pContext->usRemains = 0;
	pContext->usComputed = 0;
	pContext->ullTotalLen = 0;
	return 0;
}

SceInt32 sceSha256BlockUpdate(SceSha256Context *pContext, const void *plain, SceUInt32 len)
{
	const SceUChar8 *input = plain;
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	uint32_t total, remaining, pending, i;
	uint32_t *word;

	if (pContext == NULL || plain == NULL)
		return SCE_SHA256_ERROR_INVALID_POINTER;
	pending = pContext->usRemains;
	if (pending > 63)
		return SCE_SHA256_ERROR_INVALID_STATE;
	total = len + pending;
	pContext->ullTotalLen += len;
	if (total > 63) {
		remaining = total;
		for (;;) {
			if (pending != 0) {
				uint32_t consumed = SCE_SHA256_BLOCK_SIZE - pending;
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
				} while (pending != SCE_SHA256_BLOCK_SIZE);
				input += SCE_SHA256_BLOCK_SIZE;
			}
			word = block;
			do {
				*word = __builtin_bswap32(*word);
				word++;
			} while (word != block + 16);
			remaining -= SCE_SHA256_BLOCK_SIZE;
			Sha256Transform(block, pContext->h);
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

SceInt32 sceSha256BlockResult(SceSha256Context *pContext, SceUChar8 *digest)
{
	uint32_t state[8];
	uint32_t block[16];
	SceUChar8 *bytes = (SceUChar8 *)block;
	uint32_t *word;
	uint32_t i;

	if (pContext == NULL || digest == NULL)
		return SCE_SHA256_ERROR_INVALID_POINTER;
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
			Sha256Transform(block, state);
			for (word = block; word != block + 16; word++)
				*word = 0;
		}
		bitLength = pContext->ullTotalLen << 3;
		to = &bytes[55];
		for (shift = 56; shift != -8; shift -= 8)
			*++to = bitLength >> shift;
		for (word = block; word != block + 16; word++)
			*word = __builtin_bswap32(*word);
		Sha256Transform(block, state);
		for (word = state; word != state + 8; word++)
			*word = __builtin_bswap32(*word);
		for (i = 0; i != sizeof(state); i++)
			pContext->result[i] = ((const SceUChar8 *)state)[i];
		pContext->usComputed = 1;
	}
	for (i = 0; i != SCE_SHA256_DIGEST_SIZE; i++)
		digest[i] = pContext->result[i];
	return 0;
}

SceInt32 sceSha256Digest(const void *plain, SceUInt32 len, SceUChar8 *digest)
{
	SceSha256Context context;

	if (plain == NULL || digest == NULL)
		return SCE_SHA256_ERROR_INVALID_POINTER;
	sceSha256BlockInit(&context);
	sceSha256BlockUpdate(&context, plain, len);
	sceSha256BlockResult(&context, digest);
	return 0;
}
#else

const void *_sceSha256Core(const void *plain, unsigned int blocks, void *state);

static inline void _sceSha256SwapEndian(void *d, const void *s, SceUInt32 len)
{
	SceUInt32 i;
	const SceUInt32 *ps = (const SceUInt32 *)s;
	SceUInt32 *pd = (SceUInt32 *)d;

	for (i = 0; i < (len >> 2); i++) {
		pd[i] = __builtin_bswap32(ps[i]);
	}
}

static inline void _sceSha256Memcpy(void *pDst, const void *pSrc, SceUInt32 uiSize)
{
	SceUChar8 *pd = (SceUChar8 *)pDst;
	const SceUChar8 *ps = (const SceUChar8 *)pSrc;
	SceUInt32 i;

	for (i = 0; i < uiSize; i++) {
		pd[i] = ps[i];
	}
}

static inline void _sceSha256Memcpy32(void *pDst, const void *pSrc, SceUInt32 uiSize)
{
	SceUInt32 *pd = (SceUInt32 *)pDst;
	const SceUInt32 *ps = (const SceUInt32 *)pSrc;
	SceUInt32 i;

	for (i = 0; i < (uiSize >> 2); i++) {
		pd[i] = ps[i];
	}
}

__attribute__((noinline))
SceInt32 sceSha256BlockInit(SceSha256Context *pContext)
{
	static const SceUInt32 iv[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	if (pContext == 0) {
		return SCE_SHA256_ERROR_INVALID_POINTER;
	}
	_sceSha256Memcpy32(pContext->h, iv, sizeof(iv));
	pContext->ullTotalLen = 0;
	pContext->buf[SCE_SHA256_BLOCK_SIZE - 1] = 0;
	return 0;
}

SceInt32 sceSha256BlockUpdate(SceSha256Context *pContext, const void *plain, SceUInt32 len)
{
	SceUInt32 n;
	const SceUChar8 *pInput = (const SceUChar8 *)plain;

	if (pContext == 0 || plain == 0) {
		return SCE_SHA256_ERROR_INVALID_POINTER;
	}
	pContext->ullTotalLen += len;
	n = pContext->buf[SCE_SHA256_BLOCK_SIZE - 1] & (SCE_SHA256_BLOCK_SIZE - 1);
	if (n != 0) {
		SceUInt32 cont = SCE_SHA256_BLOCK_SIZE - n;
		if (len < cont) {
			goto not_enough;
		}
		len -= cont;
		_sceSha256Memcpy(pContext->buf + n, pInput, cont);
		pInput += cont;
		_sceSha256Core(pContext->buf, 1, pContext->h);
	}
	n = len >> 6;
	if (n != 0) {
		pInput = _sceSha256Core(pInput, n, pContext->h);
		len &= (SCE_SHA256_BLOCK_SIZE - 1);
		n = 0;
	}
not_enough:
	if (len != 0) {
		_sceSha256Memcpy(pContext->buf + n, pInput, len);
		n += len;
	}
	pContext->buf[SCE_SHA256_BLOCK_SIZE - 1] = n;
	return 0;
}

SceInt32 sceSha256BlockResult(SceSha256Context *pContext, SceUChar8 *digest)
{
	SceUInt32 n;

	if (pContext == 0 || digest == 0) {
		return SCE_SHA256_ERROR_INVALID_POINTER;
	}
	n = pContext->buf[SCE_SHA256_BLOCK_SIZE - 1];
	if (n < SCE_SHA256_BLOCK_SIZE) {
		union {
			SceUChar8 b[128];
			SceUInt64 ull[16];
		} blk;
		SceUInt32 len, i, *p;

		pContext->buf[SCE_SHA256_BLOCK_SIZE - 1] = n + 0x80;
		_sceSha256Memcpy32(pContext->result, pContext->h, 32);
		_sceSha256Memcpy32(blk.b, pContext->buf, 128);
		len = (n + 72) >> 6;
		p = (SceUInt32 *)(blk.b + n);
		*p++ = 0x80;
		p = (SceUInt32 *)((SceUIntPtr)p & ~3);
		for (i = (n >> 2); i < (len << 4) - 3; i++) {
			*p++ = 0;
		}
		*(uint8x8_t *)p = vrev64_u8((uint8x8_t)vshl_n_u64((uint64x1_t)pContext->ullTotalLen, 3));
		_sceSha256Core(blk.b, len, pContext->result);
	}
	_sceSha256SwapEndian(digest, pContext->result, SCE_SHA256_DIGEST_SIZE);
	return 0;
}

SceInt32 sceSha256Digest(const void *plain, SceUInt32 len, SceUChar8 *digest)
{
	SceSha256Context ctx;
	int res;

	sceSha256BlockInit(&ctx);
	res = sceSha256BlockUpdate(&ctx, plain, len);
	if (res >= 0) {
		res = sceSha256BlockResult(&ctx, digest);
	}
	return res;
}
#endif
