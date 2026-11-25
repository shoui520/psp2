#include "upnp_int.h"

static const u32 g_sha1_iv[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
};

static void sha1Compress(u32 *w, u32 *state);

static s32 sha1Update(Sha1Ctx *ctx, const u8 *data, u32 len);
static s32 sha1Final(Sha1Ctx *ctx, u8 *out_digest);
static s32 sha1Init(Sha1Ctx *ctx);

/* =========================================================================
 * SHA-1 core (0x0000..0x047c)
 * ========================================================================= */

static void sha1TransformBlock(u32 *w, u32 *state)
{
	sha1Compress(w, state);
}

static void sha1Compress(u32 *w, u32 *state)
{
	u32 a = state[0];
	u32 b = state[1];
	u32 c = state[2];
	u32 d = state[3];
	u32 e = state[4];
	u32 i;

	for (i = 0; i < 80; i++) {
		u32 wi;
		u32 idx = i & 15;
		if (i < 16) {
			wi = w[idx];
		} else {
			wi = w[idx] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^ w[(i + 13) & 15];
			wi = rotr32(wi, 31);
			w[idx] = wi;
		}
		u32 f, k;
		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5a827999u;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1u;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdcu;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6u;
		}
		u32 temp = rotr32(a, 27) + f + e + k + wi;
		e = d;
		d = c;
		c = rotr32(b, 2);
		b = a;
		a = temp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

static s32 sha1ComputeDigest(const u8 *data, u32 len, u8 *out_digest)
{
	u32 state[5];
	u32 w[16];
	u32 bitlen = len << 3;
	u32 i;

	state[0] = g_sha1_iv[0];
	state[1] = g_sha1_iv[1];
	state[2] = g_sha1_iv[2];
	state[3] = g_sha1_iv[3];
	state[4] = g_sha1_iv[4];

	while (len >= 64) {
		for (i = 0; i < 16; i++) {
			u32 v = ((u32)data[0] << 24) | ((u32)data[1] << 16) |
			        ((u32)data[2] << 8) | (u32)data[3];
			w[i] = v;
			data += 4;
		}
		sha1Compress(w, state);
		len -= 64;
	}

	for (i = 0; i < 16; i++)
		w[i] = 0;

	if (len >= 56) {
		u8 *wb = (u8 *)w;
		for (i = 0; i < len; i++)
			wb[i] = data[i];
		wb[len] = 0x80;
		for (i = 0; i < 16; i++)
			w[i] = bswap32(w[i]);
		sha1Compress(w, state);
		for (i = 0; i < 16; i++)
			w[i] = 0;
		w[15] = bitlen;
		sha1Compress(w, state);
	} else {
		u8 *wb = (u8 *)w;
		for (i = 0; i < len; i++)
			wb[i] = data[i];
		wb[len] = 0x80;
		for (i = 0; i < 14; i++)
			w[i] = bswap32(w[i]);
		w[15] = bitlen;
		sha1Compress(w, state);
	}

	for (i = 0; i < 5; i++) {
		u32 v = state[i];
		out_digest[i * 4 + 0] = (u8)(v >> 24);
		out_digest[i * 4 + 1] = (u8)(v >> 16);
		out_digest[i * 4 + 2] = (u8)(v >> 8);
		out_digest[i * 4 + 3] = (u8)(v);
	}
	return 0;
}


/* =========================================================================
 * HMAC-SHA1 (0x5cc8..0x5e47)
 * ========================================================================= */

void hmacSha1Compute(const u8 *data, s32 data_len, const u8 *key, s32 key_len, u8 *out_digest)
{
	Sha1Ctx ctx;
	u8 k_ipad[0x41];
	u8 k_opad[0x41];
	u8 tk[0x14];
	Sha1Ctx tctx;
	s32 i;

	if (key_len > 64) {
		sha1Init(&tctx);
		sha1Update(&tctx, key, (u32)key_len);
		sha1Final(&tctx, tk);
		key = tk;
		key_len = 20;
	}

	sceKernelMemset(k_ipad, 0, sizeof(k_ipad));
	sceKernelMemset(k_opad, 0, sizeof(k_opad));
	sceKernelMemcpy(k_ipad, key, (u32)key_len);
	sceKernelMemcpy(k_opad, key, (u32)key_len);

	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	sha1Init(&ctx);
	sha1Update(&ctx, k_ipad, 64);
	sha1Update(&ctx, data, (u32)data_len);
	sha1Final(&ctx, out_digest);

	sha1Init(&ctx);
	sha1Update(&ctx, k_opad, 64);
	sha1Update(&ctx, out_digest, 20);
	sha1Final(&ctx, out_digest);
}


/* =========================================================================
 * Streaming SHA-1 (0x85f0..0x8a24)
 * ========================================================================= */

static s32 sha1Update(Sha1Ctx *ctx, const u8 *data, u32 len)
{
	u32 block[16];
	u32 rem;
	u32 i;

	if (ctx == NULL || data == NULL)
		return (s32)0x80000103;
	if (ctx->finalized != 0)
		return (s32)0x80000021;

	ctx->total_bytes += len;
	rem = (u32)ctx->buf_len + len;

	while (rem >= 64) {
		u8 *bb = (u8 *)block;
		u32 cur = ctx->buf_len;
		if (cur != 0) {
			u32 fill = 64 - cur;
			for (i = 0; i < cur; i++)
				bb[i] = ctx->buf[i];
			for (i = 0; i < fill; i++)
				bb[cur + i] = data[i];
			data += fill;
			ctx->buf_len = 0;
		} else {
			for (i = 0; i < 64; i++)
				bb[i] = data[i];
			data += 64;
		}
		for (i = 0; i < 16; i++)
			block[i] = bswap32(block[i]);
		sha1TransformBlock(block, ctx->state);
		rem -= 64;
	}

	if (rem != 0) {
		u32 cur = ctx->buf_len;
		for (i = 0; i < rem - cur; i++)
			ctx->buf[cur + i] = data[i];
	}
	ctx->buf_len = (u16)rem;
	return 0;
}

static s32 sha1Final(Sha1Ctx *ctx, u8 *out_digest)
{
	u32 block[16];
	u32 i;

	if (ctx == NULL || out_digest == NULL)
		return (s32)0x80000103;

	if (ctx->finalized == 0) {
		u32 cur = ctx->buf_len;
		u8 *bb = (u8 *)block;
		for (i = 0; i < 16; i++)
			block[i] = 0;
		for (i = 0; i < cur; i++)
			bb[i] = ctx->buf[i];
		bb[cur] = 0x80;

		if (cur >= 56) {
			for (i = 0; i < 16; i++)
				block[i] = bswap32(block[i]);
			sha1TransformBlock(block, ctx->state);
			for (i = 0; i < 16; i++)
				block[i] = 0;
		}

		u64 bitlen = ctx->total_bytes << 3;
		for (i = 0; i < 8; i++)
			bb[56 + i] = (u8)(bitlen >> (56 - i * 8));

		for (i = 0; i < 16; i++)
			block[i] = bswap32(block[i]);
		sha1TransformBlock(block, ctx->state);

		for (i = 0; i < 5; i++)
			ctx->state[i] = bswap32(ctx->state[i]);
		ctx->finalized = 1;
		for (i = 0; i < 64; i++)
			ctx->buf[i] = 0;
	}

	const u8 *st = (const u8 *)ctx->state;
	for (i = 0; i < 20; i++)
		out_digest[i] = st[i];
	return 0;
}

static s32 sha1Init(Sha1Ctx *ctx)
{
	if (ctx == NULL)
		return (s32)0x80000103;
	ctx->state[0] = 0x67452301u;
	ctx->state[1] = 0xefcdab89u;
	ctx->state[2] = 0x98badcfeu;
	ctx->state[3] = 0x10325476u;
	ctx->state[4] = 0xc3d2e1f0u;
	ctx->buf_len = 0;
	ctx->finalized = 0;
	ctx->total_bytes = 0;
	return 0;
}
