/* Filename and descriptive names are ours: CRYPTOCME's SHA-1 (FIPS 180-1)
 * compression function, "sha1 C small16" (a 16-word message schedule),
 * below the big-endian MD block buffering.
 */
#include "ll_obj.h"

#define SSL_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define SSL_SHA1_CH(b, c, d) ((((c) ^ (d)) & (b)) ^ (d))
#define SSL_SHA1_PARITY(b, c, d) ((b) ^ (c) ^ (d))
#define SSL_SHA1_MAJ(b, c, d) ((((b) | (c)) & (d)) | ((b) & (c)))

/* Only SHA-1's five chaining values can be exported */
#define SSL_SHA1_ERROR_VALUE_COUNT 10038

static int sslOutputSha1(void *state, unsigned char *digest)
{
	SceUInt32 *value = state;
	int ret = 0;
	int i;

	for (i = 5; i != 0; i--) {
		SceUInt32 v = *value++;

		digest[0] = v >> 24;
		digest[1] = v >> 16;
		digest[2] = v >> 8;
		digest[3] = v;
		digest += 4;
	}
	return ret;
}

static int sslInitSha1(void *state)
{
	SceUInt32 *value = state;

	value[0] = 0x67452301;
	value[1] = 0xefcdab89;
	value[2] = 0x98badcfe;
	value[3] = 0x10325476;
	value[4] = 0xc3d2e1f0;
	return 0;
}

static int sslCompressSha1(void *state, const void *data, SceUInt32 length);

static const SslDigestCore sslSha1Core = {
	sslInitSha1,
	sslCompressSha1,
	sslOutputSha1,
	NULL,
	0,
	20,
	0, 5, 4, 4,
	NULL,
	0
};

/* oiw secsig algorithms 26 */
static const unsigned char sslSha1Oid[] = {
	0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a
};

static const SslDigestDescriptor sslSha1DigestDescriptor = {
	"sha1 C small16",
	{ sizeof(sslSha1Oid), sslSha1Oid },
	20, 0, 64,
	&sslMdBigEndianImplementation,
	&sslSha1Core,
	NULL,
	2,
	1,
	0
};

const SslDigestDescriptor *sslGetSha1SmallDigestDescriptor(void)
{
	return &sslSha1DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha1DigestDescriptor(void)
{
	return &sslSha1DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha1Small16DigestDescriptor(void)
{
	return &sslSha1DigestDescriptor;
}

/* The chaining values, big-endian (the FIPS 186-2 generator's output) */
int sslGetSha1DigestValues(SslDigestContext *context, unsigned char *output)
{
	const SslDigestCore *core = context->descriptor->innerImplementation;
	const SceUInt32 *value = (const SceUInt32 *)
		((unsigned char *)context->innerState + core->valueOffset);
	int skip;
	int i;

	if (core->valueCount != 5)
		return SSL_SHA1_ERROR_VALUE_COUNT;
	skip = (core->valueSize - core->reserved2) / 4;
	for (i = 0; i < 5; i++) {
		output[3] = value[i];
		output[2] = value[i] >> 8;
		output[1] = value[i] >> 16;
		output[0] = value[i] >> 24;
		value += skip;
		output += 4;
	}
	return 0;
}

static int sslCompressSha1(void *state, const void *data, SceUInt32 length)
{
	SceUInt32 *value = state;
	const SceUInt32 *in = data;
	SceUInt32 w[16];
	SceUInt32 a, b, c, d, e, t, x;
	SceUInt32 aa, bb, cc, dd, ee;
	SceUInt32 blocks = (length >> 6) - 1;
	int ret;
	int i;

	a = value[0];
	b = value[1];
	c = value[2];
	d = value[3];
	e = value[4];
	for (;;) {
		aa = a;
		bb = b;
		cc = c;
		dd = d;
		ee = e;
		ret = 0;
		for (i = 0; i < 16; i++) {
			x = in[i];
			w[i] = ((x >> 24) | (x << 24)) |
			       ((((x >> 16) & 0xff) << 8) |
				(((x >> 8) & 0xff) << 16));
		}
		for (i = 0; i < 16; i++) {
			t = w[i] + e + SSL_ROTL(a, 5) + SSL_SHA1_CH(b, c, d) +
			    0x5a827999;
			e = d;
			d = c;
			c = SSL_ROTL(b, 30);
			b = a;
			a = t;
		}
		for (i = 16; i < 20; i++) {
			x = w[i & 15] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^
			    w[(i + 13) & 15];
			w[i & 15] = x = SSL_ROTL(x, 1);
			t = e + SSL_ROTL(a, 5) + SSL_SHA1_CH(b, c, d) + x +
			    0x5a827999;
			e = d;
			d = c;
			c = SSL_ROTL(b, 30);
			b = a;
			a = t;
		}
		for (i = 20; i < 40; i++) {
			x = w[i & 15] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^
			    w[(i + 13) & 15];
			w[i & 15] = x = SSL_ROTL(x, 1);
			t = e + SSL_ROTL(a, 5) + SSL_SHA1_PARITY(b, c, d) + x +
			    0x6ed9eba1;
			e = d;
			d = c;
			c = SSL_ROTL(b, 30);
			b = a;
			a = t;
		}
		for (i = 40; i < 60; i++) {
			x = w[i & 15] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^
			    w[(i + 13) & 15];
			w[i & 15] = x = SSL_ROTL(x, 1);
			t = e + SSL_ROTL(a, 5) + SSL_SHA1_MAJ(b, c, d) + x +
			    0x8f1bbcdc;
			e = d;
			d = c;
			c = SSL_ROTL(b, 30);
			b = a;
			a = t;
		}
		for (i = 60; i < 80; i++) {
			x = w[i & 15] ^ w[(i + 2) & 15] ^ w[(i + 8) & 15] ^
			    w[(i + 13) & 15];
			w[i & 15] = x = SSL_ROTL(x, 1);
			t = e + SSL_ROTL(a, 5) + SSL_SHA1_PARITY(b, c, d) + x +
			    0xca62c1d6;
			e = d;
			d = c;
			c = SSL_ROTL(b, 30);
			b = a;
			a = t;
		}
		a += aa;
		b += bb;
		c += cc;
		d += dd;
		e += ee;
		value[0] = a;
		value[1] = b;
		value[2] = c;
		value[3] = d;
		value[4] = e;
		if (blocks == 0)
			break;
		in += 16;
		blocks--;
	}
	return ret;
}
