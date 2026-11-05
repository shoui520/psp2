/*
 * libSceSha224 (the SDK's static library, libsha224.o): SHA-224 by blocks. The compression is
 * Sony's hand-written core (sha224.S), which callers build with this file.
 */
#include <arm_neon.h>
#include <libsha224.h>
#include <scetypes.h>

const void *_sceSha224Core(const void *plain, unsigned int blocks, void *state);

static inline void _sceSha224SwapEndian(void *d, const void *s, SceUInt32 len)
{
	SceUInt32 i;
	const SceUInt32 *ps = (const SceUInt32 *)s;
	SceUInt32 *pd = (SceUInt32 *)d;

	for (i = 0; i < (len >> 2); i++) {
		pd[i] = __builtin_bswap32(ps[i]);
	}
}

static inline void _sceSha224Memcpy(void *pDst, const void *pSrc, SceUInt32 uiSize)
{
	SceUChar8 *pd = (SceUChar8 *)pDst;
	const SceUChar8 *ps = (const SceUChar8 *)pSrc;
	SceUInt32 i;

	for (i = 0; i < uiSize; i++) {
		pd[i] = ps[i];
	}
}

static inline void _sceSha224Memcpy32(void *pDst, const void *pSrc, SceUInt32 uiSize)
{
	SceUInt32 *pd = (SceUInt32 *)pDst;
	const SceUInt32 *ps = (const SceUInt32 *)pSrc;
	SceUInt32 i;

	for (i = 0; i < (uiSize >> 2); i++) {
		pd[i] = ps[i];
	}
}

__attribute__((noinline))
SceInt32 sceSha224BlockInit(SceSha224Context *pContext)
{
	static const SceUInt32 iv[8] = {
		0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
		0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
	};

	if (pContext == 0) {
		return SCE_SHA224_ERROR_INVALID_POINTER;
	}
	_sceSha224Memcpy32(pContext->h, iv, sizeof(iv));
	pContext->ullTotalLen = 0;
	pContext->buf[SCE_SHA224_BLOCK_SIZE - 1] = 0;
	return 0;
}

SceInt32 sceSha224BlockUpdate(SceSha224Context *pContext, const void *plain, SceUInt32 len)
{
	SceUInt32 n;
	const SceUChar8 *pInput = (const SceUChar8 *)plain;

	if (pContext == 0 || plain == 0) {
		return SCE_SHA224_ERROR_INVALID_POINTER;
	}
	pContext->ullTotalLen += len;
	n = pContext->buf[SCE_SHA224_BLOCK_SIZE - 1] & (SCE_SHA224_BLOCK_SIZE - 1);
	if (n != 0) {
		SceUInt32 cont = SCE_SHA224_BLOCK_SIZE - n;
		if (len < cont) {
			goto not_enough;
		}
		len -= cont;
		_sceSha224Memcpy(pContext->buf + n, pInput, cont);
		pInput += cont;
		_sceSha224Core(pContext->buf, 1, pContext->h);
	}
	n = len >> 6;
	if (n != 0) {
		pInput = _sceSha224Core(pInput, n, pContext->h);
		len &= (SCE_SHA224_BLOCK_SIZE - 1);
		n = 0;
	}
not_enough:
	if (len != 0) {
		_sceSha224Memcpy(pContext->buf + n, pInput, len);
		n += len;
	}
	pContext->buf[SCE_SHA224_BLOCK_SIZE - 1] = n;
	return 0;
}

SceInt32 sceSha224BlockResult(SceSha224Context *pContext, SceUChar8 *digest)
{
	SceUInt32 n;

	if (pContext == 0 || digest == 0) {
		return SCE_SHA224_ERROR_INVALID_POINTER;
	}
	n = pContext->buf[SCE_SHA224_BLOCK_SIZE - 1];
	if (n < SCE_SHA224_BLOCK_SIZE) {
		union {
			SceUChar8 b[128];
			SceUInt64 ull[16];
		} blk;
		SceUInt32 len, i, *p;

		pContext->buf[SCE_SHA224_BLOCK_SIZE - 1] = n + 0x80;
		_sceSha224Memcpy32(pContext->result, pContext->h, 32);
		_sceSha224Memcpy32(blk.b, pContext->buf, 128);
		len = (n + 72) >> 6;
		p = (SceUInt32 *)(blk.b + n);
		*p++ = 0x80;
		p = (SceUInt32 *)((SceUIntPtr)p & ~3);
		for (i = (n >> 2); i < (len << 4) - 3; i++) {
			*p++ = 0;
		}
		*(uint8x8_t *)p = vrev64_u8((uint8x8_t)vshl_n_u64((uint64x1_t)pContext->ullTotalLen, 3));
		_sceSha224Core(blk.b, len, pContext->result);
	}
	_sceSha224SwapEndian(digest, pContext->result, SCE_SHA224_DIGEST_SIZE);
	return 0;
}

SceInt32 sceSha224Digest(const void *plain, SceUInt32 len, SceUChar8 *digest)
{
	SceSha224Context ctx;
	int res;

	sceSha224BlockInit(&ctx);
	res = sceSha224BlockUpdate(&ctx, plain, len);
	if (res >= 0) {
		res = sceSha224BlockResult(&ctx, digest);
	}
	return res;
}
