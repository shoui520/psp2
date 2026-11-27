/*
 * sw_prng.c — SHA-1 compression/digest and FIPS 186-2 Appendix 3.1 PRNG
 * used by DTCPIP-SW.
 */
#include "sw_ecc.h"

#define rotl32(x, n) (((uint32_t)(x) << (n)) | ((uint32_t)(x) >> (32u - (n))))

#define readBe32(p) ( \
	((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | \
	((uint32_t)(p)[2] << 8) | (uint32_t)(p)[3])

#define writeBe32(p, v) do { \
	unsigned char *_wbp = (p); \
	uint32_t _wbv = (v); \
	_wbp[0] = (unsigned char)(_wbv >> 24); \
	_wbp[1] = (unsigned char)(_wbv >> 16); \
	_wbp[2] = (unsigned char)(_wbv >> 8); \
	_wbp[3] = (unsigned char)_wbv; \
} while (0)

/* SHA-1 single 64-byte block compression */
int swSha1Compress(uint32_t state[5], const unsigned char block[64])
{
	uint32_t w[80];
	uint32_t a, b, c, d, e;
	int i;

	for (i = 0; i < 16; ++i)
		w[i] = readBe32(block + 4 * i);
	for (i = 16; i < 80; ++i)
		w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1u);

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	for (i = 0; i < 80; ++i) {
		uint32_t f, k, tmp;

		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5A827999u;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ED9EBA1u;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDCu;
		} else {
			f = b ^ c ^ d;
			k = 0xCA62C1D6u;
		}
		tmp = rotl32(a, 5u) + f + e + k + w[i];
		e = d;
		d = c;
		c = rotl32(b, 30u);
		b = a;
		a = tmp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	return 0;
}

/* finalize SHA-1 context with standard padding and 64-bit bit count */
int swSha1Final(unsigned char out[20], struct SwSha1Ctx *ctx)
{
	uint32_t buffered = ctx->buffered;
	int pos = (int)buffered;
	uint32_t bitsHi, bitsLo;
	int i;

	if (buffered >= 64u)
		return -1;

	ctx->buf[pos] = 0x80u;
	if (pos > 55) {
		for (i = pos + 1; i < 64; ++i)
			ctx->buf[i] = 0;
		swSha1Compress(ctx->state, ctx->buf);
		buffered = ctx->buffered;
		pos = -1;
	}
	for (i = pos + 1; i < 56; ++i)
		ctx->buf[i] = 0;

	bitsHi = (ctx->blocksHi << 9) | (ctx->blocksLo >> 23);
	bitsLo = (ctx->blocksLo << 9) | (buffered << 3);
	writeBe32(ctx->buf + 56, bitsHi);
	writeBe32(ctx->buf + 60, bitsLo);
	swSha1Compress(ctx->state, ctx->buf);

	for (i = 0; i < 5; ++i)
		writeBe32(out + 4 * i, ctx->state[i]);
	return 0;
}

/* one-shot SHA-1 digest */
int swSha1Digest(unsigned char out[20], const unsigned char *data, int len)
{
	struct SwSha1Ctx ctx;
	const unsigned char *p = data;
	int rem = len;
	int i;

	if (len < 0)
		return -2;

	ctx.state[0] = 0x67452301u;
	ctx.state[1] = 0xEFCDAB89u;
	ctx.state[2] = 0x98BADCFEu;
	ctx.state[3] = 0x10325476u;
	ctx.state[4] = 0xC3D2E1F0u;
	ctx.blocksHi = 0;
	ctx.blocksLo = 0;
	ctx.buffered = 0;

	if (rem >= 64) {
		for (i = 0; i < 64; ++i)
			ctx.buf[i] = p[i];
		p += 64;
		swSha1Compress(ctx.state, ctx.buf);
		if (++ctx.blocksLo == 0)
			++ctx.blocksHi;
		rem -= 64;
		ctx.buffered = 0;
	}
	while (rem >= 64) {
		swSha1Compress(ctx.state, p);
		if (++ctx.blocksLo == 0)
			++ctx.blocksHi;
		p += 64;
		rem -= 64;
		ctx.buffered = 0;
	}
	for (i = 0; i < rem; ++i)
		ctx.buf[i] = p[i];
	ctx.buffered = (uint32_t)rem;
	swSha1Final(out, &ctx);
	return 0;
}

/* 160-bit big-endian word addition modulo 2^160 */
uint32_t *swPrngAddMod2_160(const uint32_t a[5], const uint32_t b[5], uint32_t out[5])
{
	uint32_t carry = 0;
	int i;

	for (i = 4; i >= 0; --i) {
		uint32_t maxv = (a[i] > b[i]) ? a[i] : b[i];
		uint32_t sum = a[i] + b[i] + carry;

		out[i] = sum;
		if (maxv > sum || (a[i] == 0xFFFFFFFFu && b[i] == 0xFFFFFFFFu))
			carry = 1;
		else
			carry = 0;
	}
	return (uint32_t *)(uintptr_t)(b - 1);
}

/* FIPS 186-2 Appendix 3.1 SHA-1 PRNG step */
int swPrngFips186(unsigned char out[20], unsigned char xkey[20], const unsigned char xseed[20])
{
	uint32_t keyWords[5];
	uint32_t seedWords[5];
	uint32_t sumWords[5];
	unsigned char block[64];
	struct SwSha1Ctx ctx;
	int i;

	for (i = 0; i < 5; ++i) {
		keyWords[i] = readBe32(xkey + 4 * i);
		seedWords[i] = readBe32(xseed + 4 * i);
	}

	swPrngAddMod2_160(keyWords, seedWords, sumWords);
	for (i = 0; i < 5; ++i)
		writeBe32(block + 4 * i, sumWords[i]);
	for (i = 20; i < 64; ++i)
		block[i] = 0;

	ctx.state[0] = 0x67452301u;
	ctx.state[1] = 0xEFCDAB89u;
	ctx.state[2] = 0x98BADCFEu;
	ctx.state[3] = 0x10325476u;
	ctx.state[4] = 0xC3D2E1F0u;
	ctx.blocksHi = 0;
	ctx.blocksLo = 0;
	ctx.buffered = 0;

	swSha1Compress(ctx.state, block);
	for (i = 0; i < 5; ++i) {
		writeBe32(out + 4 * i, ctx.state[i]);
		seedWords[i] = ctx.state[i];
	}

	swPrngAddMod2_160(keyWords, seedWords, sumWords);
	seedWords[0] = 0;
	seedWords[1] = 0;
	seedWords[2] = 0;
	seedWords[3] = 0;
	seedWords[4] = 1;
	swPrngAddMod2_160(sumWords, seedWords, keyWords);

	for (i = 0; i < 5; ++i)
		writeBe32(xkey + 4 * i, keyWords[i]);
	return 0;
}
