/*
 * libSceSha384 (the SDK's static library, libsha384.o): SHA-384 by blocks. The compression is
 * Sony's hand-written core (sha384.S), which callers build with this file.
 */
#include <libsha384.h>
#include <scetypes.h>

void _sceSha384Core(const SceUInt64 *plain, SceUInt64 *state);

static inline void _sceSha384SwapEndian(SceUInt64 *dst, const SceUInt64 *src, SceUInt32 len)
{
	SceUInt32 i;

	for (i = 0; i < (len >> 3); i++) {
		union {
			SceUInt64 ull;
			SceUInt32 ui[2];
		} s, d;

		s.ull = src[i];
		d.ui[0] = __builtin_bswap32(s.ui[1]);
		d.ui[1] = __builtin_bswap32(s.ui[0]);
		dst[i] = d.ull;
	}
}

SceInt32 sceSha384Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);

inline SceInt32 sceSha384BlockInit(SceSha384Context *pContext)
{
	if (pContext == 0) {
		return SCE_SHA384_ERROR_INVALID_POINTER;
	}
	pContext->h[0] = 0xcbbb9d5dc1059ed8ULL;
	pContext->h[1] = 0x629a292a367cd507ULL;
	pContext->h[2] = 0x9159015a3070dd17ULL;
	pContext->h[3] = 0x152fecd8f70e5939ULL;
	pContext->h[4] = 0x67332667ffc00b31ULL;
	pContext->h[5] = 0x8eb44a8768581511ULL;
	pContext->h[6] = 0xdb0c2e0d64f98fa7ULL;
	pContext->h[7] = 0x47b5481dbefa4fa4ULL;
	pContext->usRemains = 0;
	pContext->usComputed = 0;
	pContext->ullTotalLen = 0;
	return 0;
}

SceInt32 sceSha384BlockUpdate(SceSha384Context *pContext, const void *plain, SceUInt32 len)
{
	const SceUChar8 *input = plain;
	union {
		SceUChar8 b[128];
		SceUInt32 ui[32];
		SceUInt64 ull[16];
	} blk;
	SceUInt32 total, remaining, pending, i;

	if (pContext == 0 || plain == 0) {
		return SCE_SHA384_ERROR_INVALID_POINTER;
	}
	pending = pContext->usRemains;
	if (pending >= SCE_SHA384_BLOCK_SIZE) {
		return SCE_SHA384_ERROR_INVALID_STATE;
	}
	total = len + pending;
	pContext->ullTotalLen += len;
	if (total >= SCE_SHA384_BLOCK_SIZE) {
		remaining = total;
		for (;;) {
			if (pending != 0) {
				SceUInt32 cont = SCE_SHA384_BLOCK_SIZE - pending;
				const SceUChar8 *end;
				SceUChar8 *to;

				for (i = 0; i < pending; i++) {
					blk.b[i] = pContext->buf[i];
				}
				end = input + cont;
				to = &blk.b[pending];
				if (cont != 0) {
					do {
						*to++ = *input++;
					} while (input != end);
				}
				pContext->usRemains = 0;
			} else {
				const SceUChar8 *from = input;
				SceUChar8 *to = blk.b;

				do {
					*to++ = *from++;
				} while (to != blk.b + SCE_SHA384_BLOCK_SIZE);
				input += SCE_SHA384_BLOCK_SIZE;
			}
			_sceSha384SwapEndian(blk.ull, blk.ull, SCE_SHA384_BLOCK_SIZE);
			remaining -= SCE_SHA384_BLOCK_SIZE;
			_sceSha384Core(blk.ull, pContext->h);
			if (remaining < SCE_SHA384_BLOCK_SIZE) {
				break;
			}
			pending = pContext->usRemains;
		}
		total &= (SCE_SHA384_BLOCK_SIZE - 1);
	}
	if (total != 0) {
		SceUInt32 count = total - pContext->usRemains;
		SceUChar8 *to = &pContext->buf[pContext->usRemains];

		if (count != 0) {
			SceUChar8 *end = to + count;

			do {
				*to++ = *input++;
			} while (to != end);
		}
	}
	pContext->usRemains = total;
	pContext->usComputed = 0;
	return 0;
}

SceInt32 sceSha384BlockResult(SceSha384Context *pContext, SceUChar8 *digest)
{
	const SceUChar8 *from;
	SceUChar8 *to;

	if (pContext == 0 || digest == 0) {
		return SCE_SHA384_ERROR_INVALID_POINTER;
	}
	if (pContext->usComputed == 0) {
		union {
			SceUChar8 b[128];
			SceUInt32 ui[32];
			SceUInt64 ull[16];
		} blk;
		SceUInt64 h[8];
		SceUInt32 n;
		SceUInt32 i;

		n = pContext->usRemains;
		for (i = 0; i < 32; i++) {
			blk.ui[i] = 0;
		}
		for (i = 0; i < n; i++) {
			blk.b[i] = pContext->buf[i];
		}
		blk.b[n] = 0x80;
		for (from = (const SceUChar8 *)pContext->h, to = (SceUChar8 *)h;
		     to != (SceUChar8 *)(h + 8); ) {
			*to++ = *from++;
		}
		if (n >= 112) {
			_sceSha384SwapEndian(blk.ull, blk.ull, SCE_SHA384_BLOCK_SIZE);
			_sceSha384Core(blk.ull, h);
			for (i = 0; i < 32; i++) {
				blk.ui[i] = 0;
			}
		}
		for (i = 0; i < 16; i++) {
			if (i <= 7) {
				blk.b[112 + i] = 0;
			} else {
				blk.b[112 + i] = (SceUChar8)((pContext->ullTotalLen << 3) >> ((15 - i) * 8));
			}
		}
		_sceSha384SwapEndian(blk.ull, blk.ull, SCE_SHA384_BLOCK_SIZE);
		_sceSha384Core(blk.ull, h);
		_sceSha384SwapEndian(h, h, sizeof(h));
		for (from = (const SceUChar8 *)h, to = pContext->result;
		     from != (const SceUChar8 *)h + SCE_SHA384_DIGEST_SIZE; ) {
			*to++ = *from++;
		}
		pContext->usComputed = 1;
	}
	for (from = pContext->result, to = digest; to != digest + SCE_SHA384_DIGEST_SIZE; ) {
		*to++ = *from++;
	}
	return 0;
}

SceInt32 sceSha384Digest(const void *plain, SceUInt32 len, SceUChar8 *digest)
{
	SceSha384Context ctx;

	if (plain == 0 || digest == 0) {
		return SCE_SHA384_ERROR_INVALID_POINTER;
	}
	sceSha384BlockInit(&ctx);
	sceSha384BlockUpdate(&ctx, plain, len);
	sceSha384BlockResult(&ctx, digest);
	return 0;
}
