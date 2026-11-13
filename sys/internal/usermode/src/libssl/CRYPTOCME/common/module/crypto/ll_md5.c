/* Filename and descriptive names are ours: CRYPTOCME's MD5 (RFC 1321)
 * compression function, "md5 C small", below the little-endian MD block
 * buffering.
 */
#include "ll_obj.h"

typedef struct SslMd5State {
	SceUInt32 value[4];
	const SceUInt32 *table;
} SslMd5State;

#define SSL_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define SSL_MD5_F(b, c, d) ((((c) ^ (d)) & (b)) ^ (d))
#define SSL_MD5_G(b, c, d) ((((b) ^ (c)) & (d)) ^ (c))
#define SSL_MD5_H(b, c, d) ((b) ^ (c) ^ (d))
#define SSL_MD5_I(b, c, d) (((b) | ~(d)) ^ (c))

#define SSL_MD5_STEP(f, a, b, c, d, x, t, s)          \
	do {                                           \
		(a) += (x) + (t) + f((b), (c), (d));   \
		(a) = SSL_ROTL((a), (s)) + (b);        \
	} while (0)

/* T[i] = floor(abs(sin(i + 1)) * 2^32) */
static const SceUInt32 sslMd5Table[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static int sslOutputMd5(void *state, unsigned char *digest)
{
	SceUInt32 *value = state;
	int i;

	for (i = 0; i < 4; i++) {
		SceUInt32 v = *value++;

		digest[0] = v;
		digest[1] = v >> 8;
		digest[2] = v >> 16;
		digest[3] = v >> 24;
		digest += 4;
	}
	return 0;
}

static int sslInitMd5(void *state)
{
	SslMd5State *md5 = state;

	md5->value[0] = 0x67452301;
	md5->value[1] = 0xefcdab89;
	md5->value[2] = 0x98badcfe;
	md5->value[3] = 0x10325476;
	md5->table = sslMd5Table;
	return 0;
}

static int sslCompressMd5(void *state, const void *data, SceUInt32 length);

static const SslDigestCore sslMd5Core = {
	sslInitMd5,
	sslCompressMd5,
	sslOutputMd5,
	NULL,
	0,
	sizeof(SslMd5State),
	0, 4, 4, 4,
	NULL,
	0
};

/* rsadsi 2 5 */
static const unsigned char sslMd5Oid[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05
};

static const SslDigestDescriptor sslMd5DigestDescriptor = {
	"md5 C small",
	{ sizeof(sslMd5Oid), sslMd5Oid },
	16, 0, 64,
	&sslMdLittleEndianImplementation,
	&sslMd5Core,
	NULL,
	1,
	1,
	0
};

const SslDigestDescriptor *sslGetMd5SmallDigestDescriptor(void)
{
	return &sslMd5DigestDescriptor;
}

const SslDigestDescriptor *sslGetMd5DigestDescriptor(void)
{
	return &sslMd5DigestDescriptor;
}

static int sslCompressMd5(void *state, const void *data, SceUInt32 length)
{
	SslMd5State *md5 = state;
	const SceUInt32 *x = data;
	const SceUInt32 *t;
	SceUInt32 a = md5->value[0];
	SceUInt32 b = md5->value[1];
	SceUInt32 c = md5->value[2];
	SceUInt32 d = md5->value[3];
	const SceUInt32 *table = md5->table;
	SceUInt32 aa, bb, cc, dd;
	const SceUInt32 *next = x + 16;
	SceUInt32 blocks = (length >> 6) - 1;
	int i, k;

	for (;;) {
		aa = a;
		bb = b;
		cc = c;
		dd = d;
		t = table;
		for (i = 0; i < 16; i += 4) {
			SSL_MD5_STEP(SSL_MD5_F, a, b, c, d, x[i], t[0], 7);
			SSL_MD5_STEP(SSL_MD5_F, d, a, b, c, x[i + 1], t[1], 12);
			SSL_MD5_STEP(SSL_MD5_F, c, d, a, b, x[i + 2], t[2], 17);
			SSL_MD5_STEP(SSL_MD5_F, b, c, d, a, x[i + 3], t[3], 22);
			t += 4;
		}
		k = 1;
		for (i = 0; i < 4; i++) {
			SSL_MD5_STEP(SSL_MD5_G, a, b, c, d, x[k], t[0], 5);
			k = (k + 5) & 15;
			SSL_MD5_STEP(SSL_MD5_G, d, a, b, c, x[k], t[1], 9);
			k = (k + 5) & 15;
			SSL_MD5_STEP(SSL_MD5_G, c, d, a, b, x[k], t[2], 14);
			k = (k + 5) & 15;
			SSL_MD5_STEP(SSL_MD5_G, b, c, d, a, x[k], t[3], 20);
			k = (k + 5) & 15;
			t += 4;
		}
		k = 5;
		for (i = 0; i < 4; i++) {
			SSL_MD5_STEP(SSL_MD5_H, a, b, c, d, x[k], t[0], 4);
			k = (k + 3) & 15;
			SSL_MD5_STEP(SSL_MD5_H, d, a, b, c, x[k], t[1], 11);
			k = (k + 3) & 15;
			SSL_MD5_STEP(SSL_MD5_H, c, d, a, b, x[k], t[2], 16);
			k = (k + 3) & 15;
			SSL_MD5_STEP(SSL_MD5_H, b, c, d, a, x[k], t[3], 23);
			k = (k + 3) & 15;
			t += 4;
		}
		k = 0;
		for (i = 0; i < 4; i++) {
			SSL_MD5_STEP(SSL_MD5_I, a, b, c, d, x[k], t[0], 6);
			k = (k + 7) & 15;
			SSL_MD5_STEP(SSL_MD5_I, d, a, b, c, x[k], t[1], 10);
			k = (k + 7) & 15;
			SSL_MD5_STEP(SSL_MD5_I, c, d, a, b, x[k], t[2], 15);
			k = (k + 7) & 15;
			SSL_MD5_STEP(SSL_MD5_I, b, c, d, a, x[k], t[3], 21);
			k = (k + 7) & 15;
			t += 4;
		}
		a += aa;
		b += bb;
		c += cc;
		d += dd;
		md5->value[0] = a;
		md5->value[1] = b;
		md5->value[2] = c;
		md5->value[3] = d;
		x = next;
		if (blocks == 0)
			break;
		next += 16;
		blocks--;
	}
	return 0;
}
