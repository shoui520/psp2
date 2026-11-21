/*
 * SHA-1, laid out as Steve Reid's public-domain sha1.c: the block's words are expanded in
 * place, so the transform overwrites the block it hashes. The update copies every block
 * through the context's buffer. The file's name is ours.
 */
#include "mln_internal.h"

#define rol(value, bits)	(((value) << (bits)) | ((value) >> (32 - (bits))))

typedef union {
	unsigned char c[64];
	unsigned int l[16];
} CHAR64LONG16;

/* the block's words, big-endian */
#define blk0(i)	(block->l[i] = (rol(block->l[i], 24) & 0xFF00FF00) | \
			       (rol(block->l[i], 8) & 0x00FF00FF))
#define blk(i)	(block->l[(i) & 15] = rol(block->l[((i) + 13) & 15] ^ block->l[((i) + 8) & 15] ^ \
					  block->l[((i) + 2) & 15] ^ block->l[(i) & 15], 1))

/* the four rounds */
#define R0(v, w, x, y, z, i) \
	z += ((w & (x ^ y)) ^ y) + blk0(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R1(v, w, x, y, z, i) \
	z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R2(v, w, x, y, z, i) \
	z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30);
#define R3(v, w, x, y, z, i) \
	z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30);
#define R4(v, w, x, y, z, i) \
	z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30);

/* one 64-byte block into the state */
void MlnImpl_81032404(unsigned int state[5], unsigned char buffer[64])
{
	CHAR64LONG16 *block = (CHAR64LONG16 *)buffer;
	unsigned int a, b, c, d, e;

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	R0(a, b, c, d, e, 0); R0(e, a, b, c, d, 1); R0(d, e, a, b, c, 2); R0(c, d, e, a, b, 3);
	R0(b, c, d, e, a, 4); R0(a, b, c, d, e, 5); R0(e, a, b, c, d, 6); R0(d, e, a, b, c, 7);
	R0(c, d, e, a, b, 8); R0(b, c, d, e, a, 9); R0(a, b, c, d, e, 10); R0(e, a, b, c, d, 11);
	R0(d, e, a, b, c, 12); R0(c, d, e, a, b, 13); R0(b, c, d, e, a, 14); R0(a, b, c, d, e, 15);
	R1(e, a, b, c, d, 16); R1(d, e, a, b, c, 17); R1(c, d, e, a, b, 18); R1(b, c, d, e, a, 19);
	R2(a, b, c, d, e, 20); R2(e, a, b, c, d, 21); R2(d, e, a, b, c, 22); R2(c, d, e, a, b, 23);
	R2(b, c, d, e, a, 24); R2(a, b, c, d, e, 25); R2(e, a, b, c, d, 26); R2(d, e, a, b, c, 27);
	R2(c, d, e, a, b, 28); R2(b, c, d, e, a, 29); R2(a, b, c, d, e, 30); R2(e, a, b, c, d, 31);
	R2(d, e, a, b, c, 32); R2(c, d, e, a, b, 33); R2(b, c, d, e, a, 34); R2(a, b, c, d, e, 35);
	R2(e, a, b, c, d, 36); R2(d, e, a, b, c, 37); R2(c, d, e, a, b, 38); R2(b, c, d, e, a, 39);
	R3(a, b, c, d, e, 40); R3(e, a, b, c, d, 41); R3(d, e, a, b, c, 42); R3(c, d, e, a, b, 43);
	R3(b, c, d, e, a, 44); R3(a, b, c, d, e, 45); R3(e, a, b, c, d, 46); R3(d, e, a, b, c, 47);
	R3(c, d, e, a, b, 48); R3(b, c, d, e, a, 49); R3(a, b, c, d, e, 50); R3(e, a, b, c, d, 51);
	R3(d, e, a, b, c, 52); R3(c, d, e, a, b, 53); R3(b, c, d, e, a, 54); R3(a, b, c, d, e, 55);
	R3(e, a, b, c, d, 56); R3(d, e, a, b, c, 57); R3(c, d, e, a, b, 58); R3(b, c, d, e, a, 59);
	R4(a, b, c, d, e, 60); R4(e, a, b, c, d, 61); R4(d, e, a, b, c, 62); R4(c, d, e, a, b, 63);
	R4(b, c, d, e, a, 64); R4(a, b, c, d, e, 65); R4(e, a, b, c, d, 66); R4(d, e, a, b, c, 67);
	R4(c, d, e, a, b, 68); R4(b, c, d, e, a, 69); R4(a, b, c, d, e, 70); R4(e, a, b, c, d, 71);
	R4(d, e, a, b, c, 72); R4(c, d, e, a, b, 73); R4(b, c, d, e, a, 74); R4(a, b, c, d, e, 75);
	R4(e, a, b, c, d, 76); R4(d, e, a, b, c, 77); R4(c, d, e, a, b, 78); R4(b, c, d, e, a, 79);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

void MlnImpl_81033774(MlnSha1Context *context)
{
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
	context->state[4] = 0xC3D2E1F0;
	context->count[0] = context->count[1] = 0;
}

/* the bytes into the context, a block at a time through its buffer */
static inline __attribute__((always_inline)) void
MlnSha1Update(MlnSha1Context *context, const unsigned char *data, unsigned int size)
{
	unsigned int len = size;
	unsigned int j, n, left;

	j = (context->count[0] >> 3) & 63;
	n = j + len;
	if (n > 63) {
		left = n;
		do {
			MlnMemcpy(&context->buffer[j], data, 64 - j);
			data += 64 - j;
			left -= 64;
			MlnImpl_81032404(context->state, context->buffer);
			j = 0;
		} while (left > 63);
		len = n & 63;
	}
	MlnMemcpy(&context->buffer[j], data, len);
	if ((context->count[0] += size << 3) < (size << 3))
		context->count[1]++;
	context->count[1] += size >> 29;
}

void MlnImpl_810337C4(MlnSha1Context *context, const void *data, unsigned int size)
{
	MlnSha1Update(context, data, size);
}

/* the state's words, big-endian */
#define SHA1_PUT32(p, v)				\
	do {						\
		(p)[0] = (unsigned char)((v) >> 24);	\
		(p)[1] = (unsigned char)((v) >> 16);	\
		(p)[2] = (unsigned char)((v) >> 8);	\
		(p)[3] = (unsigned char)(v);		\
	} while (0)

void MlnImpl_81033864(MlnSha1Context *context, unsigned char *digest)
{
	unsigned char finalcount[8];

	SHA1_PUT32(&finalcount[0], context->count[1]);
	SHA1_PUT32(&finalcount[4], context->count[0]);
	MlnSha1Update(context, (const unsigned char *)"\200", 1);
	while ((context->count[0] & 504) != 448)
		MlnSha1Update(context, (const unsigned char *)"\0", 1);
	MlnSha1Update(context, finalcount, 8);
	SHA1_PUT32(&digest[0], context->state[0]);
	SHA1_PUT32(&digest[4], context->state[1]);
	SHA1_PUT32(&digest[8], context->state[2]);
	SHA1_PUT32(&digest[12], context->state[3]);
	SHA1_PUT32(&digest[16], context->state[4]);
}
