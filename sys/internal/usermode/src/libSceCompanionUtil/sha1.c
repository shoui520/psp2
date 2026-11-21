/*
 * SHA-1, the hash the session key's OAEP padding uses: one block at a time into a context
 * that counts the blocks, the last block padded with the message's length in bits.
 */
#include <scetypes.h>
#include "rsa_oaep.h"

#define SHA1_BLOCK_SIZE		64
#define SHA1_DIGEST_SIZE	20

#define ROL(x, n)	(((x) << (n)) | ((x) >> (32 - (n))))

typedef struct Sha1Context {
	SceUInt32 h[5];
	SceUInt32 blocksHigh;
	SceUInt32 blocks;
	int remains;
	SceUInt8 buffer[SHA1_BLOCK_SIZE];
} Sha1Context;

int Sha1Transform(SceUInt32 *h, const SceUInt8 *data)
{
	SceUInt32 w[80];
	SceUInt32 h0, h1, h2, h3, h4;
	SceUInt32 a, b, c, d, e, t;
	int i;

	h0 = h[0];
	h1 = h[1];
	h2 = h[2];
	h3 = h[3];
	h4 = h[4];

	for (i = 0; i < 16; i++) {
		w[i] = data[3] | data[2] << 8 | (data[1] << 16 | data[0] << 24);
		data += 4;
	}
	for (i = 16; i < 80; i++)
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	a = h0;
	b = h1;
	c = h2;
	d = h3;
	e = h4;
	for (i = 0; i < 80; i++) {
		if (i < 20)
			t = e + ROL(a, 5) + (d ^ (b & (c ^ d))) + w[i] + 0x5a827999;
		else if (i < 40)
			t = e + ROL(a, 5) + (b ^ c ^ d) + w[i] + 0x6ed9eba1;
		else if (i < 60)
			t = e + ROL(a, 5) + ((b & c) | (d & (b | c))) + w[i] + 0x8f1bbcdc;
		else
			t = e + ROL(a, 5) + (b ^ c ^ d) + w[i] + 0xca62c1d6;
		e = d;
		d = c;
		c = ROL(b, 30);
		b = a;
		a = t;
	}
	h[0] = h0 + a;
	h[1] = h1 + b;
	h[2] = h2 + c;
	h[3] = h3 + d;
	h[4] = h4 + e;
	return 0;
}

int Sha1Final(SceUInt8 *out, Sha1Context *ctx)
{
	SceUInt32 high, low;
	int index, i;

	index = ctx->remains;
	if (index >= SHA1_BLOCK_SIZE || index < 0)
		return -1;
	ctx->buffer[index] = 0x80;
	if (index > SHA1_BLOCK_SIZE - 9) {
		for (i = index + 1; i < SHA1_BLOCK_SIZE; i++)
			ctx->buffer[i] = 0;
		Sha1Transform(ctx->h, ctx->buffer);
		index = -1;
	}
	for (i = index + 1; i < SHA1_BLOCK_SIZE - 8; i++)
		ctx->buffer[i] = 0;

	/* the length in bits */
	high = ctx->blocksHigh << 9 | ctx->blocks >> 23;
	low = ctx->blocks << 9 | ctx->remains << 3;
	ctx->buffer[56] = high >> 24;
	ctx->buffer[57] = high >> 16;
	ctx->buffer[58] = high >> 8;
	ctx->buffer[59] = high;
	ctx->buffer[60] = low >> 24;
	ctx->buffer[61] = low >> 16;
	ctx->buffer[62] = low >> 8;
	ctx->buffer[63] = low;
	Sha1Transform(ctx->h, ctx->buffer);

	out[0] = ctx->h[0] >> 24;
	out[1] = ctx->h[0] >> 16;
	out[2] = ctx->h[0] >> 8;
	out[3] = ctx->h[0];
	out[4] = ctx->h[1] >> 24;
	out[5] = ctx->h[1] >> 16;
	out[6] = ctx->h[1] >> 8;
	out[7] = ctx->h[1];
	out[8] = ctx->h[2] >> 24;
	out[9] = ctx->h[2] >> 16;
	out[10] = ctx->h[2] >> 8;
	out[11] = ctx->h[2];
	out[12] = ctx->h[3] >> 24;
	out[13] = ctx->h[3] >> 16;
	out[14] = ctx->h[3] >> 8;
	out[15] = ctx->h[3];
	out[16] = ctx->h[4] >> 24;
	out[17] = ctx->h[4] >> 16;
	out[18] = ctx->h[4] >> 8;
	out[19] = ctx->h[4];
	return 0;
}

static inline void Sha1Update(Sha1Context *ctx, const SceUInt8 *data, int size)
{
	int i, n;

	if (ctx->remains + size >= SHA1_BLOCK_SIZE) {
		n = SHA1_BLOCK_SIZE - ctx->remains;
		for (i = 0; i < n; i++)
			ctx->buffer[ctx->remains + i] = data[i];
		data += n;
		Sha1Transform(ctx->h, ctx->buffer);
		if (++ctx->blocks == 0)
			ctx->blocksHigh++;
		size = size + ctx->remains - SHA1_BLOCK_SIZE;
		ctx->remains = 0;
		while (size >= SHA1_BLOCK_SIZE) {
			Sha1Transform(ctx->h, data);
			if (++ctx->blocks == 0)
				ctx->blocksHigh++;
			data += SHA1_BLOCK_SIZE;
			size -= SHA1_BLOCK_SIZE;
			ctx->remains = 0;
		}
	}
	for (i = 0; i < size; i++)
		ctx->buffer[ctx->remains + i] = data[i];
	ctx->remains += size;
}

int Sha1Digest(unsigned char *out, const unsigned char *data, int size)
{
	Sha1Context ctx;

	if (size < 0)
		return -2;
	ctx.h[0] = 0x67452301;
	ctx.h[1] = 0xefcdab89;
	ctx.h[2] = 0x98badcfe;
	ctx.h[3] = 0x10325476;
	ctx.h[4] = 0xc3d2e1f0;
	ctx.blocksHigh = 0;
	ctx.blocks = 0;
	ctx.remains = 0;
	Sha1Update(&ctx, data, size);
	Sha1Final(out, &ctx);
	return 0;
}
